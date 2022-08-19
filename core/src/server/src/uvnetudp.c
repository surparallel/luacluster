/*
* Copyright(C) 2021 - 2022, sun shuo <sun.shuo@surparallel.org>
* All rights reserved.
*
* This program is free software : you can redistribute it and / or modify
* it under the terms of the GNU Affero General Public License as
* published by the Free Software Foundation, either version 3 of the
* License, or(at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
* GNU Affero General Public License for more details.
*
* You should have received a copy of the GNU Affero General Public License
* along with this program.If not, see < https://www.gnu.org/licenses/>.
*/

#include "uv.h"
#include "plateform.h"
#include "filesys.h"
#include "cJSON.h"
#include "sds.h"
#include "netex.h"
#include "elog.h"
#include "dict.h"
#include "adlist.h"
#include "proto.h"
#include "entityid.h"
#include "lua_cmsgpack.h"
#include "dicthelp.h"
#include "docker.h"
#include "locks.h"
#include "redishelp.h"
#include "timesys.h"
#include "dictQueue.h"
#include "configfile.h"
#include "bitorder.h"

#define _BINDADDR_MAX 16
#define PACKET_MAX_SIZE_UDP					1472

#define _UDP_SOCKET (1<<0)      /* Client is in Pub/Sub mode. */
#define _TCP_SOCKET (1<<1)
#define _TCP_CONNECT (1<<2)

#define _TCP4_SOCKET (1<<0)      /* Client is in Pub/Sub mode. */
#define _TCP6_SOCKET (1<<1)

typedef struct _NetUdp {

    unsigned short uportBegin;                  /*  UDP listening port */
    unsigned char uportOffset;             /* offset UDP listening port */
    unsigned int udp_ip;                   /* udp_port = uport + uportOffset 最终监听的ip和端口*/
    unsigned short udp_port;               /* prot */

    uv_udp_t* udp4;
    uv_loop_t* loop;
    uv_thread_t thread;

    unsigned int allClientID;   //client计数器
    dict* id_client;            //clientid和client对象的对应关系
    list* clients;              /* List of active clients */

    unsigned long long stat_net_input_bytes; /* Bytes read from network. */
    
    uv_timer_t once;
    uv_async_t async;

    void* sendDeQueue;
    PDictList pDictList;

    unsigned long long sendStamp;
    uv_loop_t loop_struct;
} *PNetUdp, NetUdp;

typedef struct _NetClient {
    int flags;
    //udp
    char udp_recv_buf[PACKET_MAX_SIZE_UDP];

    unsigned int id;//client的id
    uv_stream_t* stream;
    PNetUdp pNetUdp;
}*PNetClient, NetClient;

static uv_udp_t sendUdp;

static void freeClient(NetClient* c) {

    listNode* ln;
    /* Remove from the list of clients */
    if (c->stream != 0 && c->id != 0) {
        ln = listSearchKey(c->pNetUdp->clients, c);
        listDelNode(c->pNetUdp->clients, ln);
        dictDelete(c->pNetUdp->id_client, &c->id);
    }

    if (c->stream != 0) {
        if(!uv_is_closing((uv_handle_t*)c->stream))
            uv_close((uv_handle_t*)c->stream, NULL);
        free(c->stream);
        c->stream = 0;
    }

    free(c);
}

static void on_close(uv_handle_t* peer) {
    PNetClient c = uv_handle_get_data(peer);
    freeClient(c);
}

static void after_shutdown(uv_shutdown_t* req, int status) {
    uv_close((uv_handle_t*)req->handle, on_close);
    free(req);
}

static void RecvCallback(PNetClient c, char* buf, size_t buffsize) {

    PProtoHead pProtoHead = (PProtoHead)buf;
    if (c->flags & _UDP_SOCKET) {

        buffsize = pProtoHead->len;
        if (pProtoHead->proto == proto_rpc_create) {
            DockerRandomPushMsg(buf, buffsize);
        }
        else if (pProtoHead->proto == proto_rpc_call) {
            PProtoRPC pProtoRPC = (PProtoRPC)buf;

            idl64 eid;
            eid.u = pProtoRPC->id;
            unsigned int addr = eid.eid.addr;
            unsigned char port = MakeDown(eid.eid.port);

            if (c->pNetUdp->udp_ip == addr && c->pNetUdp->uportOffset == port) {
                DockerPushMsg(eid.eid.dock, buf, buffsize);
            }
            else {
                n_error("RecvCallback error udp addr ip: %u,%u port: %u,%u", c->pNetUdp->udp_ip, addr, c->pNetUdp->uportOffset, port);
            }
        }
        else if (pProtoHead->proto == proto_route_call) {
            //转发给客户端所在的tcp网络
            PProtoRoute pProtoRoute = (PProtoRoute)buf;

            idl64 eid;
            eid.u = pProtoRoute->pid;
            unsigned int addr = eid.eid.addr;
            unsigned char port = MakeDown(eid.eid.port);

            if (c->pNetUdp->udp_ip == addr && c->pNetUdp->uportOffset == port) {
                //找到对应entity id 的client id 转发

            }
        }
    }
}

static void doJsonParseFile(void* pVoid, char* data)
{
    PNetUdp pNetUdp = pVoid;
    cJSON* json = cJSON_Parse(data);
    if (!json) {
        //printf("Error before: [%s]\n", cJSON_GetErrorPtr());	
        return;
    }

    cJSON* logJson = cJSON_GetObjectItem(json, "udp");
    if (logJson) {

        cJSON* item = logJson->child;
        while ((item != NULL) && (item->string != NULL))
        {
            item = item->next;
        }
    }

    cJSON_Delete(json);
}

static void NetRun(void* pVoid) {
    PNetUdp pNetUdp = pVoid;
    uv_run(pNetUdp->loop, UV_RUN_DEFAULT);
    return;
}

static void NetCommand(char* pBuf, PNetUdp pNetUdp) {
    PProtoHead pProtoHead = (PProtoHead)(pBuf);

    if (pProtoHead->proto == proto_net_destory) {

        PProtoNetDestory pProtoNetDestory = (PProtoNetDestory)pProtoHead;

        dictEntry* entry = dictFind(pNetUdp->id_client, (unsigned int*)&pProtoNetDestory->clientId);
        if (entry == NULL) {
            //没有找到客户端id丢弃封包
            n_error("NetCommand::proto_net_destory %i lost packet not find clientid %i", pProtoHead->proto, pProtoNetDestory->clientId);
            return;
        }

        //这里要调用映射层获得映射关系
        NetClient* c = (NetClient*)dictGetVal(entry);
        freeClient(c);
    }
}

static void NetBeatFun(void* pVoid) {

    PNetUdp pNetUdp = pVoid;
    uv_async_send(&pNetUdp->async);
}

static int NetSendToEntity(PNetUdp pNetUdp, unsigned long long entityid, const char* b, unsigned int s) {

    sds sendBuf = sdsnewlen(b, s);

    if (sendBuf == NULL) {
        n_error("NetSendToEntity sdsnewlen is null");
        return 0;
    }

    DqPush(pNetUdp->sendDeQueue, entityid, sendBuf, NetBeatFun, pNetUdp);
    return 1;
}

static int SendLoopFun(unsigned long long id, uv_buf_t* buf, unsigned int len, void* pAram) {
    
    PNetUdp pNetUdp = pAram;
    if (id == 0) {
        for (int i = 0; i < len; i++) {
            NetCommand(buf[i].base, pNetUdp);
            sdsfree(buf[i].base);
        }
        free(buf);        
    } else if (id == 1) {
        uv_stop(pNetUdp->loop);
        n_error("SendLoopFun::uv_stop");

        if (buf) {
            for (int i = 0; i < len; i++) {
                sdsfree(buf[i].base);
            }
            free(buf);
        }
    }

    return 1;
}

static void SendLoop(PNetUdp pNetUdp) {

    listClearNode(pNetUdp->pDictList->IDList);
    pNetUdp->pDictList->len = 0;

    pNetUdp->pDictList = DqPop(pNetUdp->sendDeQueue, pNetUdp->pDictList);
    if (pNetUdp->pDictList->len) {
        DqLoopValue(pNetUdp->sendDeQueue, pNetUdp->pDictList, SendLoopFun, pNetUdp);
    }
}

//触发信号，来至其他线程的指令
static void async_cb(uv_async_t* handle) {
    PNetUdp pNetUdp = uv_handle_get_data((uv_handle_t*)handle);
    SendLoop(pNetUdp);
}

static void slab_alloc(uv_handle_t* handle,
    size_t suggested_size,
    uv_buf_t* buf) {

    PNetClient pNetClient = uv_handle_get_data(handle);
    /* up to 16 datagrams at once */
    buf->base = pNetClient->udp_recv_buf;
    buf->len = sizeof(pNetClient->udp_recv_buf);
}

static void udp_read(uv_udp_t* handle,
    ssize_t nread,
    const uv_buf_t* rcvbuf,
    const struct sockaddr* addr,
    unsigned flags) {

    if (nread == 0) {
        /* Everything OK, but nothing read. */
        return;
    }

    if (nread < 0) {
        n_error("udp::on_recv error %s %s", uv_err_name(nread), uv_strerror(nread));
        return;
    }

    if (addr->sa_family != AF_INET) {
        n_error("udp::on_recv error addr->sa_family != AF_INET");
        return;
    }

    PNetClient c = uv_handle_get_data((uv_handle_t*)handle);
    RecvCallback(c, rcvbuf->base, rcvbuf->len);
}

static uv_udp_t* udp4_start(uv_loop_t* loop, char* ip, int port, struct sockaddr_in* addr) {
    int r;

    r = uv_ip4_addr(ip, port, addr);
    if (r) {
        n_error("udp4_start::Socket uv_ip4_addr error %s %s", uv_err_name(r), uv_strerror(r));
        return NULL;
    }

    uv_udp_t* udpServer = malloc(sizeof(uv_udp_t));
    if (udpServer == NULL) {
        n_error("udp4_start::udpServer malloc error");
        free(udpServer);
        return NULL;
    }

    PNetUdp pNetUdp = (PNetUdp)uv_loop_get_data(loop);

    r = uv_udp_init(loop, udpServer);
    if (r) {
        n_error("udp4_start::Socket creation error %s %s", uv_err_name(r), uv_strerror(r));
        free(udpServer);
        return NULL;
    }

    r = uv_udp_bind(udpServer, (const struct sockaddr*)addr, 0);
    if (r) {
        n_error("udp4_start::Bind error %s %s", uv_err_name(r), uv_strerror(r));
        free(udpServer);
        return NULL;
    }

    PNetClient c = malloc(sizeof(NetClient));
    if (c == NULL) {
        n_error("udp4_start::PNetClient malloc error");
        free(udpServer);
        return NULL;
    }

    c->id = ++pNetUdp->allClientID;
    c->pNetUdp = uv_loop_get_data(loop);
    uv_handle_set_data((uv_handle_t*)udpServer, c);
    c->flags |= _UDP_SOCKET;

    r = uv_udp_recv_start(udpServer, slab_alloc, udp_read);
    if (r) {
        n_error("udp4_start::uv_udp_recv_start error %s %s", uv_err_name(r), uv_strerror(r));
        free(udpServer);
        free(c);
        return NULL;
    }

    return udpServer;
}

static void once_cb(uv_timer_t* handle) {
    PNetUdp pNetUdp = uv_handle_get_data((uv_handle_t*)handle);
    SendLoop(pNetUdp);

    unsigned long long stamp = GetTick();
    if (stamp - pNetUdp->sendStamp > 200) {
        n_error("once_cb update time out %i", stamp - pNetUdp->sendStamp);
    }
    pNetUdp->sendStamp = stamp;
}

void* UdpCreate(char* ip, unsigned short uportBegin, unsigned char uportOffset, unsigned long long* retId, int inOrOut) {
    
    int r = 0;
    PNetUdp pNetUdp = calloc(1, sizeof(NetUdp));
    if (pNetUdp == NULL) {

        n_error("UdpCreate calloc error");
        return NULL;
    }

    pNetUdp->uportBegin = uportBegin;
    pNetUdp->uportOffset = uportOffset;
    pNetUdp->udp_ip = 0;
    pNetUdp->udp_port = uportBegin + uportOffset;
    pNetUdp->allClientID = 0;
    pNetUdp->id_client = dictCreate(DefaultUintPtr(), NULL);
    pNetUdp->clients = listCreate();
    pNetUdp->sendDeQueue = DqCreate();
    pNetUdp->pDictList = DictListCreate();
    pNetUdp->sendStamp = GetTick();

    DoJsonParseFile(pNetUdp, doJsonParseFile);
 
    if (uv_loop_init(&pNetUdp->loop_struct))
        return NULL;
    pNetUdp->loop = &pNetUdp->loop_struct;
    uv_loop_set_data(pNetUdp->loop, pNetUdp);

    r = uv_udp_init(pNetUdp->loop, &sendUdp);
    if (r) {
        n_error("NetCreate::uv_udp_init error %s %s", uv_err_name(r), uv_strerror(r));
        return NULL;
    }

    struct sockaddr_in addr;
    pNetUdp->udp4 = udp4_start(pNetUdp->loop, ip, pNetUdp->udp_port, &addr);
    if (pNetUdp->udp4 == NULL) {
        return NULL;
    }

    pNetUdp->udp_ip = ntohl(addr.sin_addr.s_addr);
    if (pNetUdp->udp_ip == 0) {
        uv_os_fd_t socket;
        uv_fileno((uv_handle_t*)pNetUdp->udp4, &socket);
        IntConfigPrimary((unsigned long long)socket, &pNetUdp->udp_ip, &pNetUdp->udp_port);
        if (pNetUdp->udp_ip == 0 || pNetUdp->udp_port == 0) {
            n_error("InitServer:: udp_ip or udp_port is empty! ip:%i port:%i", pNetUdp->udp_ip, pNetUdp->udp_port);
        }
    }

    idl64 eid;
    eid.u = 0;
    eid.eid.addr = pNetUdp->udp_ip;
    eid.eid.port = MakeUp(0, pNetUdp->udp_port - pNetUdp->uportBegin);
    *retId = eid.u;

    if(inOrOut){
        SetUpdIPAndPortToInside(pNetUdp->udp_ip, pNetUdp->udp_port);
    }
    else {
        SetUpdIPAndPortToOutside(pNetUdp->udp_ip, pNetUdp->udp_port);
    }

    n_details("InitServer::udp_port on:%i", pNetUdp->udp_port);

    //init async for send
    pNetUdp->async.data = pNetUdp;
    r = uv_async_init(pNetUdp->loop, &pNetUdp->async, async_cb);
    if (r) {
        n_error("UVNetCreate::uv_async_init error %s %s", uv_err_name(r), uv_strerror(r));
        return NULL;
    }

    r = uv_timer_init(pNetUdp->loop, &pNetUdp->once);
    if (r) {
        n_error("UVNetCreate::uv_async_init error %s %s", uv_err_name(r), uv_strerror(r));
        return NULL;
    }
    uv_handle_set_data((uv_handle_t*)&pNetUdp->once, pNetUdp);

    r = uv_timer_start(&pNetUdp->once, once_cb, 0, 100);
    if (r) {
        n_error("UVNetCreate::uv_async_init error %s %s", uv_err_name(r), uv_strerror(r));
        return NULL;
    }

    uv_thread_create(&pNetUdp->thread, NetRun, pNetUdp);

    return pNetUdp;
}

static void ClientsDestroyFun(void* value)
{
    freeClient((NetClient*)value);
}

int NetSendToCancel(PNetUdp pNetUdp) {

    sds sendBuf = sdsempty();
    DqPush(pNetUdp->sendDeQueue, 1, sendBuf, NetBeatFun, pNetUdp);
    return 1;
}

void UdpDestory(void* pVoid) {
    PNetUdp pNetUdp = pVoid;
    NetSendToCancel(pNetUdp);
    uv_thread_join(&pNetUdp->thread);

    uv_close((uv_handle_t*)&sendUdp, NULL);
    uv_close((uv_handle_t*)&pNetUdp->once, NULL);

    listSetFreeMethod(pNetUdp->clients, ClientsDestroyFun);
    listRelease(pNetUdp->clients);
    dictRelease(pNetUdp->id_client);
    DqDestory(pNetUdp->sendDeQueue);
    DictListDestory(pNetUdp->pDictList);

    uv_close((uv_handle_t*)pNetUdp->udp4, NULL);
    free(pNetUdp->udp4);

    uv_close((uv_handle_t*)&pNetUdp->async, NULL);
    free(pNetUdp);

}

static void NetUDPAddr2(PNetUdp pNetUdp, unsigned int* ip, unsigned char* uportOffset, unsigned short* uport) {
    if (pNetUdp == NULL) {
        return;
    }

    *ip = pNetUdp->udp_ip;
    *uportOffset = pNetUdp->uportOffset;
    *uport = pNetUdp->uportBegin;
}

/*
最稳妥的办法是通过发送队列发送udp封包
*/
void UdpSendTo(char* ip, unsigned short port, unsigned char* b, unsigned int s) {
    struct sockaddr_in server_addr;
    uv_buf_t buf;
    int r;

    if (s > PACKET_MAX_SIZE_UDP) {
        n_error("UdpSendTo:: error Length greater than limit PACKET_MAX_SIZE_UDP %i", s);
        return;
    }

    r = uv_ip4_addr(ip, port, &server_addr);
    
    if (r) {
        n_error("UdpSendTo::uv_ip4_addr error %s %s", uv_err_name(r), uv_strerror(r));
        return;
    }

    buf = uv_buf_init(b, s);
    r = uv_udp_try_send(&sendUdp, &buf, 1, (const struct sockaddr*)&server_addr);

    if (r < 0) {
        n_error("UdpSendTo::uv_udp_try_send error %s %s", uv_err_name(r), uv_strerror(r));
    }
}

/*
最稳妥的办法是通过发送队列发送udp封包
*/

void UdpSendToWithUINT(unsigned int ip, unsigned short port, unsigned char* b, unsigned int s) {
    struct sockaddr_in server_addr;
    uv_buf_t buf;
    int r;

    if (s > PACKET_MAX_SIZE_UDP) {
        n_error("UdpSendToWithUINT::NetSendToNode error Length greater than limit PACKET_MAX_SIZE_UDP %i", s);
        return;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    unsigned int out_ip = htonl(ip);
    memcpy(&(server_addr.sin_addr.s_addr), &out_ip, sizeof(unsigned int));

    buf = uv_buf_init(b, s);
    r = uv_udp_try_send(&sendUdp, &buf, 1, (const struct sockaddr*)&server_addr);

    if (r < 0) {
        n_error("UdpSendToWithUINT::uv_udp_try_send error %s %s", uv_err_name(r), uv_strerror(r));
    }
}