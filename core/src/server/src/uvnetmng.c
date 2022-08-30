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

#include "uvnetmng.h"
#include "uv.h"
#include "plateform.h"
#include "filesys.h"
#include "cJSON.h"
#include "sds.h"
#include "netex.h"
#include "elog.h"
#include "dict.h"
#include "adlist.h"
#include "uvnettcp.h"
#include "dicthelp.h"
#include "uvnetudp.h"
#include "configfile.h"
#include "entityid.h"

#define _BINDADDR_MAX 16
typedef struct _NetServerMng {
    char* udp4BindAddr[_BINDADDR_MAX];     /* udp4 addr upd只能使用4*/
    unsigned char udp4PortOffset[_BINDADDR_MAX]; /* udp地址对应的端口为零使用默认*/
    unsigned short udp4Count;
    unsigned short udpBeginPort;           /*  udp起始端口，所有地址公用 默认端口偏移为零 */
    unsigned char udpPortOffset;      /* 当所有绑定端口都失败后，默认udp绑定端口的偏移量 */

    void* TcpServer;
    void* udpWorker[_BINDADDR_MAX];
    unsigned int udpWorkerCount;

    dict* udpIndex;
} *PNetServerMng, NetServerMng;

static PNetServerMng __pNetServerMng;

static void doJsonParseFile(void* pVoid, char* data)
{
    PNetServerMng pNetServerMng = pVoid;
    cJSON* json = cJSON_Parse(data);
    if (!json) {
        //printf("Error before: [%s]\n", cJSON_GetErrorPtr());	
        return;
    }

    cJSON* logJson = cJSON_GetObjectItem(json, "netmng");
    if (logJson) {

        cJSON* item = logJson->child;
        while ((item != NULL) && (item->string != NULL))
        {
            if (strcmp(item->string, "udpBeginPort") == 0) {
                pNetServerMng->udpBeginPort = (unsigned short)cJSON_GetNumberValue(item);
            }
            else if (strcmp(item->string, "udp4") == 0) {

                pNetServerMng->udp4Count = 0;
                for (int i = 0; i < cJSON_GetArraySize(item); i++)
                {
                    cJSON* arrayItem = cJSON_GetArrayItem(item, i);
                    cJSON* item = arrayItem->child;
                    while ((item != NULL) && (item->string != NULL))
                    {
                        if (strcmp(item->string, "udp4BindAddr") == 0) {
                            pNetServerMng->udp4BindAddr[i] = sdsnew(cJSON_GetStringValue(item));
                        }
                        else if (strcmp(item->string, "udp4Count") == 0) {
                            pNetServerMng->udp4PortOffset[i] = (unsigned short)cJSON_GetNumberValue(item);
                        }
                        item = item->next;
                    }

                    if (++pNetServerMng->udp4Count >= _BINDADDR_MAX) {
                        break;
                    }
                }
            }
            item = item->next;
        }
    }

    cJSON_Delete(json);
}

void udpFreeCallback(void* privdata, void* val) {
    DICT_NOTUSED(privdata);
    UdpDestory(val);
}

static dictType LongLongDictUdpType = {
    longlongHashCallback,
    NULL,
    NULL,
    LonglongCompareCallback,
    memFreeCallback,
    udpFreeCallback
};

static void* DefaultLonglongUdpPtr() {
    return &LongLongDictUdpType;
}

void* NetMngCreate(unsigned char nodetype) {

    int r = 0;
    PNetServerMng pNetServerMng = calloc(1, sizeof(NetServerMng));
    if (pNetServerMng == NULL) {
        n_error("NetMngCreate calloc error");
        return NULL;
    }

    pNetServerMng->udpBeginPort = 7795;
    pNetServerMng->udpPortOffset = 1;
    pNetServerMng->udpWorkerCount = 1;
    pNetServerMng->TcpServer = 0;
    pNetServerMng->udpIndex = dictCreate(DefaultLonglongUdpPtr(), NULL);

    DoJsonParseFile(pNetServerMng, doJsonParseFile);
    pNetServerMng->TcpServer = TcpCreate(pNetServerMng, nodetype);

    if (!(nodetype & NO_UDP_LISTEN)) {
        for (int i = 0; i < pNetServerMng->udp4Count; i++) {
            if (pNetServerMng->udp4PortOffset[i] == 0) {
                n_error("udp4PortOffset Zero is not allowed");
                continue;
            }
            unsigned long long id;
            void* pVoid = UdpCreate(pNetServerMng->udp4BindAddr[i], pNetServerMng->udpBeginPort, pNetServerMng->udp4PortOffset[i], &id, nodetype & NO_TCP_LISTEN);
            if (pVoid) {
                dictAddWithLonglong(pNetServerMng->udpIndex, id, pVoid);
            }
        }

        if (dictSize(pNetServerMng->udpIndex) == 0) {
            for (;pNetServerMng->udpPortOffset < UCHAR_MAX; pNetServerMng->udpPortOffset++) {
                unsigned long long id;
                void* pVoid = UdpCreate("0.0.0.0", pNetServerMng->udpBeginPort, pNetServerMng->udpPortOffset, &id, nodetype & NO_TCP_LISTEN);
                if (pVoid) {
                    dictAddWithLonglong(pNetServerMng->udpIndex, id, pVoid);
                    break;
                }
            }
        }

        if (dictSize(pNetServerMng->udpIndex) == 0) {
            n_error("UDP startup failed");
        }
    }

    __pNetServerMng = pNetServerMng;
    return pNetServerMng;
}

void NetMngDestory() {

    PNetServerMng pNetServerMng = __pNetServerMng;
    //停止所有线程并释放
    if(pNetServerMng->TcpServer)
        TcpDestory(pNetServerMng->TcpServer);
    dictRelease(pNetServerMng->udpIndex);

    for (int i = 0; i < pNetServerMng->udp4Count; i++) {
        sdsfree(pNetServerMng->udp4BindAddr[i]);
    }
}

void GetRandomUdp(unsigned long long* id) {
    if (dictSize(__pNetServerMng->udpIndex) == 0) {
        *id = 0x11000000000000;
        return;
    }
    dictEntry* entry = dictGetRandomKey(__pNetServerMng->udpIndex);
    *id = *(unsigned long long*)dictGetKey(entry);
}

int IsNodeUdp(unsigned long long id) {

    idl64 eid;
    eid.u = id;
    eid.eid.dock = 0;
    eid.eid.id = 0;
    unsigned long long cid = eid.u;

    dictEntry* entry = dictFind(__pNetServerMng->udpIndex, &cid);
    if (entry == NULL)
        return 0;
    else
        return 1;
}

unsigned short UdpBasePort() {
    return __pNetServerMng->udpBeginPort;
}

int MngSendToEntity(unsigned long long entityid, const char* b, unsigned int s) {
    return TcpSendToEntity(__pNetServerMng->TcpServer, entityid, b, s);
}

int MngSendToClient(unsigned long int id, const char* b, unsigned int s) {
    return TcpSendToClient(__pNetServerMng->TcpServer, id, b, s);
}

int MngRandomSendTo(const char* b, unsigned int s) {
    return TcpRandomSendTo(__pNetServerMng->TcpServer, b, s);
}
