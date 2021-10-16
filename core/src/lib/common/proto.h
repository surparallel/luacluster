/* args.c - about args function
*
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


enum proto {
	//节点之间的协议
	proto_rpc_create = 1,
	proto_rpc_call,//经过服务器处理可以转发给客户端
	proto_rpc_destory,//用于网络层链接被销毁通知docker销毁对应的entity
	//转发给客户端的协议
	proto_route_call,//直接转发到客户端，不经过服务器逻辑处理
	//控制协议
	proto_ctr_cancel,//控制退出
	proto_run_lua,//执行lua脚本
	//发送给网络层
	proto_net_bind,//绑定网络到entityid
	proto_net_destory,//docker对应的entity被销毁，通知网络层断开链接
	proto_net_connect,//创建一个链接
	//发送协议
	proto_client_id,//使用网络发送tcp消息给指定client id
	proto_client_entity,//使用网络发送tcp消息给指点entity id
};

#pragma pack(push,1)
//协议头保留长度是为了tcp协议
typedef struct _ProtoHead {
	unsigned short len;
	unsigned char proto;
}*PProtoHead, ProtoHead;

//创建对象协议
typedef struct _ProtoRPCCreate {
	ProtoHead		protoHead;
	unsigned char callArg[];//包含对象的构造文件名和初始化参数
}*PProtoRPCCreate, ProtoRPCCreate;

//远程调用协议
typedef struct _ProtoRPC {
	ProtoHead		protoHead;
	unsigned long long id;
	unsigned char callArg[];//包含函数名称的数据包
}*PProtoRPC, ProtoRPC;

//转发的客户端调用协议, 客户端调用服务器只能调用对应sid不能转发，所以不需要转发id
typedef struct _ProtoRoute {
	ProtoHead		protoHead;
	unsigned long long did;//目标的id
	unsigned long long pid;//转发的proxy id
	unsigned char callArg[];
}*PProtoRoute, ProtoRoute;

//转发的客户端调用协议, 客户端调用服务器只能调用对应sid不能转发，所以不需要转发id
typedef struct _ProtoRunLua {
	ProtoHead		protoHead;
	unsigned char luaString[];
}*PProtoRunLua, ProtoRunLua;

//docker发送给网络层，绑定clientid
typedef struct _ProtoNetBind {
	ProtoHead		protoHead;
	unsigned int clientId;
	unsigned long long entityId;
}*PProtoNetBind, ProtoNetBind;

//docker发送给网络层，销毁clientid
//网络层会调用proto_rpc_create和proto_rpc_destory
//来创建tcp相关的entity
//是否需要额外的管理层来管理这些数据呢？
typedef struct _ProtoNetDestory {
	ProtoHead		protoHead;
	unsigned int clientId;
}*PProtoNetDestory, ProtoNetDestory;

//发送不同类型的tcp协议到客户端
typedef struct _ProtoSendToClient {
	ProtoHead		protoHead;
	unsigned int	clientId;
	unsigned char buf[];
}*PProtoSendToClient, ProtoSendToClient;

typedef struct _ProtoSendToEntity {
	ProtoHead		protoHead;
	unsigned long long entityId;
	unsigned char buf[];
}*PProtoSendToEntity, ProtoSendToEntity;

typedef struct _ProtoSendTCancel {
	ProtoHead		protoHead;
}*PProtoSendTCancel, ProtoSendTCancel;

//目前不支持ipv6记得去改
typedef struct _ProtoConnect {
	ProtoHead		protoHead;
	char ip[20];
	unsigned short port;
}*PProtoConnect, ProtoConnect;

#pragma pack(pop)