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
#include "uvnettcp.h"
#include "configfile.h"

#define _BINDADDR_MAX 16

#define _TCP_SOCKET (1<<1)
#define _TCP_CONNECT (1<<2)

#define _TCP4_SOCKET (1<<0)      /* Client is in Pub/Sub mode. */
#define _TCP6_SOCKET (1<<1)


typedef struct {
    uv_write_t req;
    uv_buf_t* buf;
    unsigned int len;
} write_req_t;

typedef struct {
    uv_write_t req;
    uv_buf_t buf;
} write_req_t2;


typedef struct _WorkServer {
    sds         entityObj;                 //tcp链接绑定的entity对象默认为“account”,由配置文件配置
    sds         botsObj;

    uv_loop_t uv_loop;
    uv_loop_t* loop;
    uv_thread_t thread;

    unsigned int allClientID;   //client计数器
    dict* id_client;            //clientid和client对象的对应关系
    list* clients;              /* List of active clients */
    unsigned int workId;        //每个work的id

    dict* entity_client;
    unsigned long long stat_net_input_bytes; /* Bytes read from network. */

    uv_timer_t once;
    sds tcp_recv_buf;//缓冲区
    uv_async_t async;

    void* sendDeQueue;
    PDictList pDictList;
    unsigned long long sendStamp;

    void* pTcp;
    uv_tcp_t* tcpHandle;
} *PWorkServer, WorkServer;

typedef struct _NetClient {
    int flags;
    unsigned int clientId;//client的id
    unsigned int dockerid;
    unsigned long long entityId;//对应entiy的id

    uv_stream_t* stream;
    PWorkServer pWorkServer;

    char* halfBuf;
}*PNetClient, NetClient;

static void freeClient(NetClient* c) {

    listNode* ln;
    /* Remove from the list of clients */
    if (c->stream != 0 && c->clientId != 0) {
        ln = listSearchKey(c->pWorkServer->clients, c);
        listDelNode(c->pWorkServer->clients, ln);
        dictDelete(c->pWorkServer->id_client, &c->clientId);
        if (c->entityId != 0)
            dictDelete(c->pWorkServer->entity_client, (void*)&c->entityId);
    }

    //如果在entityId创建过程中或者切换过程中销毁会导致entity对象的残留。
    if ((c->flags & _TCP_SOCKET || c->flags & _TCP_CONNECT) && c->entityId != 0) {
        int len = sizeof(ProtoRPC);
        PProtoRPC pProtoRPC = malloc(len);
        pProtoRPC->id = c->entityId;

        PProtoHead pProtoHead = (PProtoHead)pProtoRPC;
        pProtoHead->len = len;
        pProtoHead->proto = proto_rpc_destory;        
        DockerPushMsg(c->dockerid, (unsigned char*)pProtoHead, len);
        free(pProtoRPC);
    }

    if (c->stream != 0) {
        if(!uv_is_closing((uv_handle_t*)c->stream))
            uv_close((uv_handle_t*)c->stream, NULL);
        free(c->stream);
        c->stream = 0;
    }

    /* Release other dynamically allocated client structure fields,
     * and finally release the client structure itself. */
    free(c);
}

static void tcp_alloc(uv_handle_t* handle,
    size_t suggested_size,
    uv_buf_t* buf) {
    PNetClient c = uv_handle_get_data(handle);

    size_t len = sdslen(c->pWorkServer->tcp_recv_buf);
    if (suggested_size > sdslen(c->pWorkServer->tcp_recv_buf)) {
        size_t add = suggested_size - len;
        c->pWorkServer->tcp_recv_buf = sdsMakeRoomFor(c->pWorkServer->tcp_recv_buf, add);
        sdsIncrLen(c->pWorkServer->tcp_recv_buf, add);
    }

    buf->base = c->pWorkServer->tcp_recv_buf;
    buf->len = suggested_size;
}

static void on_close(uv_handle_t* peer) {
    PNetClient c = uv_handle_get_data(peer);
    freeClient(c);
}

static void after_shutdown(uv_shutdown_t* req, int status) {
    uv_close((uv_handle_t*)req->handle, on_close);
    free(req);
}

static void after_write(uv_write_t* req, int status) {
    write_req_t* wr;
    /* Free the read/write buffer and the request */
    wr = (write_req_t*)req;

    for (size_t i = 0; i < wr->len; i++)
    {
        sdsfree(wr->buf[i].base);
    }

    free(wr->buf);
    free(wr);

    if (status == 0)
        return;
    n_error("uv_write error: %s - %s", uv_err_name(status), uv_strerror(status));
}

//转发的情况直接发送
static void after_write2(uv_write_t* req, int status) {
    write_req_t* wr;

    /* Free the read/write buffer and the request */
    wr = (write_req_t*)req;
    sdsfree(wr->req.data);
    free(wr);

    if (status == 0)
        return;
    n_error("uv_write error: %s - %s", uv_err_name(status), uv_strerror(status));
}

static void RecvCallback(PNetClient c, char* buf, size_t buffsize) {

    PProtoHead pProtoHead = (PProtoHead)buf;
    if (c->flags & _TCP_SOCKET || c->flags & _TCP_CONNECT) {
        if (pProtoHead->proto == proto_route_call) {
            //因为机器人的存在，这里会存在node <=> client
            //虽然工程上说是对等的，但因为安全的原因，客户端的npc不能调用服务器的npc方法

            if (c->entityId != 0) {
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

                idl64 eid;
                eid.u = pProtoRoute->pid;
                DockerPushMsg(eid.eid.dock, buf, buffsize);
            }
            else {
                n_error("Client is initializing!")
            }
        }
        else if (pProtoHead->proto == proto_packet) {
            PProtoRPC pProtoRPC = (PProtoRPC)buf;
            pProtoRPC->id = c->entityId;//替换掉对应的id

            idl64 eid;
            eid.u = pProtoRPC->id;

            DockerPushMsg(eid.eid.dock, buf, buffsize);
        }
        else if (pProtoHead->proto == proto_run_lua) {
            PProtoRunLua pProtoRunLua = (PProtoRunLua)buf;
            DockerRunScript("", 0, pProtoRunLua->dockerid, pProtoRunLua->luaString, pProtoRunLua->protoHead.len - sizeof(PProtoRunLua));
        }
        else {
            n_error("Discard the error packet!");
            uv_close((uv_handle_t*)c->stream, on_close);
        }
    }
}

//tcp收到封包
static void tcp_read(uv_stream_t* handle,
    ssize_t nread,
    const uv_buf_t* buf)
{
    PNetClient c = uv_handle_get_data((uv_handle_t*)handle);
    //n_details("tcp_read stream:;%i client::%i", handle, c->id);
    if (nread < 0) {
        /* Error or EOF */
        n_error("tcp_read error %s %s", uv_err_name(nread), uv_strerror(nread));

        //启动销毁流程
        uv_shutdown_t* sreq = malloc(sizeof * sreq);
        int r = uv_shutdown(sreq, handle, after_shutdown);
        if (r) {
            n_error("Socket uv_shutdown error %s %s", uv_err_name(r), uv_strerror(r));
            return;
        }
        return;
    }if (nread == 0) {
        n_error("Client closed connection");
        freeClient(c);
        return;
    }

    ssize_t readSize = 0;
    char* pos = 0;
    if (c->halfBuf != 0) {
        int remainLen = 0;
        ssize_t len = *(unsigned int*)c->halfBuf;
        if (len > nread + sdslen(c->halfBuf)) {
            memcpy(c->halfBuf + sdslen(c->halfBuf), buf->base, nread);
            sdsinclen(c->halfBuf, nread);
            return;
        }

        remainLen = len - sdslen(c->halfBuf);
        memcpy(c->halfBuf + sdslen(c->halfBuf), buf->base, remainLen);
        sdsinclen(c->halfBuf, remainLen);

        RecvCallback(c, c->halfBuf, len);
        sdsfree(c->halfBuf);
        c->halfBuf = 0;

        readSize = remainLen;
        if (nread <= readSize) {
            return;
        }
        pos = buf->base + remainLen;
    }
    else {
        pos = buf->base;
    }

    while (true) {
        ssize_t len = *(unsigned int*)pos;

        if (len > (nread - readSize)) {
            int remainLen = nread - readSize;
            c->halfBuf = sdsMakeRoomFor(sdsempty(),len);
            if (c->halfBuf == 0) {
                n_error("tcp_read::sdsMakeRoomFor c->halfBuf ");
                //需要加一个封包校验标志
                break;
            }
            memcpy(c->halfBuf, pos, remainLen);
            sdsIncrLen(c->halfBuf, remainLen);
            break;
        } 
        RecvCallback(c, pos, len);
        readSize += len;
        pos += len;

        if (nread <= readSize) {
            break;
        }
    }
}

static unsigned int RandomDividend(unsigned int dividend, unsigned int merchant, unsigned int mod) {
    unsigned int divisor = dividend / merchant;
    unsigned int remainder = dividend % merchant;
    if (remainder && remainder > mod) {
        divisor++;
    }

    if (divisor == 0) {
        n_error("dividend must be greater than merchant");
        return 0;
    }

    unsigned int r = 0;
    r = rand() % divisor;
    r = merchant * r + mod;
    return r;
}

static void NetRun(void* pVoid) {
    PWorkServer pWorkServer = pVoid;
    uv_run(pWorkServer->loop, UV_RUN_DEFAULT);
    return;
}

static void connect_cb(uv_connect_t* req, int status) {

    if (status != 0) {
        free(req);
        n_error("connect_cb error: %s - %s", uv_err_name(status), uv_strerror(status));
        return;
    }

    NetClient* c = req->data;
    free(req);

    dictAddWithUint(c->pWorkServer->id_client, c->clientId, c);
    listAddNodeTail(c->pWorkServer->clients, c);

    if (c->flags & _TCP_CONNECT) {
        //这里调用绑定协议尝试创建entity
        mp_buf* pmp_buf = mp_buf_new();
        mp_encode_bytes(pmp_buf, c->pWorkServer->botsObj, sdslen(c->pWorkServer->botsObj));
        mp_encode_map(pmp_buf, 2);
        mp_encode_bytes(pmp_buf, "isconnect", strlen("isconnect"));
        mp_encode_int(pmp_buf, 1);
        mp_encode_bytes(pmp_buf, "clientid", strlen("clientid"));

        idl32 id;
        id.id.work = c->pWorkServer->workId;
        id.id.client = c->clientId;

        mp_encode_int(pmp_buf, id.u);

        unsigned int len = sizeof(ProtoRPCCreate) + pmp_buf->len;
        PProtoHead pProtoHead = malloc(len);
        pProtoHead->len = len;
        pProtoHead->proto = proto_rpc_create;

        PProtoRPCCreate protoRPCCreate = (PProtoRPCCreate)pProtoHead;
        memcpy(protoRPCCreate->callArg, pmp_buf->b, pmp_buf->len);

        c->dockerid = RandomDividend(DockerSize(), TcpWorkerCount(c->pWorkServer->pTcp), c->pWorkServer->workId);
        DockerPushMsg(c->dockerid, (unsigned char*)pProtoHead, len);
    }


    int r = uv_read_start(c->stream, tcp_alloc, tcp_read);
    if (r) {
        n_error("Socket read_start error %s %s", uv_err_name(r), uv_strerror(r));
        freeClient(c);
        return;
    }
}

static void NetCommand(char* pBuf, PWorkServer pWorkServer) {
    PProtoHead pProtoHead = (PProtoHead)(pBuf);

    if (pProtoHead->proto == proto_net_bind) {
        PProtoNetBind pProtoNetBind = (PProtoNetBind)pProtoHead;

        idl32 id;
        id.u = pProtoNetBind->clientId;
        unsigned int idint = id.id.client;

        dictEntry* entry = dictFind(pWorkServer->id_client, (unsigned int*)&idint);
        if (entry == NULL) {
            //没有找到客户端id丢弃封包
            n_error("NetCommand::proto_net_bind %i lost packet not find clientid %i", pProtoHead->proto, pProtoNetBind->clientId);
            return;
        }

        //这里要调用映射层获得映射关系
        NetClient* c = (NetClient*)dictGetVal(entry);
        c->entityId = pProtoNetBind->entityId;

        entry = dictFind(pWorkServer->entity_client, (void*)&c->entityId);
        if (entry != NULL)
            dictDelete(pWorkServer->entity_client, (void*)&c->entityId);
        dictAddWithLonglong(pWorkServer->entity_client, c->entityId, c);
    }
    else if (pProtoHead->proto == proto_net_destory) {

        PProtoNetDestory pProtoNetDestory = (PProtoNetDestory)pProtoHead;

        dictEntry* entry = dictFind(pWorkServer->id_client, (unsigned int*)&pProtoNetDestory->clientId);
        if (entry == NULL) {
            //没有找到客户端id丢弃封包
            n_error("NetCommand::proto_net_destory %i lost packet not find clientid %i", pProtoHead->proto, pProtoNetDestory->clientId);
            return;
        }

        //这里要调用映射层获得映射关系
        NetClient* c = (NetClient*)dictGetVal(entry);
        freeClient(c);
    }
    else if (pProtoHead->proto == proto_net_connect) {

        PProtoConnect pProtoConnect = (PProtoConnect)pProtoHead;
        struct sockaddr_in addr;

        int r = uv_ip4_addr(pProtoConnect->ip, pProtoConnect->port, &addr);
        if (r) {
            n_error("NetCommand::uv_ip4_addr error: %s - %s", uv_err_name(r), uv_strerror(r));
            return;
        }

        uv_connect_t* connect_req = malloc(sizeof(uv_connect_t));
        if (connect_req == NULL)return;

        PNetClient c = calloc(1, sizeof(NetClient));
        if (c == NULL)return;

        c->stream = malloc(sizeof(uv_tcp_t));
        if (c->stream == NULL)return;

        connect_req->data = c;
        /* associate server with stream */
        c->stream->data = c;
        c->pWorkServer = pWorkServer;
        c->clientId = ++pWorkServer->allClientID;
        c->flags |= _TCP_CONNECT;
        c->halfBuf = 0;

        r = uv_tcp_init(pWorkServer->loop, (uv_tcp_t*)c->stream);
        if (r) {
            n_error("NetCommand::uv_ip4_addr error: %s - %s", uv_err_name(r), uv_strerror(r));
            free(connect_req);
            free(c->stream);
            free(c);
            return;
        }

        r = uv_tcp_connect(connect_req,
            (uv_tcp_t*)c->stream,
            (const struct sockaddr*)&addr,
            connect_cb);
        if (r) {
            n_error("uv_tcp_connect error: %s - %s", uv_err_name(r), uv_strerror(r));
            uv_close((uv_handle_t*)c->stream, NULL);
            free(connect_req);
            free(c->stream);
            free(c);
        }
    }
}

static void NetBeatFun(void* pVoid) {
    PWorkServer pWorkServer = pVoid;
    uv_async_send(&pWorkServer->async);
}

int TcpWorkSendToEntity(void* pVoid, unsigned long long entityid, const char* b, unsigned int s) {
    PWorkServer pWorkServer = pVoid;
    sds sendBuf = sdsnewlen(b, s);

    if (sendBuf == NULL) {
        n_error("NetSendToEntity sdsnewlen is null");
        return 0;
    }

    DqPush(pWorkServer->sendDeQueue, entityid, sendBuf, NetBeatFun, pWorkServer);
    return 1;
}

static int SendLoopFun(unsigned long long id, uv_buf_t* buf, unsigned int len, void* pAram) {
    PWorkServer pWorkServer = pAram;
    if (id == 0) {
        for (int i = 0; i < len; i++) {
            NetCommand(buf[i].base, pWorkServer);
            sdsfree(buf[i].base);
        }
        free(buf);        
    } else if (id == 1) {
        uv_stop(pWorkServer->loop);
        n_error("SendLoopFun::uv_stop");

        if (buf) {
            for (int i = 0; i < len; i++) {
                sdsfree(buf[i].base);
            }
            free(buf);
        }
    }
    else {
        dictEntry* entry = dictFind(pWorkServer->entity_client, (unsigned int*)&id);
        if (entry == NULL) {
            //没有找到客户端id丢弃封包
            n_error("PipeHandler::entity_client lost packet not find entity_client %U", id);
            return 0;
        }
        else {
            NetClient* c = (NetClient*)dictGetVal(entry);

            write_req_t* wr = (write_req_t*)malloc(sizeof(write_req_t));
            if (wr == NULL) return 0;
            wr->buf = buf;
            wr->len = len;

            int r = uv_write(&wr->req, c->stream, buf, len, after_write);
            if (r < 0) {
                n_error("async_cb::uv_write failed error %s %s", uv_err_name(r), uv_strerror(r));
            }
        }
    }

    return 1;
}

static void SendLoop(PWorkServer pWorkServer) {

    listClearNode(pWorkServer->pDictList->IDList);
    pWorkServer->pDictList->len = 0;

    pWorkServer->pDictList = DqPop(pWorkServer->sendDeQueue, pWorkServer->pDictList);
    if (pWorkServer->pDictList->len) {
        DqLoopValue(pWorkServer->sendDeQueue, pWorkServer->pDictList, SendLoopFun, pWorkServer);
    }
}

//触发信号，来至其他线程的指令
static void async_cb(uv_async_t* handle) {
    PWorkServer pWorkServer = uv_handle_get_data((uv_handle_t*)handle);
    SendLoop(pWorkServer);
}

static void once_cb(uv_timer_t* handle) {
    PWorkServer pWorkServer = uv_handle_get_data((uv_handle_t*)handle);
    SendLoop(pWorkServer);

    unsigned long long stamp = GetTick();
    if (stamp - pWorkServer->sendStamp > 200) {
        n_error("once_cb update time out %i", stamp - pWorkServer->sendStamp);
    }
    pWorkServer->sendStamp = stamp;
}

static void on_connection(uv_stream_t* server, int status) {

    int r;
    PWorkServer pWorkServer = uv_handle_get_data((uv_handle_t*)server);
    if (status != 0) {
        n_error("Connect error %s %s", uv_err_name(status), uv_strerror(status));
        return;
    }

    //创建client对象，创建接收数据缓冲区（只大不小），添加到当前链接队列列表
    PNetClient c = calloc(1, sizeof(NetClient));

    if (c == NULL) {
        n_error("on_connection calloc c");
        return;
    }

    c->stream = malloc(sizeof(uv_tcp_t));

    if (c->stream == NULL) {
        n_error("on_connection malloc stream");
        return;
    }

    r = uv_tcp_init(pWorkServer->loop, (uv_tcp_t*)c->stream);
    if (r) {
        n_error("tcpwork accept error %s %s", uv_err_name(r), uv_strerror(r));
        free(c->stream);
        free(c);
        return;
    }

    /* associate server with stream */
    c->stream->data = c;
    c->pWorkServer = pWorkServer;
    c->clientId = ++pWorkServer->allClientID;
    c->flags |= _TCP_SOCKET;
    c->halfBuf = 0;

    r = uv_accept(server, c->stream);
    if (r) {
        n_error("tcpwork accept error %s %s", uv_err_name(r), uv_strerror(r));
        free(c);
        return;
    }

    r = uv_read_start(c->stream, tcp_alloc, tcp_read);
    if (r) {
        n_error("tcpwork read_start error %s %s", uv_err_name(r), uv_strerror(r));
        uv_close((uv_handle_t*)&c->stream, NULL);
        free(c);
        return;
    }

    dictAddWithUint(pWorkServer->id_client, c->clientId, c);
    listAddNodeTail(pWorkServer->clients, c);

    if (c->flags & _TCP_SOCKET) {
        //这里调用绑定协议尝试创建entity
        mp_buf* pmp_buf = mp_buf_new();
        mp_encode_bytes(pmp_buf, pWorkServer->entityObj, sdslen(pWorkServer->entityObj));
        mp_encode_map(pmp_buf, 1);
        mp_encode_bytes(pmp_buf, "clientid", strlen("clientid"));

        idl32 id;
        id.id.work = pWorkServer->workId;
        id.id.client = c->clientId;

        mp_encode_int(pmp_buf, id.u);

        unsigned int len = sizeof(ProtoRPCCreate) + pmp_buf->len;
        PProtoHead pProtoHead = malloc(len);
        pProtoHead->len = len;
        pProtoHead->proto = proto_rpc_create;

        PProtoRPCCreate protoRPCCreate = (PProtoRPCCreate)pProtoHead;
        memcpy(protoRPCCreate->callArg, pmp_buf->b, pmp_buf->len);
        c->dockerid = RandomDividend(DockerSize(), TcpWorkerCount(pWorkServer->pTcp), pWorkServer->workId);
        DockerPushMsg(c->dockerid, (unsigned char*)pProtoHead, len);
    }
}

static uv_tcp_t* tcp6_start(uv_loop_t* loop, char* ip, int port) {
    struct sockaddr_in6 addr6;
    int r;

    r = uv_ip6_addr(ip, port, &addr6);
    if (r) {
        n_error("tcp6_start::Socket uv_ip6_addr error %s %s", uv_err_name(r), uv_strerror(r));
        return NULL;
    }

    uv_tcp_t* tcpHandle = malloc(sizeof(uv_tcp_t));
    r = uv_tcp_init(loop, tcpHandle);
    if (r) {
        n_error("tcp6_start::Socket creation error %s %s", uv_err_name(r), uv_strerror(r));
        free(tcpHandle);
        return NULL;
    }

    r = uv_tcp_bind(tcpHandle, (const struct sockaddr*)&addr6, 0);
    if (r) {
        n_error("tcp6_start::Bind error %s %s", uv_err_name(r), uv_strerror(r));
        free(tcpHandle);
        return NULL;
    }

    uv_handle_set_data((uv_handle_t*)tcpHandle, uv_loop_get_data(loop));

    r = uv_listen((uv_stream_t*)tcpHandle, SOMAXCONN, on_connection);
    if (r) {
        n_error("tcp6_start::Listen error %s %s", uv_err_name(r), uv_strerror(r));
        free(tcpHandle);
        return NULL;
    }

    n_details("InitServer::tcp6_port on: %s %i", ip, port);

    return tcpHandle;
}

static uv_tcp_t* tcp4_start(uv_loop_t* loop, char* ip, int port) {
    struct sockaddr_in addr;
    int r;

    r = uv_ip4_addr(ip, port, &addr);
    if (r) {
        n_error("tcp4_start::Socket uv_ip4_addr error %s %s", uv_err_name(r), uv_strerror(r));
        return NULL;
    }

    uv_tcp_t* tcpHandle = malloc(sizeof(uv_tcp_t));
    r = uv_tcp_init(loop, tcpHandle);
    if (r) {
        n_error("tcp4_start::Socket creation error %s %s", uv_err_name(r), uv_strerror(r));
        free(tcpHandle);
        return NULL;
    }

    r = uv_tcp_bind(tcpHandle, (const struct sockaddr*)&addr, 0);
    if (r) {
        n_error("tcp4_start::Bind error %s %s", uv_err_name(r), uv_strerror(r));
        free(tcpHandle);
        return NULL;
    }

    uv_handle_set_data((uv_handle_t*)tcpHandle, uv_loop_get_data(loop));

    r = uv_listen((uv_stream_t*)tcpHandle, SOMAXCONN, on_connection);
    if (r) {
        n_error("tcp4_start::Listen error %s %s", uv_err_name(r), uv_strerror(r));
        free(tcpHandle);
        return NULL;
    }

    n_details("InitServer::tcp4_port on: %s %i", ip, port);

    return tcpHandle;
}

static void alloc_buffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
    buf->base = realloc(buf->base, suggested_size);
    buf->len = suggested_size;
}

static void doJsonParseFile(void* pVoid, char* data)
{
    PWorkServer pWorkServer = pVoid;
    cJSON* json = cJSON_Parse(data);
    if (!json) {
        //printf("Error before: [%s]\n", cJSON_GetErrorPtr());	
        return;
    }

    cJSON* logJson = cJSON_GetObjectItem(json, "tcpWork");
    if (logJson) {

        cJSON* item = logJson->child;
        while ((item != NULL) && (item->string != NULL))
        {
            if (strcmp(item->string, "entityObj") == 0) {
                sdsfree(pWorkServer->entityObj);
                pWorkServer->entityObj = sdsnew(cJSON_GetStringValue(item));
            }
            else if (strcmp(item->string, "botsObj") == 0) {
                sdsfree(pWorkServer->botsObj);
                pWorkServer->botsObj = sdsnew(cJSON_GetStringValue(item));
            }
            item = item->next;
        }
    }

    cJSON_Delete(json);
}

void* TcpWorkCreate(unsigned int workId, void* pVoid, unsigned int family, char* ip, unsigned short port) {
    
    int r = 0;
    PWorkServer pWorkServer = calloc(1, sizeof(WorkServer));
    if (pWorkServer == NULL) {
        n_error("TcpWorkCreate calloc PWorkServer errror");
        return NULL;
    }

    pWorkServer->entityObj = sdsnew("client");
    pWorkServer->botsObj = sdsnew("bots");
    pWorkServer->allClientID = 0;
    pWorkServer->workId = workId;
    pWorkServer->id_client = dictCreate(DefaultUintPtr(), NULL);
    pWorkServer->clients = listCreate();
    pWorkServer->entity_client = dictCreate(DefaultLonglongPtr(), NULL);
    pWorkServer->tcp_recv_buf = sdsempty();
    pWorkServer->sendDeQueue = DqCreate();
    pWorkServer->pDictList = DictListCreate();
    pWorkServer->sendStamp = GetTick();
    pWorkServer->pTcp = pVoid;

    DoJsonParseFile(pWorkServer, doJsonParseFile);

    if (uv_loop_init(&pWorkServer->uv_loop)) {
        n_error("TcpWorkCreate uv_loop_init errror");
        return NULL;
    }    

    pWorkServer->loop = &pWorkServer->uv_loop;
    uv_loop_set_data(pWorkServer->loop, pWorkServer);

    //init
    if (family == AF_INET)
        pWorkServer->tcpHandle = tcp4_start(pWorkServer->loop, ip, port);
    else if(family == AF_INET6)
        pWorkServer->tcpHandle = tcp6_start(pWorkServer->loop, ip, port);

    //init async for send
    pWorkServer->async.data = pWorkServer;
    r = uv_async_init(pWorkServer->loop, &pWorkServer->async, async_cb);
    if (r) {
        n_error("UVNetCreate::uv_async_init error %s %s", uv_err_name(r), uv_strerror(r));
        return NULL;
    }
    uv_handle_set_data((uv_handle_t*)&pWorkServer->async, pWorkServer);

    r = uv_timer_init(pWorkServer->loop, &pWorkServer->once);
    if (r) {
        n_error("UVNetCreate::uv_async_init error %s %s", uv_err_name(r), uv_strerror(r));
        return NULL;
    }
    uv_handle_set_data((uv_handle_t*)&pWorkServer->once, pWorkServer);

    r = uv_timer_start(&pWorkServer->once, once_cb, 0, 100);
    if (r) {
        n_error("UVNetCreate::uv_async_init error %s %s", uv_err_name(r), uv_strerror(r));
        return NULL;
    }

    uv_thread_create(&pWorkServer->thread, NetRun, pWorkServer);
    return pWorkServer;
}

static void ClientsDestroyFun(void* value)
{
    freeClient((NetClient*)value);
}

static int NetSendToCancel(PWorkServer pWorkServer) {

    sds sendBuf = sdsempty();
    DqPush(pWorkServer->sendDeQueue, 1, sendBuf, NetBeatFun, pWorkServer);
    return 1;
}

void TcpWorkDestory(void* pVoid) {

    PWorkServer pWorkServer = pVoid;
    NetSendToCancel(pWorkServer);
    uv_thread_join(&pWorkServer->thread);
    uv_close((uv_handle_t*)pWorkServer->tcpHandle, NULL);
    free(pWorkServer->tcpHandle);
    uv_close((uv_handle_t*)&pWorkServer->once, NULL);

    listSetFreeMethod(pWorkServer->clients, ClientsDestroyFun);
    listRelease(pWorkServer->clients);
    dictRelease(pWorkServer->id_client);
    sdsfree(pWorkServer->entityObj);
    sdsfree(pWorkServer->botsObj);
    dictRelease(pWorkServer->entity_client);
    DqDestory(pWorkServer->sendDeQueue);
    DictListDestory(pWorkServer->pDictList);

    /* Free the query buffer */ 
    sdsfree(pWorkServer->tcp_recv_buf);
    uv_loop_close(pWorkServer->loop);
    free(pWorkServer);
}
