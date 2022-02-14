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
#include "uvnet.h"
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

#define _BINDADDR_MAX 16
#define PACKET_MAX_SIZE_UDP					1472

#define _UDP_SOCKET (1<<0)      /* Client is in Pub/Sub mode. */
#define _TCP_SOCKET (1<<1)
#define _TCP_CONNECT (1<<2)

#define _TCP4_SOCKET (1<<0)      /* Client is in Pub/Sub mode. */
#define _TCP6_SOCKET (1<<1)

typedef struct _NetServer {
    int hz;
    unsigned int maxclients;               /* Max number of simultaneous clients */
    unsigned short uport;                  /*  UDP listening port */
    unsigned char uportOffset;             /* offset UDP listening port */
    unsigned int udp_ip;                   /* udp_port = uport + uportOffset 最终监听的ip和端口*/
    unsigned short udp_port;               /* prot */
    unsigned short port;                   /* TCP listening port */

    char* bindaddr[_BINDADDR_MAX];         /* Addresses we should bind to */
    int bindaddr_count;                    /* Number of addresses in server.bindaddr[] */
    uv_tcp_t* tcp6_handle[_BINDADDR_MAX];
    int tcp6_handle_count;
    uv_tcp_t* tcp4_handle[_BINDADDR_MAX];
    int tcp4_handle_count;
    uv_udp_t* udp4;
    uv_udp_t sendUdp;

    sds         entityObj;                 //tcp链接绑定的entity对象默认为“account”,由配置文件配置
    sds         botsObj;

    int         nodetype;                  //节点类型，目前有对内，对外对内，需要加入特殊类型。例如全局服务器专用节点，存储服务专用节点，指定某个服务专用节点。

    uv_loop_t* loop;
    uv_thread_t thread;

    unsigned int allClientID;   //client计数器
    dict* id_client;            //clientid和client对象的对应关系
    list* clients;              /* List of active clients */

    //entity和客户端tcp的对应关系
    //应用于转发到客户端的协议
    dict* entity_client;

    unsigned long long stat_net_input_bytes; /* Bytes read from network. */
    uv_async_t async;
    list* sendBuf;
    void* sendBufMutexHandle;

    uv_timer_t once;

    unsigned int bindListenIp; //指定默认绑定的地址类型4 or 6
} *PNetServer, NetServer;

typedef struct _NetClient {
    sds tcp_recv_buf;//缓冲区
    int flags;

    //udp
    char udp_recv_buf[PACKET_MAX_SIZE_UDP];

    unsigned int id;//client的id
    unsigned int dockerid;
    unsigned long long entityId;//对应entiy的id

    uv_stream_t* stream;
    PNetServer pNetServer;

}*PNetClient, NetClient;

typedef struct {
    uv_write_t req;
    uv_buf_t buf;
} write_req_t;

PNetServer _pNetServer;

static void freeClient(NetClient* c) {

    /* Free the query buffer */
    sdsfree(c->tcp_recv_buf);

    listNode* ln;
    /* Remove from the list of clients */
    if (c->stream != 0 && c->id != 0) {
        ln = listSearchKey(c->pNetServer->clients, c);
        listDelNode(c->pNetServer->clients, ln);
        dictDelete(c->pNetServer->id_client, &c->id);
        if (c->entityId != 0)
            dictDelete(c->pNetServer->entity_client, (void*)&c->entityId);
    }

    //如果在entityId创建过程中或者切换过程中销毁会导致entity对象的残留。
    if (c->flags & _TCP_SOCKET || c->flags & _TCP_CONNECT) {

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

    if (suggested_size > sdslen(c->tcp_recv_buf)) {
        c->tcp_recv_buf = sdsMakeRoomFor(c->tcp_recv_buf, suggested_size);
        sdsIncrLen(c->tcp_recv_buf, suggested_size);
    }

    buf->base = c->tcp_recv_buf;
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
    sdsfree(wr->req.data);
    free(wr);

    if (status == 0)
        return;
    n_error("uv_write error: %s - %s", uv_err_name(status), uv_strerror(status));
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
            EID eID;
            CreateEIDFromLongLong(pProtoRPC->id, &eID);
            unsigned int addr = GetAddrFromEID(&eID);
            unsigned char port = GetPortFromEID(&eID);

            if (c->pNetServer->udp_ip == addr && c->pNetServer->uportOffset == port) {
                unsigned char docker = GetDockFromEID(&eID);
                DockerPushMsg(docker, buf, buffsize);
            }
            else {
                n_error("RecvCallback error udp addr ip: %i,%i port: %i,%i", c->pNetServer->udp_ip, addr, c->pNetServer->uportOffset, port);
            }
        }
        else if (pProtoHead->proto == proto_route_call) {
            //转发给客户端
            PProtoRoute pProtoRoute = (PProtoRoute)buf;
            EID eID;
            CreateEIDFromLongLong(pProtoRoute->pid, &eID);
            unsigned int addr = GetAddrFromEID(&eID);
            unsigned char port = GetPortFromEID(&eID);

            if (c->pNetServer->udp_ip == addr && c->pNetServer->uportOffset == port) {
                //找到对应entity id 的client id 转发
                dictEntry* entry = dictFind(c->pNetServer->entity_client, (void*)&pProtoRoute->pid);
                if (entry != NULL) {
                    PNetClient pNetClient = (PNetClient)dictGetVal(entry);

                    sds s = sdsnewlen(buf, buffsize);
                    write_req_t* wr = (write_req_t*)malloc(sizeof * wr);
                    wr->buf = uv_buf_init(s, buffsize);
                    wr->req.data = s;
                    int r = uv_write(&wr->req, pNetClient->stream, &wr->buf, 1, after_write);
                    if (r < 0) {
                        n_error("RecvCallback::proto_route_call failed error %s %s", uv_err_name(r), uv_strerror(r));
                    }
                }
            }
        }
        else if (pProtoHead->proto == proto_run_lua) {
            PProtoRunLua pProtoRunLua = (PProtoRunLua)buf;
            DockerRunScript("", 0, pProtoRunLua->dockerid, pProtoRunLua->luaString, pProtoRunLua->protoHead.len - sizeof(PProtoRunLua));
        }
    }
    else if (c->flags & _TCP_SOCKET || c->flags & _TCP_CONNECT) {
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
                EID eID;
                CreateEIDFromLongLong(pProtoRoute->pid, &eID);
                docker = GetDockFromEID(&eID);

                DockerPushMsg(docker, buf, buffsize);
            }
            else {
                n_error("Client is initializing!")
            }
        }
        else {
            n_error("Discard the error packet!")
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

    unsigned int readSize = 0;
    char* pos = buf->base;
    while (true) {
        if (nread <= readSize) {
            break;
        }

        unsigned short len = *(unsigned short*)pos;
        if (len > (nread - readSize)) {
            n_error("tcp_read The remaining nread cannot meet the length requirement");
            break;
        } 
        RecvCallback(c, pos, len);
        readSize += len;
        pos += len;
    }
}

static void on_connection(uv_stream_t* server, int status) {
    
    int r;
    PNetServer pNetServer = uv_handle_get_data((uv_handle_t*)server);
    if (status != 0) {
        n_error("Connect error %s %s", uv_err_name(status), uv_strerror(status));
        return;
    }

    //创建client对象，创建接收数据缓冲区（只大不小），添加到当前链接队列列表
    PNetClient c = calloc(1, sizeof(NetClient));
    c->stream = malloc(sizeof(uv_tcp_t));
    
    r = uv_tcp_init(pNetServer->loop, (uv_tcp_t*)c->stream);
    if (r) {
        n_error("Socket accept error %s %s", uv_err_name(r), uv_strerror(r));
        free(c->stream);
        free(c);
        return;
    }

    /* associate server with stream */
    c->stream->data = c;
    c->pNetServer = pNetServer;
    c->id = ++pNetServer->allClientID;
    c->flags |= _TCP_SOCKET;
    c->tcp_recv_buf = sdsempty();

    r = uv_accept(server, c->stream);
    if (r) {
        n_error("Socket accept error %s %s", uv_err_name(r), uv_strerror(r));
        free(c->stream);
        free(c);
        return;
    }

    r = uv_read_start(c->stream, tcp_alloc, tcp_read);
    if (r) {
        n_error("Socket read_start error %s %s", uv_err_name(r), uv_strerror(r));
        free(c->stream);
        free(c);
        return;
    }

    dictAddWithUint(pNetServer->id_client, c->id, c);
    listAddNodeTail(pNetServer->clients, c);

    if (c->flags & _TCP_SOCKET) {
        //这里调用绑定协议尝试创建entity
        mp_buf* pmp_buf = mp_buf_new();
        mp_encode_bytes(pmp_buf, pNetServer->entityObj, sdslen(pNetServer->entityObj));
        mp_encode_map(pmp_buf, 1);
        mp_encode_bytes(pmp_buf, "clientid", strlen("clientid"));
        mp_encode_int(pmp_buf, c->id);

        unsigned short len = sizeof(ProtoRPCCreate) + pmp_buf->len;
        PProtoHead pProtoHead = malloc(len);
        pProtoHead->len = len;
        pProtoHead->proto = proto_rpc_create;

        PProtoRPCCreate protoRPCCreate = (PProtoRPCCreate)pProtoHead;
        memcpy(protoRPCCreate->callArg, pmp_buf->b, pmp_buf->len);

        c->dockerid = DockerRandomPushMsg((unsigned char*)pProtoHead, len);
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

    uv_tcp_t* tcpServer = malloc(sizeof(uv_tcp_t));
    PNetServer pNetServer = (PNetServer)uv_loop_get_data(loop);

    r = uv_tcp_init(loop, tcpServer);
    if (r) {
        n_error("tcp6_start::Socket creation error %s %s", uv_err_name(r), uv_strerror(r));
        free(tcpServer);
        return NULL;
    }

    r = uv_tcp_bind(tcpServer, (const struct sockaddr*)&addr6, 0);
    if (r) {
        n_error("tcp6_start::Bind error %s %s", uv_err_name(r), uv_strerror(r));
        free(tcpServer);
        return NULL;
    }

    uv_handle_set_data((uv_handle_t*)tcpServer, uv_loop_get_data(loop));

    r = uv_listen((uv_stream_t*)tcpServer, SOMAXCONN, on_connection);
    if (r) {
        n_error("tcp6_start::Listen error %s %s", uv_err_name(r), uv_strerror(r));
        free(tcpServer);
        return NULL;
    }

    return tcpServer;
}

static uv_tcp_t* tcp4_start(uv_loop_t* loop, char* ip, int port) {
    struct sockaddr_in addr;
    int r;
    
    r = uv_ip4_addr(ip, port, &addr);
    if (r) {
        n_error("tcp4_start::Socket uv_ip4_addr error %s %s", uv_err_name(r), uv_strerror(r));
        return NULL;
    }

    uv_tcp_t* tcpServer = malloc(sizeof(uv_tcp_t));
    PNetServer pNetServer = (PNetServer)uv_loop_get_data(loop);

    r = uv_tcp_init(loop, tcpServer);
    if (r) {
        n_error("tcp4_start::Socket creation error %s %s", uv_err_name(r), uv_strerror(r));
        free(tcpServer);
        return NULL;
    }

    r = uv_tcp_bind(tcpServer, (const struct sockaddr*)&addr, 0);
    if (r) {
        n_error("tcp4_start::Bind error %s %s", uv_err_name(r), uv_strerror(r));
        free(tcpServer);
        return NULL;
    }

    uv_handle_set_data((uv_handle_t*)tcpServer, uv_loop_get_data(loop));

    r = uv_listen((uv_stream_t*)tcpServer, SOMAXCONN, on_connection);
    if (r) {
        n_error("tcp4_start::Listen error %s %s", uv_err_name(r), uv_strerror(r));
        free(tcpServer);
        return NULL;
    }

    return tcpServer;
}

static void doJsonParseFile(PNetServer pNetServer, char* config)
{
    if (config == NULL) {
        config = getenv("AssetsPath");
        if (config == 0 || access_t(config, 0) != 0) {
            config = "../../res/server/config_defaults.json";
            if (access_t(config, 0) != 0) {
                return;
            }
        }
    }

    FILE* f; size_t len; char* data;
    cJSON* json;

    f = fopen_t(config, "rb");

    if (f == NULL) {
        printf("Error Open File: [%s]\n", config);
        return;
    }

    fseek_t(f, 0, SEEK_END);
    len = ftell_t(f);
    fseek_t(f, 0, SEEK_SET);
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
                if (pNetServer->uport == 9577)
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
            else if (strcmp(item->string, "udp_ip") == 0) {
                inet_pton(AF_INET, cJSON_GetStringValue(item), &pNetServer->udp_ip);
            }
            else if (strcmp(item->string, "entityObj") == 0) {
                sdsfree(pNetServer->entityObj);
                pNetServer->entityObj = sdsnew(cJSON_GetStringValue(item));
            }
            else if (strcmp(item->string, "botsObj") == 0) {
                sdsfree(pNetServer->botsObj);
                pNetServer->botsObj = sdsnew(cJSON_GetStringValue(item));
            }
            else if (strcmp(item->string, "bindListenIp") == 0) {
                unsigned short r = (unsigned short)cJSON_GetNumberValue(item);
                if (r == 4)
                    pNetServer->bindListenIp = _TCP4_SOCKET;
                else if (r == 6)
                    pNetServer->bindListenIp = _TCP6_SOCKET;
            }
            item = item->next;
        }
    }

    cJSON_Delete(json);
    free(data);
}

static void NetRun(void* pVoid) {
    PNetServer pNetServer = pVoid;
    uv_run(pNetServer->loop, UV_RUN_DEFAULT);
    return;
}

static void connect_cb(uv_connect_t* req, int status) {

    if (status != 0) {
        n_error("connect_cb error: %s - %s", uv_err_name(status), uv_strerror(status));
    }

    NetClient* c = req->data;
    free(req);

    dictAddWithUint(c->pNetServer->id_client, c->id, c);
    listAddNodeTail(c->pNetServer->clients, c);

    if (c->flags & _TCP_CONNECT) {
        //这里调用绑定协议尝试创建entity
        mp_buf* pmp_buf = mp_buf_new();
        mp_encode_bytes(pmp_buf, c->pNetServer->botsObj, sdslen(c->pNetServer->botsObj));
        mp_encode_map(pmp_buf, 2);
        mp_encode_bytes(pmp_buf, "isconnect", strlen("isconnect"));
        mp_encode_int(pmp_buf, 1);
        mp_encode_bytes(pmp_buf, "clientid", strlen("clientid"));
        mp_encode_int(pmp_buf, c->id);

        unsigned short len = sizeof(ProtoRPCCreate) + pmp_buf->len;
        PProtoHead pProtoHead = malloc(len);
        pProtoHead->len = len;
        pProtoHead->proto = proto_rpc_create;

        PProtoRPCCreate protoRPCCreate = (PProtoRPCCreate)pProtoHead;
        memcpy(protoRPCCreate->callArg, pmp_buf->b, pmp_buf->len);

        c->dockerid = DockerRandomPushMsg((unsigned char*)pProtoHead, len);
    }

    int r = uv_read_start(c->stream, tcp_alloc, tcp_read);
    if (r) {
        n_error("Socket read_start error %s %s", uv_err_name(r), uv_strerror(r));
        freeClient(c);
        return;
    }
}

static void NetCommand(char* pBuf, PNetServer pNetServer) {
    PProtoHead pProtoHead = (PProtoHead)(pBuf);

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
        //创建client对象，创建接收数据缓冲区（只大不小），添加到当前链接队列列表
        PNetClient c = calloc(1, sizeof(NetClient));
        c->stream = malloc(sizeof(uv_tcp_t));

        connect_req->data = c;
        /* associate server with stream */
        c->stream->data = c;
        c->pNetServer = pNetServer;
        c->id = ++pNetServer->allClientID;
        c->flags |= _TCP_CONNECT;
        c->tcp_recv_buf = sdsempty();

        r = uv_tcp_init(pNetServer->loop, (uv_tcp_t*)c->stream);
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

/*这里要加队列锁，支持多线程*/
int NetSendToClient(unsigned int id, const char* b, unsigned short s) {

    sds sendBuf = sdsnewlen(0, s + sizeof(ProtoSendToClient));
    PProtoSendToClient pProtoSendToClient = (PProtoSendToClient)sendBuf;
    pProtoSendToClient->protoHead.len = s + sizeof(ProtoSendToClient);
    pProtoSendToClient->protoHead.proto = proto_client_id;
    pProtoSendToClient->clientId = id;
    memcpy(pProtoSendToClient->buf, b, s);

    int len = 0;
    MutexLock(_pNetServer->sendBufMutexHandle, "");
    listAddNodeTail(_pNetServer->sendBuf, sendBuf);
    len = listLength(_pNetServer->sendBuf);
    if (len == 1) {
        uv_async_send(&_pNetServer->async);
    }
    MutexUnlock(_pNetServer->sendBufMutexHandle, "");
    return 1;
}

int NetSendToEntity(unsigned long long entityid, const char* b, unsigned short s) {

    sds sendBuf = sdsnewlen(0, s + sizeof(ProtoSendToEntity));
    PProtoSendToEntity pProtoSendToEntity = (PProtoSendToEntity)sendBuf;
    pProtoSendToEntity->protoHead.len = s + sizeof(ProtoSendToEntity);
    pProtoSendToEntity->protoHead.proto = proto_client_entity;
    pProtoSendToEntity->entityId = entityid;
    memcpy(pProtoSendToEntity->buf, b, s);

    int len = 0;
    MutexLock(_pNetServer->sendBufMutexHandle, "");
    listAddNodeTail(_pNetServer->sendBuf, sendBuf);
    len = listLength(_pNetServer->sendBuf);
    if (len == 1) {
        uv_async_send(&_pNetServer->async);
    }
    MutexUnlock(_pNetServer->sendBufMutexHandle, "");
    return 1;
}

static void SendLoop(PNetServer pNetServer) {
    int r;
    while (true) {
        //从队列中取出数据
        void* value = 0;
        MutexLock(pNetServer->sendBufMutexHandle, "");
        if (listLength(pNetServer->sendBuf) != 0) {
            listNode* node = listFirst(pNetServer->sendBuf);
            value = listNodeValue(node);
            listDelNode(pNetServer->sendBuf, node);
        }
        MutexUnlock(pNetServer->sendBufMutexHandle, "");

        if (value == 0)
            break;

        PProtoHead pProtoHead = (PProtoHead)value;
        NetClient* c = 0;

        if (pProtoHead->proto == proto_client_id) {

            PProtoSendToClient pProtoSendToClient = (PProtoSendToClient)value;
            if (pProtoSendToClient->clientId == 0) {
                NetCommand(pProtoSendToClient->buf, pNetServer);
                sdsfree(value);
                continue;
            }

            dictEntry* entry = dictFind(pNetServer->id_client, (unsigned int*)&pProtoSendToClient->clientId);
            if (entry == NULL) {
                //没有找到客户端id丢弃封包
                n_error("PipeHandler::proto_client_id %i lost packet not find clientid %i", pProtoHead->proto, pProtoSendToClient->clientId);
                sdsfree(value);
                return;
            }
            c = (NetClient*)dictGetVal(entry);

            int nread = pProtoSendToClient->protoHead.len - sizeof(PProtoSendToClient);
            write_req_t* wr = (write_req_t*)malloc(sizeof * wr);
            wr->buf = uv_buf_init(pProtoSendToClient->buf, nread);
            wr->req.data = value;
            r = uv_write(&wr->req, c->stream, &wr->buf, 1, after_write);
            if (r < 0) {
                n_error("async_cb::uv_write failed error %s %s", uv_err_name(r), uv_strerror(r));
            }
        }
        else if (pProtoHead->proto == proto_client_entity) {
            PProtoSendToEntity pProtoSendToEntity = (PProtoSendToEntity)value;
            if (pProtoSendToEntity->entityId == 0) {
                NetCommand(pProtoSendToEntity->buf, pNetServer);
                sdsfree(value);
                continue;
            }

            dictEntry* entry = dictFind(pNetServer->entity_client, (unsigned long long*) & pProtoSendToEntity->entityId);
            if (entry == NULL) {
                //没有找到客户端id丢弃封包
                n_error("PipeHandler::proto_client_entity %i lost packet not find entityId %U", pProtoHead->proto, pProtoSendToEntity->entityId);
                return;
            }
            c = (NetClient*)dictGetVal(entry);

            int nread = pProtoSendToEntity->protoHead.len - sizeof(ProtoSendToEntity);
            write_req_t* wr = (write_req_t*)malloc(sizeof * wr);
            wr->buf = uv_buf_init(pProtoSendToEntity->buf, nread);
            wr->req.data = value;

            if (uv_write(&wr->req, c->stream, &wr->buf, 1, after_write)) {
                n_error("async_cb::uv_write failed error %s %s", uv_err_name(r), uv_strerror(r));
            }
        }
        else if (pProtoHead->proto == proto_ctr_cancel) {
            sdsfree(value);
            uv_stop(pNetServer->loop);
        }
    }
}

//触发信号，来至其他线程的指令
void async_cb(uv_async_t* handle) {

    PNetServer pNetServer = uv_handle_get_data((uv_handle_t*)handle);
    SendLoop(pNetServer);
}

static int listenToPort(PNetServer pNetServer, int port) {
    int j = 0;
    /* Force binding of 0.0.0.0 if no bind address is specified, always
     * entering the loop if j == 0. */
    if (pNetServer->bindaddr_count == 0) {
        pNetServer->bindaddr[0] = NULL;
        if (pNetServer->bindListenIp & _TCP4_SOCKET) {
            pNetServer->tcp4_handle[pNetServer->tcp4_handle_count] = tcp4_start(pNetServer->loop, "0.0.0.0", port);
            if (pNetServer->tcp4_handle[pNetServer->tcp4_handle_count] == 0) {
                n_error(
                    "Creating Server TCP4 listening socket %s:%i:",
                    pNetServer->bindaddr[j] ? pNetServer->bindaddr[j] : "*",
                    port);
                return 0;
            }
            else {
                pNetServer->tcp4_handle_count++;
            }
        }
        
        if (pNetServer->bindListenIp & _TCP6_SOCKET) {
            pNetServer->tcp6_handle[pNetServer->tcp6_handle_count] = tcp6_start(pNetServer->loop, "0:0:0:0:0:0:0:0", port);
            if (pNetServer->tcp6_handle[pNetServer->tcp6_handle_count] == 0) {
                n_error(
                    "Creating Server TCP6 listening socket %s:%i:",
                    pNetServer->bindaddr[j] ? pNetServer->bindaddr[j] : "*",
                    port);

                return 0;
            }
            else {
                pNetServer->tcp6_handle_count++;
            }
        }
        return 1;
    }

    for (j = 0; j < pNetServer->bindaddr_count || j == 0; j++) {
        if (pNetServer->bindaddr[j] == NULL) {
            /* Bind * for both IPv6 and IPv4, we enter here only if
             * server.bindaddr_count == 0. */
            continue;
        }
        else if (strchr(pNetServer->bindaddr[j], ':')) {
            /* Bind IPv6 address. */
            pNetServer->tcp6_handle[pNetServer->tcp4_handle_count] = tcp6_start(pNetServer->loop, pNetServer->bindaddr[j], port);
            if (pNetServer->tcp6_handle[pNetServer->tcp6_handle_count] == 0) {
                n_error(
                    "Creating Server TCP6 bindaddr listening socket %s:%i:",
                    pNetServer->bindaddr[j] ? pNetServer->bindaddr[j] : "*",
                    port);
                continue;
            }
            else {
                pNetServer->tcp4_handle_count++;
            }
            
        }
        else {
            /* Bind IPv4 address. */
            pNetServer->tcp4_handle[pNetServer->tcp4_handle_count] = tcp4_start(pNetServer->loop, pNetServer->bindaddr[j], port);
            if (pNetServer->tcp4_handle[pNetServer->tcp4_handle_count] == 0) {
                n_error(
                    "Creating Server TCP4 bindaddr listening socket %s:%i:",
                    pNetServer->bindaddr[j] ? pNetServer->bindaddr[j] : "*",
                    port);
                continue;
            }
            else {
                pNetServer->tcp4_handle_count++;
            }
        }
    }
    return 1;
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

static uv_udp_t* udp4_start(uv_loop_t* loop, char* ip, int port) {
    struct sockaddr_in addr;
    int r;

    r = uv_ip4_addr(ip, port, &addr);
    if (r) {
        n_error("udp4_start::Socket uv_ip4_addr error %s %s", uv_err_name(r), uv_strerror(r));
        return NULL;
    }

    uv_udp_t* udpServer = malloc(sizeof(uv_udp_t));
    PNetServer pNetServer = (PNetServer)uv_loop_get_data(loop);

    r = uv_udp_init(loop, udpServer);
    if (r) {
        n_error("udp4_start::Socket creation error %s %s", uv_err_name(r), uv_strerror(r));
        free(udpServer);
        return NULL;
    }

    r = uv_udp_bind(udpServer, (const struct sockaddr*)&addr, 0);
    if (r) {
        n_error("udp4_start::Bind error %s %s", uv_err_name(r), uv_strerror(r));
        free(udpServer);
        return NULL;
    }

    PNetClient c = malloc(sizeof(NetClient));
    c->id = ++pNetServer->allClientID;
    c->pNetServer = uv_loop_get_data(loop);
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
    PNetServer pNetServer = uv_handle_get_data((uv_handle_t*)handle);
    SendLoop(pNetServer);
}

void* NetCreate(int nodetype, unsigned short listentcp) {
    
    int r = 0;
    _pNetServer = calloc(1, sizeof(NetServer));
    _pNetServer->hz = 10;

    if (listentcp)
        _pNetServer->port = listentcp;
    else
        _pNetServer->port = 9577;

    _pNetServer->uport = 7795;
    _pNetServer->maxclients = 2000;
    _pNetServer->udp_ip = 0;
    _pNetServer->udp_port = _pNetServer->uport;
    _pNetServer->entityObj = sdsnew("account");
    _pNetServer->botsObj = sdsnew("bots");
    _pNetServer->nodetype = nodetype;
    _pNetServer->allClientID = 0;
    _pNetServer->id_client = dictCreate(DefaultUintPtr(), NULL);
    _pNetServer->clients = listCreate();
    _pNetServer->entity_client = dictCreate(DefaultLonglongPtr(), NULL);
    _pNetServer->sendBuf = listCreate();
    _pNetServer->sendBufMutexHandle = MutexCreateHandle(LockLevel_4);

#ifdef _WIN32
    _pNetServer->bindListenIp = _TCP4_SOCKET | _TCP6_SOCKET;
#else
    _pNetServer->bindListenIp = _TCP4_SOCKET;
#endif // _WIN32 

    doJsonParseFile(_pNetServer, NULL);
 
    _pNetServer->loop = uv_default_loop();
    uv_loop_set_data(_pNetServer->loop, _pNetServer);

    if (!(nodetype & NO_TCP_LISTEN)) {
        /* Open the TCP listening socket for the user commands. */
        if (_pNetServer->port != 0 &&
            listenToPort(_pNetServer, _pNetServer->port) == 0)
            exit(1);
    }

    if (!(nodetype & NO_UDP_LISTEN)) {
        //udp
        r = uv_udp_init(_pNetServer->loop, &_pNetServer->sendUdp);
        if (r) {
            n_error("NetCreate::uv_udp_init error %s %s", uv_err_name(r), uv_strerror(r));
            return NULL;
        }

        for (_pNetServer->uportOffset = 0; _pNetServer->uportOffset < 256; _pNetServer->uportOffset++) {
            /* Open the UDP listening socket for the user commands. */
            _pNetServer->udp_port = _pNetServer->uport + _pNetServer->uportOffset;
            _pNetServer->udp4 = udp4_start(_pNetServer->loop, "0.0.0.0", _pNetServer->udp_port);
            if (_pNetServer->uport != 0 && _pNetServer->udp4 != NULL) {

                if (_pNetServer->udp_ip == 0) {
                    uv_os_fd_t socket;
                    uv_fileno((uv_handle_t*)_pNetServer->udp4, &socket);
                    IntConfigPrimary((unsigned long long)socket, &_pNetServer->udp_ip, &_pNetServer->udp_port);
                    if (_pNetServer->udp_ip == 0 || _pNetServer->udp_port == 0) {
                        n_error("InitServer:: udp_ip or udp_port is empty! ip:%i port:%i", _pNetServer->udp_ip, _pNetServer->udp_port);
                    }
                }

                if (_pNetServer->nodetype) {
                    SetUpdIPAndPortToInside(_pNetServer->udp_ip, _pNetServer->udp_port);
                }
                else {
                    SetUpdIPAndPortToOutside(_pNetServer->udp_ip, _pNetServer->udp_port);
                }

                break;
            }
        }
        n_details("InitServer::udp_port on:%i", _pNetServer->udp_port);
    }

    //init async for send
    _pNetServer->async.data = _pNetServer;
    r = uv_async_init(_pNetServer->loop, &_pNetServer->async, async_cb);
    if (r) {
        n_error("UVNetCreate::uv_async_init error %s %s", uv_err_name(r), uv_strerror(r));
        return NULL;
    }

    r = uv_timer_init(uv_default_loop(), &_pNetServer->once);
    if (r) {
        n_error("UVNetCreate::uv_async_init error %s %s", uv_err_name(r), uv_strerror(r));
        return NULL;
    }
    uv_handle_set_data((uv_handle_t*)&_pNetServer->once, _pNetServer);

    r = uv_timer_start(&_pNetServer->once, once_cb, 10, 0);
    if (r) {
        n_error("UVNetCreate::uv_async_init error %s %s", uv_err_name(r), uv_strerror(r));
        return NULL;
    }

    uv_thread_create(&_pNetServer->thread, NetRun, _pNetServer);
    return _pNetServer;
}

static void ClientsDestroyFun(void* value)
{
    freeClient((NetClient*)value);
}

static void SendBufDestroyFun(void* value)
{
    sdsfree((sds)value);
}

int NetSendToCancel() {

    sds sendBuf = sdsnewlen(0, sizeof(ProtoSendTCancel));
    PProtoSendTCancel pProtoSendTCancel = (PProtoSendTCancel)sendBuf;
    pProtoSendTCancel->protoHead.proto = proto_ctr_cancel;
    pProtoSendTCancel->protoHead.len = 0;

    int len = 0;
    MutexLock(_pNetServer->sendBufMutexHandle, "");
    listAddNodeTail(_pNetServer->sendBuf, sendBuf);
    len = listLength(_pNetServer->sendBuf);
    if (len == 1) {
        //todo 可能会导致空循环一次，在刚刚处理数据后
        uv_async_send(&_pNetServer->async);
    }
    MutexUnlock(_pNetServer->sendBufMutexHandle, "");
    return 1;
}

void NetDestory() {
    if (_pNetServer) {

        NetSendToCancel();
        uv_thread_join(&_pNetServer->thread);

        uv_close((uv_handle_t*)&_pNetServer->sendUdp, NULL);
        uv_close((uv_handle_t*)&_pNetServer->once, NULL);

        for (int i = 0; i < _pNetServer->bindaddr_count; i++) {
            sdsfree(_pNetServer->bindaddr[i]);
        }

        listSetFreeMethod(_pNetServer->clients, ClientsDestroyFun);
        listRelease(_pNetServer->clients);
        dictRelease(_pNetServer->id_client);
        sdsfree(_pNetServer->entityObj);
        sdsfree(_pNetServer->botsObj);
        dictRelease(_pNetServer->entity_client);

        for (int i = 0; i < _BINDADDR_MAX; i++) {
            if (_pNetServer->bindaddr[i] != 0)
                sdsfree(_pNetServer->bindaddr[i]);
        }
    
        listSetFreeMethod(_pNetServer->sendBuf, SendBufDestroyFun);
        listRelease(_pNetServer->sendBuf);
        MutexDestroyHandle(_pNetServer->sendBufMutexHandle);

        for (int i = 0; i < _pNetServer->tcp6_handle_count; i++) {
            if (_pNetServer->tcp6_handle[i] != 0) {
                uv_close((uv_handle_t*)_pNetServer->tcp6_handle[i], NULL);
                free(_pNetServer->tcp6_handle[i]);
            }
        }

        for (int i = 0; i < _pNetServer->tcp4_handle_count; i++) {
            if (_pNetServer->tcp4_handle[i] != 0) {
                uv_close((uv_handle_t*)_pNetServer->tcp4_handle[i], NULL);
                free(_pNetServer->tcp4_handle[i]);
            }
        }

        uv_close((uv_handle_t*)_pNetServer->udp4, NULL);
        free(_pNetServer->udp4);

        uv_close((uv_handle_t*)&_pNetServer->async, NULL);
        free(_pNetServer);
    }
}

void NetUDPAddr2(unsigned int* ip, unsigned char* uportOffset, unsigned short* uport) {
    if (_pNetServer == NULL) {
        return;
    }

    *ip = _pNetServer->udp_ip;
    *uportOffset = _pNetServer->uportOffset;
    *uport = _pNetServer->uport;
}

/*
任何entity都会通过全局对象或空间对象获得其他对象的mailbox，
这样就可以在任何条件下发送消息给entity到网络层，
网络层会有所有entity对象的dock映射关系，
所以mailbox不需要携带dock数据。
*/
void NetSendToNode(char* ip, unsigned short port, unsigned char* b, unsigned short s) {
    struct sockaddr_in server_addr;
    uv_buf_t buf;
    int r;

    if (s > PACKET_MAX_SIZE_UDP) {
        n_error("NetSendToNode::NetSendToNode error Length greater than limit PACKET_MAX_SIZE_UDP %i", s);
        return;
    }

    r = uv_ip4_addr(ip, port, &server_addr);
    
    if (r) {
        n_error("NetSendToNode::uv_ip4_addr error %s %s", uv_err_name(r), uv_strerror(r));
        return;
    }

    buf = uv_buf_init(b, s);
    r = uv_udp_try_send(&_pNetServer->sendUdp, &buf, 1, (const struct sockaddr*)&server_addr);

    if (r < 0) {
        n_error("NetSendToNode::uv_udp_try_send error %s %s", uv_err_name(r), uv_strerror(r));
    }
}

void NetSendToNodeWithUINT(unsigned int ip, unsigned short port, unsigned char* b, unsigned short s) {
    struct sockaddr_in server_addr;
    uv_buf_t buf;
    int r;

    if (s > PACKET_MAX_SIZE_UDP) {
        n_error("NetSendToNode::NetSendToNode error Length greater than limit PACKET_MAX_SIZE_UDP %i", s);
        return;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    unsigned int out_ip = htonl(ip);
    memcpy(&(server_addr.sin_addr.s_addr), &out_ip, sizeof(unsigned int));

    buf = uv_buf_init(b, s);
    r = uv_udp_try_send(&_pNetServer->sendUdp, &buf, 1, (const struct sockaddr*)&server_addr);

    if (r < 0) {
        n_error("NetSendToNode::uv_udp_try_send error %s %s", uv_err_name(r), uv_strerror(r));
    }
}