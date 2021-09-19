/*
 * Copyright (c) 2009-2012, Salvatore Sanfilippo <antirez at gmail dot com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef _WIN32
#include <sys/timeb.h>
#else
#include <sys/time.h>
#endif

#include "ae.h"
#include "adlist.h"
#include "anet.h"
#include "elog.h"
#include "zmalloc.h"
#include "sds.h"
#include "locks.h"
#include "dict.h"
#include "dicthelp.h"
#include "networking.h"
#include "lua_cmsgpack.h"
#include "proto.h"
#include "docker.h"
#include "entityid.h"
#include "redishelp.h"

#ifdef _WIN32
#include "Win32_FDAPI.h"
#include "win32_wsiocp2.h"
#include "Win32_error.h"
#include "WSPiapi.h"
#endif

#include <math.h>
#include <pthread.h>
#include "cJSON.h"
#include "netex.h"
#include "dict.h"

WIN32_ONLY(extern int WSIOCP_QueueAccept(int listenfd);)

#define ANET_ERR_LEN 256
#define BINDADDR_MAX 16
#define MIN_RESERVED_FDS 32
#define EVENTLOOP_FDSET_INCR (MIN_RESERVED_FDS+96)
#define _ERR -1
#define _OK 0
#define _BINDADDR_MAX 16
#define MAX_ACCEPTS_PER_CALL 1000
#define _IP_STR_LEN 46 /* INET6_ADDRSTRLEN is 46, but we need to be sure */

/* Client flags */
#define _CLOSE_AFTER_REPLY (1<<0) /* Close after writing entire reply. */
#define _CLOSE_ASAP (1<<1)/* Close this client ASAP */
#define _UNIX_SOCKET (1<<2) /* Client connected via Unix domain socket */
#define _UDP_SOCKET (1<<3)      /* Client is in Pub/Sub mode. */
#define _TCP_SOCKET (1<<4)
#define _TCP_CONNECT (1<<5)

#define _REPLY_CHUNK_BYTES (16*1024) /* 16k output buffer */
#define _MAX_WRITE_PER_EVENT (1024*64)
#define _IOBUF_LEN         (1024*16)  /* Generic I/O buffer size */

                             /* Anti-warning macro... */
#define _NOTUSED(V) ((void) V)
#define PACKET_MAX_SIZE_UDP					1472

typedef struct _NetClient {
    int fd;
    int flags;              /* REDIS_SLAVE | REDIS_MONITOR | REDIS_MULTI ... */

    unsigned short recv_size;//预计接受的封包数量
    unsigned short recv_pos;//实际接受的封包数量
    sds recv_buf;//缓冲区
    char recv_status;

    void* private_data; //保留数据
    list* sendBuf;

    //udp
    char recvBuf[PACKET_MAX_SIZE_UDP];

    unsigned int id;//client的id
    unsigned long long entityId;//对应entiy的id
}*PNetClient, NetClient;

typedef struct _NetServer {
	aeEventLoop* el;
    int ipfd[BINDADDR_MAX]; /* TCP socket file descriptors */
    int ipfd_count;             /* Used slots in ipfd[] */
    int udpfd[BINDADDR_MAX]; /* UDP socket file descriptors */
    int udpfd_count;             /* Used slots in ipfd[] */
    unsigned int udp_ip;/* 最终监听的ip和端口*/
    unsigned short udp_port; /* prot */
    int sofd;                   /* Unix socket file descriptor */
    list* clients;              /* List of active clients */
    list* clients_to_close;     /* Clients to close asynchronously */
    char neterr[ANET_ERR_LEN];   /* Error buffer for anet.c */
    unsigned int maxclients;            /* Max number of simultaneous clients */
    unsigned short uport;                  /* Config UDP listening port */
    unsigned char uportOffset;                  /* offset UDP listening port */
    unsigned short port;                   /* TCP listening port */
    int tcp_backlog;            /* TCP listen() backlog */
    char* bindaddr[_BINDADDR_MAX]; /* Addresses we should bind to */
    int bindaddr_count;         /* Number of addresses in server.bindaddr[] */
    char* unixsocket;           /* UNIX socket path */
    mode_t unixsocketperm;      /* UNIX socket permission */
    int hz;                     /* serverCron() calls frequency in hertz */
    int cronloops;              /* Number of times the cron function run */
    PORT_LONGLONG stat_rejected_conn;   /* Clients rejected because of maxclients */
    PORT_LONGLONG stat_numconnections;  /* Number of connections received */
    int tcpkeepalive;               /* Set SO_KEEPALIVE if non-zero. */

    PORT_ULONGLONG maxmemory;   /* Max number of memory bytes to use */
    PORT_LONGLONG stat_net_output_bytes; /* Bytes written to network. */

    size_t client_max_querybuf_len; /* Limit for client query buffer length */

    PORT_LONGLONG stat_net_input_bytes; /* Bytes read from network. */

    list* sendBuf;

    void* mutexHandle;
    dict* id_client;//clientid和client对象的对应关系

    unsigned int id_count;

    pthread_t thread;
    char      thread_flag;

    //entity和客户端tcp的对应关系
    //应用于转发到客户端的协议
    dict* entity_client;

    sds         entityObj;//tcp链接绑定的entity对象默认为“account”,由配置文件配置
    sds         botsObj;

    int         nodetype;//节点类型，目前有对内，对外对内，需要加入特殊类型。例如全局服务器专用节点，存储服务专用节点，指定某个服务专用节点。
}*PNetServer, NetServer;

PNetServer pNetServer = 0; /* server global state */



void RecvCallback(PNetClient c, char* buf, size_t buffsize, void* private_data) {

    PProtoHead pProtoHead = (PProtoHead)buf;
    if (c->flags & _UDP_SOCKET) {
        
        buffsize = pProtoHead->len;
        if (pProtoHead->proto == proto_rpc_create) {
            DockerRandomPushMsg(buf, buffsize);
        }
        else if (pProtoHead->proto == proto_rpc_call) {
            PProtoRPC pProtoRPC = (PProtoRPC)buf;
            VPEID pVPEID = CreateEIDFromLongLong(pProtoRPC->id);
            unsigned int addr = GetAddrFromEID(pVPEID);
            unsigned char port = GetPortFromEID(pVPEID);
 
            if (pNetServer->udp_ip == addr && pNetServer->uportOffset == port) {
                unsigned char docker = GetDockFromEID(pVPEID);
                DockerPushMsg(docker, buf, buffsize);
            }
            else {
                n_error("RecvCallback error udp addr ip: %i,%i port: %i,%i", pNetServer->udp_ip, addr, pNetServer->uportOffset, port);
            }
            DestoryEID(pVPEID);
        }
        else if (pProtoHead->proto == proto_route_call) {
            //转发给客户端
            PProtoRoute pProtoRoute = (PProtoRoute)buf;
            VPEID pVPEID = CreateEIDFromLongLong(pProtoRoute->pid);
            unsigned int addr = GetAddrFromEID(pVPEID);
            unsigned char port = GetPortFromEID(pVPEID);
            DestoryEID(pVPEID);

            if (pNetServer->udp_ip == addr && pNetServer->uportOffset == port) {
                //找到对应entity id 的client id 转发
                dictEntry* entry = dictFind(pNetServer->entity_client, (void*)&pProtoRoute->pid);
                if (entry != NULL) {
                    PNetClient pNetClient = (PNetClient)dictGetVal(entry);
                    SendToClient(pNetClient->id, buf, buffsize);
                }
            }
        }
    }
    else {
        if (pProtoHead->proto == proto_route_call) {
            //客户端收到的服务器消息
            //或者服务收到客户端的消息
            //虽然工程上说是对等的，但因为安全的原因，客户端的npc不能调用服务器的npc方法
            
            PProtoRoute  pProtoRoute = (PProtoRoute)buf;
            unsigned char docker;
            if (c->flags & _TCP_CONNECT) {
                pProtoRoute->pid = c->entityId;//替换掉对应的id
            }
            else
            {
                pProtoRoute->did = c->entityId;//替换掉对应的id
                pProtoRoute->pid = c->entityId;//替换掉对应的id
            }

            VPEID pVPEID = CreateEIDFromLongLong(pProtoRoute->pid);
            docker = GetDockFromEID(pVPEID);
            DestoryEID(pVPEID);

            DockerPushMsg(docker, buf, buffsize);
        }
        else {
            n_error("Discard the error packet")
        }
    }
}

/* Initialize a set of file descriptors to listen to the specified 'port'
 * binding the addresses specified in the Redis server configuration.
 *
 * The listening file descriptors are stored in the integer array 'fds'
 * and their number is set in '*count'.
 *
 * The addresses to bind are specified in the global server.bindaddr array
 * and their number is server.bindaddr_count. If the server configuration
 * contains no specific addresses to bind, this function will try to
 * bind * (all addresses) for both the IPv4 and IPv6 protocols.
 *
 * On success the function returns REDIS_OK.
 *
 * On error the function returns REDIS_ERR. For the function to be on
 * error, at least one of the server.bindaddr addresses was
 * impossible to bind, or no bind addresses were specified in the server
 * configuration but the function is not able to bind * for at least
 * one of the IPv4 or IPv6 protocols. */
int listenToPort(int port, int* fds, int* count) {
    int j;

    /* Force binding of 0.0.0.0 if no bind address is specified, always
     * entering the loop if j == 0. */
    if (pNetServer->bindaddr_count == 0) pNetServer->bindaddr[0] = NULL;
    for (j = 0; j < pNetServer->bindaddr_count || j == 0; j++) {
        if (pNetServer->bindaddr[j] == NULL) {
            /* Bind * for both IPv6 and IPv4, we enter here only if
             * server.bindaddr_count == 0. */
            fds[*count] = anetTcp6Server(pNetServer->neterr, port, NULL,
                pNetServer->tcp_backlog);
            if (fds[*count] != ANET_ERR) {
                anetNonBlock(NULL, fds[*count]);
                (*count)++;
            }
            fds[*count] = anetTcpServer(pNetServer->neterr, port, NULL,
                pNetServer->tcp_backlog);
            if (fds[*count] != ANET_ERR) {
                anetNonBlock(NULL, fds[*count]);
                (*count)++;
            }
            /* Exit the loop if we were able to bind * on IPv4 or IPv6,
             * otherwise fds[*count] will be ANET_ERR and we'll print an
             * error and return to the caller with an error. */
            if (*count) break;
        }
        else if (strchr(pNetServer->bindaddr[j], ':')) {
            /* Bind IPv6 address. */
            fds[*count] = anetTcp6Server(pNetServer->neterr, port, pNetServer->bindaddr[j],
                pNetServer->tcp_backlog);
        }
        else {
            /* Bind IPv4 address. */
            fds[*count] = anetTcpServer(pNetServer->neterr, port, pNetServer->bindaddr[j],
                pNetServer->tcp_backlog);
        }
        if (fds[*count] == ANET_ERR) {
            n_error(
                "Creating Server TCP listening socket %s:%i: %s",
                pNetServer->bindaddr[j] ? pNetServer->bindaddr[j] : "*",
                port, pNetServer->neterr);
            return _ERR;
        }
        anetNonBlock(NULL, fds[*count]);
        (*count)++;
    }
    return _OK;
}

int listenToUdpPort(int port, int* fds, int* count) {
    int j;

    /* Force binding of 0.0.0.0 if no bind address is specified, always
     * entering the loop if j == 0. */
    if (pNetServer->bindaddr_count == 0) pNetServer->bindaddr[0] = NULL;
    for (j = 0; j < pNetServer->bindaddr_count || j == 0; j++) {
        if (pNetServer->bindaddr[j] == NULL) {
            /* Bind * for both IPv6 and IPv4, we enter here only if
             * server.bindaddr_count == 0. */
            fds[*count] = anetUdpServer(pNetServer->neterr, port, NULL,
                pNetServer->tcp_backlog);
            if (fds[*count] != ANET_ERR) {
                anetNonBlock(NULL, fds[*count]);
                (*count)++;
            }
            /* Exit the loop if we were able to bind * on IPv4 or IPv6,
             * otherwise fds[*count] will be ANET_ERR and we'll print an
             * error and return to the caller with an error. */
            if (*count) break;
        }
        else {
            /* Bind IPv4 address. */
            fds[*count] = anetUdpServer(pNetServer->neterr, port, pNetServer->bindaddr[j],
                pNetServer->tcp_backlog);
        }
        if (fds[*count] == ANET_ERR) {
            n_warn(
                "Creating Server UDP listening socket %s:%i: %s",
                pNetServer->bindaddr[j] ? pNetServer->bindaddr[j] : "*",
                port, pNetServer->neterr);
            return _ERR;
        }
        anetNonBlock(NULL, fds[*count]);
        (*count)++;
    }
    return _OK;
}

void SendBufDestroyFun(void* value)
{
    sdsfree((sds)value);
}

//tcp链接会频繁调用关闭,在关闭后如果是tcp链接呀销毁对应的entity
void freeClient(NetClient* c, int order) {
    listNode* ln;

    /* Free the query buffer */
    sdsfree(c->recv_buf);
    c->recv_buf = NULL;
    if (c->sendBuf)
    {
        listSetFreeMethod(pNetServer->sendBuf, SendBufDestroyFun);
        listRelease(c->sendBuf);
    }

    /* Close socket, unregister events, and remove list of replies and
     * accumulated arguments. */
    if (c->fd != -1) {
        aeDeleteFileEvent(pNetServer->el, c->fd, AE_READABLE);
        aeDeleteFileEvent(pNetServer->el, c->fd, AE_WRITABLE);
        close(c->fd);
    }


    /* Remove from the list of clients */
    if (c->fd != -1 && c->id != 0) {
        ln = listSearchKey(pNetServer->clients, c);
        listDelNode(pNetServer->clients, ln);
        dictDelete(pNetServer->id_client, &c->id);
        if(c->entityId != 0)
            dictDelete(pNetServer->entity_client, (void*)&c->entityId);
    }

    /* If this client was scheduled for async freeing we need to remove it
     * from the queue. */
    if (c->flags & _CLOSE_ASAP) {
        ln = listSearchKey(pNetServer->clients_to_close, c);
        listDelNode(pNetServer->clients_to_close, ln);
    }

    if (order == 1 && c->flags & _TCP_SOCKET) {
        mp_buf* pmp_buf = mp_buf_new();
        mp_encode_int(pmp_buf, c->entityId);

        unsigned short len = sizeof(ProtoRPCCreate) + pmp_buf->len;
        PProtoHead pProtoHead = malloc(len);
        pProtoHead->len = len;
        pProtoHead->proto = proto_rpc_destory;

        PProtoRPCCreate protoRPCCreate = (PProtoRPCCreate)pProtoHead;
        memcpy(protoRPCCreate->callArg, pmp_buf->b, pmp_buf->len);

        VPEID pVPEID = CreateEIDFromLongLong(c->entityId);
        unsigned int dockerid = GetDockFromEID(pVPEID);
        DestoryEID(pVPEID);

        DockerPushMsg(dockerid, (unsigned char*)pProtoHead, len);
    }

    /* Release other dynamically allocated client structure fields,
     * and finally release the client structure itself. */
    zfree(c);
}

void ClientsDestroyFun(void* value)
{
    freeClient((NetClient*)value, 0);
}

void freeClientsInAsyncFreeQueue(void) {
    while (listLength(pNetServer->clients_to_close)) {
        listNode* ln = listFirst(pNetServer->clients_to_close);
        NetClient* c = listNodeValue(ln);

        c->flags &= ~_CLOSE_ASAP;
        freeClient(c, 0);
        listDelNode(pNetServer->clients_to_close, ln);
    }
}

/*
* 这里尽可能多的将数据从底层缓冲区复制到当前层，批量的复制可以减少系统锁的开销。
*/

void readQueryFromClient(aeEventLoop* el, int fd, void* privdata, int mask) {

    PNetClient c = (NetClient*)privdata;
    int nread;
    _NOTUSED(el);
    _NOTUSED(mask);

    if (c->recv_status == 0)
    {
        c->recv_size = sizeof(unsigned short);
        c->recv_pos = 0;
    }

    if (c->recv_size + c->recv_pos > sdslen(c->recv_buf)) {
        c->recv_buf = sdsMakeRoomFor(c->recv_buf, c->recv_size + c->recv_pos);
        sdsIncrLen(c->recv_buf, c->recv_size + c->recv_pos);
    }

    n_details("readQueryFromClient::read fhand:;%i client::%i", fd, c->id);
    
    nread = (int)IF_WIN32(w_read, read)(fd, (unsigned char *)c->recv_buf + c->recv_pos, c->recv_size - c->recv_pos);                          WIN_PORT_FIX /* cast (int) */
    if (nread == -1) {
        if (errno == EAGAIN) {
            nread = 0;
        }
        else {
            n_error("Reading from client: %s", IF_WIN32(wsa_strerror(errno), strerror(errno)));
            freeClient(c, 0);
            return;
        }
    }
    else if (nread == 0) {
        n_error("Client closed connection");
        freeClient(c, 0);
        return;
    }

    WIN32_ONLY(WSIOCP_QueueNextRead(fd);)
    if (nread) {

        if (c->recv_status == 0)
        {
            c->recv_size = *(unsigned short*)c->recv_buf;
            if (c->recv_size > pNetServer->client_max_querybuf_len) {
                n_error("Closing client that reached max query buffer length");
                freeClient(c, 0);
                return;
            }

            c->recv_pos = sizeof(unsigned short);
            c->recv_status = 1;
        }
        else {
            c->recv_pos += nread;

            //包收完整了
            if (c->recv_pos == c->recv_size) {
                RecvCallback(c, c->recv_buf, c->recv_size, c->private_data);
                c->recv_status = 0;
            }
        }
        pNetServer->stat_net_input_bytes += nread;
    }
    else {
        return;
    }
}

void readQueryFromUdp(aeEventLoop* el, int fd, void* privdata, int mask) {
    NetClient* c = (NetClient*)privdata;
    _NOTUSED(el);
    _NOTUSED(mask);

    struct sockaddr_in sin;
    socklen_t	sinLen = sizeof(sin);

    int ret = IF_WIN32(WSIOCP_WSARecvFrom, recvfrom)(fd, (char*)c->recvBuf, PACKET_MAX_SIZE_UDP, 0, (struct sockaddr*)&sin, &sinLen);

    if (ret == -1)
    {
        n_error("recvfrom error: %s", IF_WIN32(wsa_strerror(errno), strerror(errno)));
        freeClient(c, 0);
        return;
    }
    //WIN32_ONLY(WSIOCP_QueueUdpNextRead(fd, (struct sockaddr*)&sin, &sinLen););

    RecvCallback(c, c->recvBuf, ret, c->private_data);
}

NetClient* createClient(int fd) {
    NetClient* c = zmalloc(sizeof(NetClient));

    /* passing -1 as fd it is possible to create a non connected client.
     * This is useful since all the Redis commands needs to be executed
     * in the context of a client. When commands are executed in other
     * contexts (for instance a Lua script) we need a non connected client. */
    if (fd != -1) {
        anetNonBlock(NULL, fd);
        anetEnableTcpNoDelay(NULL, fd);
        if (pNetServer->tcpkeepalive)
            anetKeepAlive(NULL, fd, pNetServer->tcpkeepalive);
        if (aeCreateFileEvent(pNetServer->el, fd, AE_READABLE,
            readQueryFromClient, c) == AE_ERR)
        {
            close(fd);
            zfree(c);
            return NULL;
        }
    }

    c->fd = fd;
    c->flags = 0;
    c->id = ++pNetServer->id_count;
    c->recv_buf = sdsempty();
    c->sendBuf = listCreate();
    c->entityId = 0;

    if (fd != -1) {
        dictAddWithUint(pNetServer->id_client, c->id, c);
        listAddNodeTail(pNetServer->clients, c);
    }
    return c;
}

NetClient* createUDPClient(int fd) {
    NetClient* c = zmalloc(sizeof(NetClient));

    c->fd = fd;
    c->flags = 0;
    c->id = ++pNetServer->id_count;
    c->recv_buf = sdsempty();

    if (fd != -1) {
        dictAddWithUint(pNetServer->id_client, c->id, c);
        listAddNodeTail(pNetServer->clients, c);
    }
    return c;
}

static void acceptCommonHandler(int fd, int flags) {
    NetClient* c;
    if ((c = createClient(fd)) == NULL) {
        n_error(
            "Error registering fd event for the new client: %s (fd=%d)",
            strerror(errno), fd);
        close(fd); /* May be already closed, just ignore errors */
        return;
    }

    n_details("acceptCommonHandler::accept fhand:;%i client::%i", fd, c->id);
    /* If maxclient directive is set and this is one client more... close the
     * connection. Note that we create the client instead to check before
     * for this condition, since now the socket is already set in non-blocking
     * mode and we can send an error for free using the Kernel I/O */
    if (listLength(pNetServer->clients) > (PORT_ULONG)pNetServer->maxclients) {
        char* err = "-ERR max number of clients reached\r\n";

        /* That's a best effort error message, don't check write errors */
        if (IF_WIN32(w_write, write)(c->fd, err, strlen(err)) == -1) {
            /* Nothing to do, Just to avoid the warning... */
        }
        pNetServer->stat_rejected_conn++;
        freeClient(c, 0);
        return;
    }
    pNetServer->stat_numconnections++;
    c->flags |= flags;

    if (c->flags & _TCP_SOCKET) {
        //这里调用绑定协议尝试创建entity
        mp_buf* pmp_buf = mp_buf_new();

        if (c->flags & _TCP_CONNECT)
        {
            mp_encode_bytes(pmp_buf, pNetServer->botsObj, sdslen(pNetServer->botsObj));
        }
        else
        {
            mp_encode_bytes(pmp_buf, pNetServer->entityObj, sdslen(pNetServer->entityObj));
        }

        if (c->flags & _TCP_CONNECT)
        {
            mp_encode_map(pmp_buf, 2);
            mp_encode_bytes(pmp_buf, "isconnect", strlen("isconnect"));
            mp_encode_int(pmp_buf, 1);
        }
        else {
            mp_encode_map(pmp_buf, 1);
        }

        mp_encode_bytes(pmp_buf, "clientid", strlen("clientid"));
        mp_encode_int(pmp_buf, c->id);

        unsigned short len = sizeof(ProtoRPCCreate) + pmp_buf->len;
        PProtoHead pProtoHead = malloc(len);
        pProtoHead->len = len;
        pProtoHead->proto = proto_rpc_create;

        PProtoRPCCreate protoRPCCreate = (PProtoRPCCreate)pProtoHead;
        memcpy(protoRPCCreate->callArg, pmp_buf->b, pmp_buf->len);

        DockerRandomPushMsg((unsigned char*)pProtoHead, len);
    }
}

void acceptTcpHandler(aeEventLoop* el, int fd, void* privdata, int mask) {
    int cport, cfd, max = MAX_ACCEPTS_PER_CALL;
    char cip[_IP_STR_LEN];
    _NOTUSED(el);
    _NOTUSED(mask);
    _NOTUSED(privdata);

    while (max--) {
        cfd = anetTcpAccept(pNetServer->neterr, fd, cip, sizeof(cip), &cport);
        if (cfd == ANET_ERR) {
            if (errno != EWOULDBLOCK) {
                n_warn(
                    "Accepting client connection: %s", pNetServer->neterr);
#ifdef _WIN32
                if (WSIOCP_QueueAccept(fd) == -1) {
                    n_warn(
                        "acceptTcpHandler: failed to queue another accept.");
                }
#endif
            }
            return;
        }
        n_details("Accepted ip:%s port:%i", cip, cport);
        acceptCommonHandler(cfd, _TCP_SOCKET);
    }
}


void ConnectTcp(const char* addr, unsigned short port) {

    char err[256] = { 0 };
    int s = anetTcpNonBlockConnect(err, (char*)addr, port);

    if(s != ANET_ERR)
        acceptCommonHandler(s, _TCP_SOCKET | _TCP_CONNECT);
    else
        n_error("client ConnectTcp: %s", err);
}

void acceptUnixHandler(aeEventLoop* el, int fd, void* privdata, int mask) {
    int cfd, max = MAX_ACCEPTS_PER_CALL;
    _NOTUSED(el);
    _NOTUSED(mask);
    _NOTUSED(privdata);

    while (max--) {
        cfd = anetUnixAccept(pNetServer->neterr, fd);
        if (cfd == ANET_ERR) {
            if (errno != EWOULDBLOCK)
                n_warn(
                    "Accepting client connection: %s", pNetServer->neterr);
            return;
        }
        n_details( "Accepted connection to %s", pNetServer->unixsocket);
        acceptCommonHandler(cfd, _UNIX_SOCKET);
    }
}

/* Schedule a client to free it at a safe time in the serverCron() function.
 * This function is useful when we need to terminate a client but we are in
 * a context where calling freeClient() is not possible, because the client
 * should be valid for the continuation of the flow of the program. */
void freeClientAsync(NetClient* c) {
    if (c->flags & _CLOSE_ASAP) return;
    c->flags |= _CLOSE_ASAP;
    listAddNodeTail(pNetServer->clients_to_close, c);
}

#ifdef _WIN32
void sendReplyBufferDone(aeEventLoop* el, int fd, void* privdata, int written) {
    WSIOCP_Request* req = (WSIOCP_Request*)privdata;
    NetClient* c = (NetClient*)req->client;
    _NOTUSED(el);
    _NOTUSED(fd);

    if (listLength(c->sendBuf) == 0) {
        aeDeleteFileEvent(pNetServer->el, c->fd, AE_WRITABLE);

        /* Close connection after entire reply has been sent. */
        if (c->flags & _CLOSE_AFTER_REPLY) {
            freeClientAsync(c);
        }
    }
}

void sendReplyToClient(aeEventLoop* el, int fd, void* privdata, int mask) {
    NetClient* c = (NetClient*)privdata;
    int nwritten = 0, totwritten = 0;
    size_t objmem;

    _NOTUSED(el);
    _NOTUSED(mask);

    n_details("readQueryFromClient::send fhand:;%i client::%i", fd, c->id);

    while (listLength(c->sendBuf)) {
        listNode* head = listFirst(c->sendBuf);
        char* pBbuf = (char*)listNodeValue(head);

        int result = WSIOCP_SocketSend(fd, pBbuf, sdslen((char*)pBbuf),
            el, c, pBbuf, sendReplyBufferDone);

        if (errno != WSA_IO_PENDING) {
            //多次出错可能导致死循环这里要有处理方法杀死客户端链接
            n_warn("warn pending to client: %s", wsa_strerror(errno));
            return;
        }
        else if (result == SOCKET_ERROR && errno != WSA_IO_PENDING) {
            n_error("Error writing to client: %s", wsa_strerror(errno));
            freeClient(c, 0);
            return;
        }
        sdsfree(pBbuf);
        listDelNode(c->sendBuf, head);
        totwritten += nwritten;
 
        /* Note that we avoid to send more than REDIS_MAX_WRITE_PER_EVENT
         * bytes, in a single threaded server it's a good idea to serve
         * other clients as well, even if a very large request comes from
         * super fast link that is always able to accept data (in real world
         * scenario think about 'KEYS *' against the loopback interface).
         *
         * However if we are over the maxmemory limit we ignore that and
         * just deliver as much data as it is possible to deliver. */
        pNetServer->stat_net_output_bytes += totwritten;
        if (totwritten > _MAX_WRITE_PER_EVENT &&
            (pNetServer->maxmemory == 0 ||
                zmalloc_used_memory() < pNetServer->maxmemory)) break;
    }
}
#else
void sendReplyToClient(aeEventLoop* el, int fd, void* privdata, int mask) {
    NetClient* c = privdata;
    int nwritten = 0, totwritten = 0;
    REDIS_NOTUSED(el);
    REDIS_NOTUSED(mask);

    while (listLength(c->sendBuf)) {

        listNode* head = listFirst(c->sendBuf);
        char* pBbuf = (char*)listNodeValue(head);

        nwritten = write(fd, pBbuf, sdslen((char*)listNodeValue(head)));
        if (nwritten <= 0) break;

        sdsfree(pBbuf);
        listDelNode(c->sendBuf, head);
        totwritten += nwritten;

        /* Note that we avoid to send more than REDIS_MAX_WRITE_PER_EVENT
         * bytes, in a single threaded server it's a good idea to serve
         * other clients as well, even if a very large request comes from
         * super fast link that is always able to accept data (in real world
         * scenario think about 'KEYS *' against the loopback interface).
         *
         * However if we are over the maxmemory limit we ignore that and
         * just deliver as much data as it is possible to deliver. */
        pNetServer->stat_net_output_bytes += totwritten;
        if (totwritten > _MAX_WRITE_PER_EVENT &&
            (pNetServer->maxmemory == 0 ||
                zmalloc_used_memory() < pNetServer->maxmemory)) break;
    }
    if (nwritten == -1) {
        if (errno == EAGAIN) {
            nwritten = 0;
        }
        else {
            n_error(
                "Error writing to client: %s", strerror(errno));
            freeClient(c, 0);
            return;
        }
    }

    if (listNodeValue(head) == 0) {
        aeDeleteFileEvent(server.el, c->fd, AE_WRITABLE);

        /* Close connection after entire reply has been sent. */
        if (c->flags & _CLOSE_AFTER_REPLY) freeClient(c, 0);
    }
}
#endif

/* This function is called every time we are going to transmit new data
 * to the client. The behavior is the following:
 *
 * If the client should receive new data (normal clients will) the function
 * returns REDIS_OK, and make sure to install the write handler in our event
 * loop so that when the socket is writable new data gets written.
 *
 * If the client should not receive new data, because it is a fake client
 * (used to load AOF in memory), a master or because the setup of the write
 * handler failed, the function returns REDIS_ERR.
 *
 * The function may return REDIS_OK without actually installing the write
 * event handler in the following cases:
 *
 * 1) The event handler should already be installed since the output buffer
 *    already contained something.
 * 2) The client is a slave but not yet online, so we want to just accumulate
 *    writes in the buffer but not actually sending them yet.
 *
 * Typically gets called every time a reply is built, before adding more
 * data to the clients output buffers. If the function returns REDIS_ERR no
 * data should be appended to the output buffers. */
int prepareClientToWrite(NetClient* c) {

    if (c->fd <= 0) return _ERR; /* Fake client for AOF loading. */

    /* Only install the handler if not already installed and, in case of
     * slaves, if the client can actually receive writes. */
    if (listLength(c->sendBuf) != 0)
    {
        /* Try to install the write handler. */
        if (aeCreateFileEvent(pNetServer->el, c->fd, AE_WRITABLE,
            sendReplyToClient, c) == AE_ERR)
        {
            freeClientAsync(c);
            return _ERR;
        }
    }

    /* Authorize the caller to queue in the output buffer of this client. */
    return _OK;
}


void NetCommand(char* pBuf) {
    PProtoHead pProtoHead =(PProtoHead)(pBuf);

    if (pProtoHead->proto == proto_net_bind) {
        PProtoNetBind pProtoNetBind = (PProtoNetBind)pProtoHead;

        dictEntry* entry = dictFind(pNetServer->id_client, (unsigned int*)&pProtoNetBind->clientId);
        if (entry == NULL) {
            //没有找到客户端id丢弃封包
            n_error("NetCommand::proto_net_bind %i lost packet not find clientid %i", pProtoHead->proto, pProtoNetBind->clientId);
            return;
        }

        //这里要调用映射层获得映射关系
        NetClient* c = (NetClient*)dictGetVal(entry);
        c->entityId = pProtoNetBind->entityId;

        entry = dictFind(pNetServer->entity_client, (void*)&c->entityId);
        if (entry != NULL)
            dictDelete(pNetServer->entity_client, (void*)&c->entityId);
        dictAddWithLonglong(pNetServer->entity_client, c->entityId, c);
    }
    else if (pProtoHead->proto == proto_net_destory) {

        PProtoNetDestory pProtoNetDestory = (PProtoNetDestory)pProtoHead;

        dictEntry* entry = dictFind(pNetServer->id_client, (unsigned int*)&pProtoNetDestory->clientId);
        if (entry == NULL) {
            //没有找到客户端id丢弃封包
            n_error("NetCommand::proto_net_destory %i lost packet not find clientid %i", pProtoHead->proto, pProtoNetDestory->clientId);
            return;
        }

        //这里要调用映射层获得映射关系
        NetClient* c = (NetClient*)dictGetVal(entry);
        freeClient(c, 1);
    }
    else if (pProtoHead->proto == proto_net_connect) {

        PProtoConnect pProtoConnect = (PProtoConnect)pProtoHead;
        ConnectTcp(pProtoConnect->ip, pProtoConnect->port);
    }
}

// 这里还要处理命令， 如果client的id为零表示为这个封包是一个控制命令
//1，绑定entityid
//2，销毁
void PipeHandler() {

    //从队列中取出数据
    void* value = 0;
    MutexLock(pNetServer->mutexHandle, "");
    if (listLength(pNetServer->sendBuf) != 0) {
        listNode* node = listFirst(pNetServer->sendBuf);
        value = listNodeValue(node);
        listDelNode(pNetServer->sendBuf, node);
    }
    MutexUnlock(pNetServer->mutexHandle, "");

    if (value == 0)
        return;

    PProtoHead pProtoHead = (PProtoHead)value;
    NetClient* c = 0;

    if (pProtoHead->proto == proto_client_id) {

        PProtoSendToClient pProtoSendToClient = (PProtoSendToClient)value;
        if (pProtoSendToClient->clientId == 0) {
            NetCommand(pProtoSendToClient->buf);
            sdsfree(value);
            return;
        }

        dictEntry* entry = dictFind(pNetServer->id_client, (unsigned int*)&pProtoSendToClient->clientId);
        if (entry == NULL) {
            //没有找到客户端id丢弃封包
            n_error("PipeHandler::proto_client_id %i lost packet not find clientid %i", pProtoHead->proto, pProtoSendToClient->clientId);
            sdsfree(value);
            return;
        }
        c = (NetClient*)dictGetVal(entry);
        listAddNodeTail(c->sendBuf, sdsnewlen(pProtoSendToClient->buf, pProtoSendToClient->protoHead.len - sizeof(PProtoSendToClient)));
    }
    else if (pProtoHead->proto == proto_client_entity) {
        PProtoSendToEntity pProtoSendToEntity = (PProtoSendToEntity)value;
        if (pProtoSendToEntity->entityId == 0) {
            NetCommand(pProtoSendToEntity->buf);
            sdsfree(value);
            return;
        }

        dictEntry* entry = dictFind(pNetServer->entity_client, (unsigned long long*)&pProtoSendToEntity->entityId);
        if (entry == NULL) {
            //没有找到客户端id丢弃封包
            n_error("PipeHandler::proto_client_entity %i lost packet not find entityId %U", pProtoHead->proto, pProtoSendToEntity->entityId);
            sdsfree(value);
            return;
        }
        c = (NetClient*)dictGetVal(entry);
        listAddNodeTail(c->sendBuf, sdsnewlen(pProtoSendToEntity->buf, pProtoSendToEntity->protoHead.len - sizeof(ProtoSendToEntity)));
    }
    sdsfree(value);

    if (prepareClientToWrite(c) != _OK) {
        return;
    }

    if (c->flags & _CLOSE_AFTER_REPLY) return;
}

int serverCron(struct aeEventLoop* eventLoop, PORT_LONGLONG id, void* clientData) {

    freeClientsInAsyncFreeQueue();

    pNetServer->cronloops++;

    PipeHandler();

    return 1000 / pNetServer->hz;
}

/*
任何entity都会通过全局对象或空间对象获得其他对象的mailbox，
这样就可以在任何条件下发送消息给entity到网络层，
网络层会有所有entity对象的dock映射关系，
所以mailbox不需要携带dock数据。
*/
void SendToNode(char* ip, unsigned short port, unsigned char* b, unsigned short s) {

    int sock = IF_WIN32(w_socket, socket)(AF_INET, SOCK_DGRAM, 0);//IPV4  SOCK_DGRAM 数据报套接字（UDP协议）  
    if (sock < 0)
    {
        n_error("SendToNode::sock < 0 %s %i %i", ip, port);
        return;
    }
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    IF_WIN32(w_inet_pton, inet_pton)(AF_INET, ip, (void*)&server_addr.sin_addr);

    socklen_t len = sizeof(server_addr);

    if (IF_WIN32(w_sendto, sendto)(sock, b, s, 0, (struct sockaddr*)&server_addr, len) < 0)
    {
        n_error("SendToNode::sendto error %s %i", ip, port);
        return;
    }
}

void SendToNodeWithUINT(unsigned int ip, unsigned short port, unsigned char* b, unsigned short s) {

    int sock = IF_WIN32(w_socket, socket)(AF_INET, SOCK_DGRAM, 0);//IPV4  SOCK_DGRAM 数据报套接字（UDP协议）  
    if (sock < 0)
    {
        n_error("SendToNode::sock < 0 %i %i %i", ip, port);
        return;
    }
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.S_un.S_addr = htonl(ip);

    socklen_t len = sizeof(server_addr);

    if (IF_WIN32(w_sendto, sendto)(sock, b, s, 0, (struct sockaddr*)&server_addr, len) < 0)
    {
        n_error("SendToNodeWithUINT::sendto error %i %i", ip, port);
        return;
    }
}

void Send6ToNode(char* ip, unsigned short port, unsigned char* b, unsigned short s) {

    int sock = IF_WIN32(w_socket, socket)(AF_INET6, SOCK_DGRAM, 0);//IPV4  SOCK_DGRAM 数据报套接字（UDP协议）  
    if (sock < 0)
    {
        n_error("Send6ToNode::sock < 0 %s %i %i", ip, port);
        return;
    }
    struct sockaddr_in server_addr;
    w_inet_pton(AF_INET6, ip, (void*)&server_addr.sin_addr);
    server_addr.sin_port = htons(port);

    socklen_t len = sizeof(server_addr);

    if (IF_WIN32(w_sendto, sendto)(sock, b, s, 0, (struct sockaddr*)&server_addr, len) < 0)
    {
        n_error("Send6ToNode::sendto %s %i %i", ip, port);
        return;
    }
}

/*这里要加队列锁，支持多线程*/
int SendToClient(unsigned int id, const char* b, unsigned short s) {

    sds sendBuf = sdsnewlen(0, s + sizeof(ProtoSendToClient));
    PProtoSendToClient pProtoSendToClient = (PProtoSendToClient)sendBuf;
    pProtoSendToClient->protoHead.len = s + sizeof(ProtoSendToClient);
    pProtoSendToClient->protoHead.proto = proto_client_id;
    pProtoSendToClient->clientId = id;
    memcpy(pProtoSendToClient->buf, b, s);
 
    MutexLock(pNetServer->mutexHandle, "");
    listAddNodeTail(pNetServer->sendBuf, sendBuf);
    MutexUnlock(pNetServer->mutexHandle, "");
    return _OK;
}

int SendToEntity(unsigned long long entityid, const char* b, unsigned short s) {

    sds sendBuf = sdsnewlen(0, s + sizeof(ProtoSendToEntity));
    PProtoSendToEntity pProtoSendToEntity = (PProtoSendToEntity)sendBuf;
    pProtoSendToEntity->protoHead.len = s + sizeof(ProtoSendToEntity);
    pProtoSendToEntity->protoHead.proto = proto_client_entity;
    pProtoSendToEntity->entityId = entityid;
    memcpy(pProtoSendToEntity->buf, b, s);

    MutexLock(pNetServer->mutexHandle, "");
    listAddNodeTail(pNetServer->sendBuf, sendBuf);
    MutexUnlock(pNetServer->mutexHandle, "");
    return _OK;
}

static void doJsonParseFile(char* config)
{
    if (config == NULL) {
        config = getenv("GrypaniaAssetsPath");
        if (config == 0 || IF_WIN32(w_access,access)(config, 0) != 0) {
            config = "../../res/server/config_defaults.json";
            if (IF_WIN32(w_access, access)(config, 0) != 0) {
                return;
            }
        }
    }

    FILE* f; size_t len; char* data;
    cJSON* json;

    f = fopen(config, "rb");

    if (f == NULL) {
        //printf("Error Open File: [%s]\n", filename);
        return;
    }

    fseek(f, 0, SEEK_END);
    len = ftell(f);
    fseek(f, 0, SEEK_SET);
    data = (char*)malloc(len + 1);
    fread(data, 1, len, f);
    fclose(f);

    json = cJSON_Parse(data);
    if (!json) {
        //printf("Error before: [%s]\n", cJSON_GetErrorPtr());	
        free(data);
        return;
    }

    cJSON* logJson = cJSON_GetObjectItem(json, "net_config");
    if (logJson) {

        cJSON* item = logJson->child;
        while ((item != NULL) && (item->string != NULL))
        {
            if (strcmp(item->string, "hz") == 0) {
                pNetServer->hz = (int)cJSON_GetNumberValue(item);
            }
            else if (strcmp(item->string, "port") == 0) {
                pNetServer->port = (unsigned short)cJSON_GetNumberValue(item);
            }
            else if (strcmp(item->string, "uport") == 0) {
                if(pNetServer->uport == 9577)
                    pNetServer->uport = (unsigned short)cJSON_GetNumberValue(item);
            }
            else if (strcmp(item->string, "ips") == 0) {

                pNetServer->bindaddr_count = 0;
                for (int i = 0; i < cJSON_GetArraySize(item); i++)
                {
                    cJSON* arrayItem = cJSON_GetArrayItem(item, i);
                    if (arrayItem->type == cJSON_String) {
                        pNetServer->bindaddr[i] = sdsnew(cJSON_GetStringValue(item));
                        if (++pNetServer->bindaddr_count >= _BINDADDR_MAX) {
                            break;
                        }
                    }
                } 
            }
            else if (strcmp(item->string, "maxclients") == 0) {
                pNetServer->maxclients = (unsigned short)cJSON_GetNumberValue(item);
            }
            else if (strcmp(item->string, "client_max_querybuf_len") == 0) {
                pNetServer->client_max_querybuf_len = (size_t)cJSON_GetNumberValue(item);
            }
            else if (strcmp(item->string, "udp_ip") == 0) {
                InetPton(cJSON_GetStringValue(item), &pNetServer->udp_ip);
            }
            else if (strcmp(item->string, "entityObj") == 0) {
                sdsfree(pNetServer->entityObj);
                pNetServer->entityObj = sdsnew(cJSON_GetStringValue(item));
            }
            else if (strcmp(item->string, "botsObj") == 0) {
                sdsfree(pNetServer->botsObj);
                pNetServer->botsObj = sdsnew(cJSON_GetStringValue(item));
            }

            item = item->next;
        }
    }

    cJSON_Delete(json);
    free(data);
}

void beforeSleep(struct aeEventLoop* eventLoop) {

    //检查到标记退出线程
    if (pNetServer->thread_flag) {
        aeStop(eventLoop);
    }
}

void* NetRun(void* pVoid) {

    _NOTUSED(pVoid);
    aeSetBeforeSleepProc(pNetServer->el, beforeSleep);
    aeMain(pNetServer->el);
    aeDeleteEventLoop(pNetServer->el);
    return NULL;
}

void NetServiceCreate(int nodetype, unsigned short listentcp) {

    if (pNetServer) return;
   
    pNetServer = calloc(1, sizeof(NetServer));

    pNetServer->clients = listCreate();
    pNetServer->clients_to_close = listCreate();
    pNetServer->el = aeCreateEventLoop(pNetServer->maxclients + EVENTLOOP_FDSET_INCR);
    pNetServer->sendBuf = listCreate();
    pNetServer->mutexHandle = MutexCreateHandle(LockLevel_4);
    pNetServer->id_client = dictCreate(DefaultUintPtr(), NULL);
    pNetServer->hz = 10;

    if (listentcp)
        pNetServer->port = listentcp;
    else
        pNetServer->port = 9577;

    pNetServer->uport = 7795;
    pNetServer->maxclients = 2000;
    pNetServer->client_max_querybuf_len = 4 * 1024 * 1024;//4M
    pNetServer->udp_ip = 0;
    pNetServer->udp_port = pNetServer->uport;
    pNetServer->entity_client = dictCreate(DefaultLonglongPtr(), NULL);
    pNetServer->entityObj = sdsnew("account");
    pNetServer->botsObj = sdsnew("bots");
    pNetServer->nodetype = nodetype;

    doJsonParseFile(NULL);

    if (!nodetype) {
        /* Open the TCP listening socket for the user commands. */
         if (pNetServer->port != 0 &&
            listenToPort(pNetServer->port, pNetServer->ipfd, &pNetServer->ipfd_count) == _ERR)
            exit(1);
    }
 
    int udpSevRet = _ERR;
    for (pNetServer->uportOffset = 0; pNetServer->uportOffset < 256; pNetServer->uportOffset++) {
        /* Open the UDP listening socket for the user commands. */
        pNetServer->udp_port = pNetServer->uport + pNetServer->uportOffset;
        if (pNetServer->uport != 0 &&
            (udpSevRet = listenToUdpPort(pNetServer->udp_port, pNetServer->udpfd, &pNetServer->udpfd_count)) != _ERR) {
              
            if(pNetServer->udp_ip == 0)
                IntConfigPrimary(pNetServer->udpfd[0], &pNetServer->udp_ip, &pNetServer->udp_port);
            if (pNetServer->udp_ip == 0 || pNetServer->udp_port == 0) {
                n_error("InitServer:: udp_ip or udp_port is empty! ip:%i port:%i", pNetServer->udp_ip, pNetServer->udp_port);
            }
            else {
                if (pNetServer->nodetype) {
                    SetUpdIPAndPortToInside(pNetServer->udp_ip, pNetServer->udp_port);
                }
                else {
                    SetUpdIPAndPortToOutside(pNetServer->udp_ip, pNetServer->udp_port);
                }
            }
            break;
        }
    }

    if (_ERR == udpSevRet) {
        n_error("Creating Server UDP listening socket in all port");
        exit(1);
    }

    /* Open the listening Unix domain socket. */

    if (pNetServer->unixsocket != NULL) {
        unlink(pNetServer->unixsocket); 
        pNetServer->sofd = anetUnixServer(pNetServer->neterr, pNetServer->unixsocket,
            pNetServer->unixsocketperm, pNetServer->tcp_backlog);
        if (pNetServer->sofd == ANET_ERR) {
            n_error("Opening Unix socket: %s", pNetServer->neterr);
            exit(1);
        }
        anetNonBlock(NULL, pNetServer->sofd);
    }

    /* Abort if there are no listening sockets at all. */
    if (pNetServer->ipfd_count == 0 && pNetServer->sofd < 0) {
        n_error("Configured to not listen anywhere, exiting.");
        exit(1);
    }

    /* Create the serverCron() time event, that's our main way to process
 * background operations. */
  
    if (aeCreateTimeEvent(pNetServer->el, 1, serverCron, NULL, NULL) == AE_ERR) {
        n_error("Can't create the serverCron time event.");
        exit(1);
    }

    /* Create an event handler for accepting new connections in TCP and Unix
     * domain sockets. */

    for (int j = 0; j < pNetServer->ipfd_count; j++) {
        if (aeCreateFileEvent(pNetServer->el, pNetServer->ipfd[j], AE_READABLE,
            acceptTcpHandler, NULL) == AE_ERR)
        {
            n_details(
                "Unrecoverable error creating server.ipfd file event.");
        }
    }
    
    for (int j = 0; j < pNetServer->udpfd_count; j++) {

        NetClient* c;
        if ((c = createUDPClient(pNetServer->udpfd[j])) == NULL) {
            n_error(
                "Error registering fd event for the new client: %s (fd=%d)",
                strerror(errno), pNetServer->udpfd[j]);
            close(pNetServer->udpfd[j]); 
            continue;
        }

        struct sockaddr_in sin;
        socklen_t	sinLen = sizeof(sin);

        int ret = IF_WIN32(WSIOCP_WSARecvFrom, recvfrom)(pNetServer->udpfd[j], (char*)c->recvBuf, PACKET_MAX_SIZE_UDP, 0, (struct sockaddr*)&sin, &sinLen);

        if (listLength(pNetServer->clients) > (PORT_ULONG)pNetServer->maxclients) {
            char* err = "-ERR max number of clients reached\r\n";

            if (IF_WIN32(w_write, write)(c->fd, err, strlen(err)) == -1) {

            }
            pNetServer->stat_rejected_conn++;
            freeClient(c, 0);
            continue;
        }
        pNetServer->stat_numconnections++;
        c->flags |= _UDP_SOCKET;

        if (aeCreateFileEvent(pNetServer->el, pNetServer->udpfd[j], AE_READABLE,
            readQueryFromUdp, c) == AE_ERR)
        {
            n_details(
                "Unrecoverable error creating server.udpfd file event.");
        }
    }

    if (pNetServer->sofd > 0 && aeCreateFileEvent(pNetServer->el, pNetServer->sofd, AE_READABLE,
        acceptUnixHandler, NULL) == AE_ERR) n_details("Unrecoverable error creating server.sofd file event.");

    //开启线程开始监听
    pthread_create(&pNetServer->thread, NULL, NetRun, pNetServer);
}

void NetUDPAddr(unsigned int* ip, unsigned char* uportOffset, unsigned short* uport) {
    if (pNetServer == NULL) {
        return;
    }

   *ip = pNetServer->udp_ip;
   *uportOffset = pNetServer->uportOffset;
   *uport = pNetServer->uport;
}

void NetCancel() {

    //将线程停止标记设置为1
    pNetServer->thread_flag = 1;

    //等待线程结束
    pthread_join(pNetServer->thread, NULL);
}

/*注意这里要先销毁线程*/
void NetServerDestory() {

    if (pNetServer) {
        NetCancel();

        for (int i = 0; i < pNetServer->bindaddr_count; i++) {
            sdsfree(pNetServer->bindaddr[i]);
        }

        listRelease(pNetServer->clients_to_close);
        listSetFreeMethod(pNetServer->clients, ClientsDestroyFun);
        listRelease(pNetServer->clients);
        aeDeleteEventLoop(pNetServer->el);

        listSetFreeMethod(pNetServer->sendBuf, SendBufDestroyFun);
        listRelease(pNetServer->sendBuf);
        MutexDestroyHandle(pNetServer->mutexHandle);
        dictRelease(pNetServer->id_client);
        dictRelease(pNetServer->entity_client);
        sdsfree(pNetServer->entityObj);
        sdsfree(pNetServer->botsObj);

        for (int i = 0; i < _BINDADDR_MAX; i++) {
            if (pNetServer->bindaddr[i] != 0)
                sdsfree(pNetServer->bindaddr[i]);
        }

        pNetServer = 0;
    }
}