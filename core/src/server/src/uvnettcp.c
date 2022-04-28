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
#include "cJSON.h"
#include "sds.h"
#include "elog.h"
#include "dict.h"
#include "adlist.h"
#include "dicthelp.h"
#include "docker.h"
#include "timesys.h"
#include "dictQueue.h"
#include "uvnetmng.h"
#include "filesys.h"
#include "uvnettcpwork.h"
#include "entityid.h"
#include "configfile.h"

#define _BINDADDR_MAX 16

typedef struct _NetTcp {

    char* tcp4BindAddr[_BINDADDR_MAX];     /* ip4 addr */
    unsigned short tcp4Port[_BINDADDR_MAX]; /* ip4地址对应的端口为零使用默认*/
    unsigned short tcp4Count;

    char* tcp6BindAddr[_BINDADDR_MAX];     /* ip6 addr */
    unsigned short tcp6Port[_BINDADDR_MAX];/* ip6地址对应的端口为零使用默认*/
    unsigned short tcp6Count;

    unsigned short tpcPort;//默认tcp端口
    unsigned long long sendStamp;

    void* pMng;
    void* tcpWorker[_BINDADDR_MAX * 3];
    unsigned int tcpWorkerAdd;
    unsigned int tcpWorkerCount;
} *PNetTcp, NetTcp;

static void doJsonParseFile(void* pVoid, char* data)
{
    PNetTcp pNetTcp = pVoid;
    cJSON* json = cJSON_Parse(data);
    if (!json) {
        //printf("Error before: [%s]\n", cJSON_GetErrorPtr());	
        return;
    }

    cJSON* logJson = cJSON_GetObjectItem(json, "tcp");
    if (logJson) {

        cJSON* item = logJson->child;
        while ((item != NULL) && (item->string != NULL))
        {
            if (strcmp(item->string, "tpcPort") == 0) {
                pNetTcp->tpcPort = (unsigned short)cJSON_GetNumberValue(item);
            }
            else if (strcmp(item->string, "tcp4") == 0) {

                pNetTcp->tcp4Count = 0;
                for (int i = 0; i < cJSON_GetArraySize(item); i++)
                {
                    cJSON* arrayItem = cJSON_GetArrayItem(item, i);
                    cJSON* item = arrayItem->child;
                    while ((item != NULL) && (item->string != NULL))
                    {
                        if (strcmp(item->string, "tcp4BindAddr") == 0) {
                            pNetTcp->tcp4BindAddr[i] = sdsnew(cJSON_GetStringValue(item));
                        }
                        else if (strcmp(item->string, "tcp4Port") == 0) {
                            pNetTcp->tcp4Port[i] = (unsigned short)cJSON_GetNumberValue(item);
                        }
                        item = item->next;
                    }

                    if (++pNetTcp->tcp4Count >= _BINDADDR_MAX) {
                        break;
                    }
                }
            }
            else if (strcmp(item->string, "tcp6") == 0) {

                pNetTcp->tcp6Count = 0;
                for (int i = 0; i < cJSON_GetArraySize(item); i++)
                {
                    cJSON* arrayItem = cJSON_GetArrayItem(item, i);
                    cJSON* item = arrayItem->child;
                    while ((item != NULL) && (item->string != NULL))
                    {
                        if (strcmp(item->string, "tcp6BindAddr") == 0) {
                            pNetTcp->tcp4BindAddr[i] = sdsnew(cJSON_GetStringValue(item));
                        }
                        else if (strcmp(item->string, "tcp6Port") == 0) {
                            pNetTcp->tcp6Port[i] = (unsigned short)cJSON_GetNumberValue(item);
                        }
                        item = item->next;
                    }

                    if (++pNetTcp->tcp6Count >= _BINDADDR_MAX) {
                        break;
                    }
                }
            }
            else if (strcmp(item->string, "tcpWorkerAdd") == 0) {
                pNetTcp->tcpWorkerAdd = (unsigned short)cJSON_GetNumberValue(item);
                if (pNetTcp->tcpWorkerAdd > _BINDADDR_MAX )
                    pNetTcp->tcpWorkerAdd = _BINDADDR_MAX;
            }
            item = item->next;
        }
    }

    cJSON_Delete(json);
}

void close_cb(uv_handle_t* handle) {
    uv_tcp_t* stream = (uv_tcp_t*)handle;
    free(stream);
}

void echo_write(uv_write_t* req, int status) {
    uv_tcp_t* stream = (uv_tcp_t*)req->data;
    if (status < 0) {
        n_error("echo_write error %s %s", uv_err_name(status), uv_strerror(status));
    }
}

static int listenToPort(PNetTcp pNetTcp) {

    //ip4
    for (int i = 0; i < pNetTcp->tcp4Count; i++) {
        pNetTcp->tcpWorker[pNetTcp->tcpWorkerCount++] = TcpWorkCreate(pNetTcp->tcpWorkerCount, pNetTcp, AF_INET, pNetTcp->tcp4BindAddr[i], pNetTcp->tcp4Port[i] == 0 ? pNetTcp->tpcPort : pNetTcp->tcp4Port[i]);
    }

    //ip6
    for (int i = 0; i < pNetTcp->tcp6Count; i++) {
        pNetTcp->tcpWorker[pNetTcp->tcpWorkerCount++] = TcpWorkCreate(pNetTcp->tcpWorkerCount, pNetTcp, AF_INET6, pNetTcp->tcp4BindAddr[i], pNetTcp->tcp4Port[i] == 0 ? pNetTcp->tpcPort : pNetTcp->tcp4Port[i]);
     }

    //default
    if (pNetTcp->tcpWorkerCount == 0) {
        pNetTcp->tcpWorker[pNetTcp->tcpWorkerCount++] = TcpWorkCreate(pNetTcp->tcpWorkerCount, pNetTcp, AF_INET, "0.0.0.0", pNetTcp->tpcPort);
    }
    return 1;
}

void* TcpCreate(void* pMng, unsigned int nodetype) {

    int r = 0;
    PNetTcp pNetTcp = calloc(1, sizeof(NetTcp));

    if (pNetTcp == NULL) {
        n_error("calloc PNetTcp error");
        return NULL;
    }

    pNetTcp->tpcPort = 9577;
    pNetTcp->sendStamp = GetTick();
    pNetTcp->pMng = pMng;
    pNetTcp->tcpWorkerAdd = 1;
    pNetTcp->tcpWorkerCount = 0;

    DoJsonParseFile(pNetTcp, doJsonParseFile);

    if (!(nodetype & NO_TCP_LISTEN))
        listenToPort(pNetTcp);

    if (nodetype & NO_TCP_LISTEN && nodetype & NO_UDP_LISTEN) {
        for (size_t i = 0; i < pNetTcp->tcpWorkerAdd; i++)
        {
            pNetTcp->tcpWorker[pNetTcp->tcpWorkerCount++] = TcpWorkCreate(pNetTcp->tcpWorkerCount, pNetTcp, 0, 0, 0);
        }
    }
    return pNetTcp;
}


void TcpDestory(void* pVoid) {
    PNetTcp pNetTcp = pVoid;

    for (int i = 0; i < pNetTcp->tcp4Count; i++) {
        sdsfree(pNetTcp->tcp4BindAddr[i]);
    }

    for (int i = 0; i < pNetTcp->tcp6Count; i++) {
        sdsfree(pNetTcp->tcp6BindAddr[i]);
    }

    for (int i = 0; i < pNetTcp->tcpWorkerCount; i++) {

        if(pNetTcp->tcpWorker[i])TcpWorkDestory(pNetTcp->tcpWorker[i]);
    }
    free(pNetTcp);
}

int TcpSendToEntity(void* pVoid, unsigned long long entityid, const char* b, unsigned int s) {

    PNetTcp pNetTcp = pVoid;
    idl64 id;
    id.u = entityid;
    return TcpWorkSendToEntity(pNetTcp->tcpWorker[id.eid.dock % pNetTcp->tcpWorkerCount], entityid, b, s);
}

int TcpSendToClient(void* pVoid, unsigned long int id, const char* b, unsigned int s) {
    PNetTcp pNetTcp = pVoid;
    return TcpWorkSendToEntity(pNetTcp->tcpWorker[id], 0, b, s);
}

int TcpRandomSendTo(void* pVoid, const char* b, unsigned int s) {
    PNetTcp pNetTcp = pVoid;
    return TcpWorkSendToEntity(pNetTcp->tcpWorker[rand() % pNetTcp->tcpWorkerCount], 0, b, s);
}

unsigned int TcpWorkerCount(void* pVoid) {
    PNetTcp pNetTcp = pVoid;
    return pNetTcp->tcpWorkerCount;
}