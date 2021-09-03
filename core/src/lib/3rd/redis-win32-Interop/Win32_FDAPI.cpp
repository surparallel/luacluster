/*
 * Copyright (c), Microsoft Open Technologies, Inc.
 * All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *  - Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  - Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "win32_types.h"

#define FDAPI_NOCRTREDEFS
#include "Win32_FDAPI.h"
#include "win32_rfdmap.h"
#include <exception>
#include <system_error>
#include <mswsock.h>
#include <sys/stat.h>
#include <WSPiApi.h>
#include "Win32_fdapi_crt.h"
#include "Win32_variadicFunctor.h"
#include "Win32_ANSI.h"
#include "Win32_RedisLog.h"
#include "Win32_Common.h"
#include "Win32_Error.h"
#include "Win32_Assert.h"

using namespace std;

#define CATCH_AND_REPORT()  catch(const std::exception &){::redisLog(REDIS_WARNING, "FDAPI: std exception");}catch(...){::redisLog(REDIS_WARNING, "FDAPI: other exception");}

extern "C" {
// Unix compatible FD based routines
fdapi_accept w_accept = NULL;
fdapi_access w_access = NULL;
fdapi_bind w_bind = NULL;
fdapi_connect w_connect = NULL;
fdapi_fcntl w_fcntl = NULL;
fdapi_fstat w_fdapi_fstat64 = NULL;
fdapi_fsync w_fsync = NULL;
fdapi_ftruncate w_ftruncate = NULL;
fdapi_getpeername w_getpeername = NULL;
fdapi_getsockname w_getsockname = NULL;
fdapi_getsockopt w_getsockopt = NULL;
fdapi_isatty w_isatty = NULL;
fdapi_inet_ntop w_inet_ntop = NULL;
fdapi_inet_pton w_inet_pton = NULL;
fdapi_listen w_listen = NULL;
fdapi_lseek64 w_lseek64 = NULL;
fdapi_open w_open = NULL;
fdapi_pipe w_pipe = NULL;
fdapi_poll w_poll = NULL;
fdapi_read w_read = NULL;
fdapi_select w_select = NULL;
fdapi_setsockopt w_setsockopt = NULL;
fdapi_socket w_socket = NULL;
fdapi_write w_write = NULL;
fdapi_recvfrom w_recvfrom = NULL;
fdapi_sendto w_sendto = NULL;
fdapi_shutdown w_shutdown = NULL;
}
/*
w_htonl = FDAPI_htonl;
w_htons = FDAPI_htons;
w_gethostbyaddr = FDAPI_gethostbyaddr;
w_gethostbyname = FDAPI_gethostbyname;
w_inet_ntoa = FDAPI_inet_ntoa;
w_freeaddrinfo = FDAPI_freeaddrinfo;
w_getaddrinfo = FDAPI_getaddrinfo;
w_ntohl = FDAPI_ntohlFDAPI_ntohl;
w_ntohs = FDAPI_ntohs;
 w_getnameinfo = FDAPI_getnameinfo;
*/
auto f_WSACleanup = WSACleanup;//dllfunctor_stdcall<int>("ws2_32.dll", "WSACleanup");
auto f_WSAFDIsSet = __WSAFDIsSet;// dllfunctor_stdcall<int, SOCKET, fd_set*>("ws2_32.dll", "__WSAFDIsSet");
auto f_WSAGetLastError = WSAGetLastError;// dllfunctor_stdcall<int>("ws2_32.dll", "WSAGetLastError");
auto f_WSAGetOverlappedResult = WSAGetOverlappedResult;// dllfunctor_stdcall<BOOL, SOCKET, LPWSAOVERLAPPED, LPDWORD, BOOL, LPDWORD>("ws2_32.dll", "WSAGetOverlappedResult");
auto f_WSADuplicateSocket = WSADuplicateSocketW;//dllfunctor_stdcall<int, SOCKET, DWORD, LPWSAPROTOCOL_INFO>("ws2_32.dll", "WSADuplicateSocketW");
auto f_WSAIoctl = WSAIoctl;// dllfunctor_stdcall<int, SOCKET, DWORD, LPVOID, DWORD, LPVOID, DWORD, LPVOID, LPWSAOVERLAPPED, LPWSAOVERLAPPED_COMPLETION_ROUTINE>("ws2_32.dll", "WSAIoctl");
auto f_WSARecv = WSARecv;// dllfunctor_stdcall<int, SOCKET, LPWSABUF, DWORD, LPDWORD, LPDWORD, LPWSAOVERLAPPED, LPWSAOVERLAPPED_COMPLETION_ROUTINE>("ws2_32.dll", "WSARecv");
auto f_WSASocket = WSASocketW;// dllfunctor_stdcall<SOCKET, int, int, int, LPWSAPROTOCOL_INFO, GROUP, DWORD>("ws2_32.dll", "WSASocketW");
auto f_WSASend = WSASend;// dllfunctor_stdcall<int, SOCKET, LPWSABUF, DWORD, LPDWORD, DWORD, LPWSAOVERLAPPED, LPWSAOVERLAPPED_COMPLETION_ROUTINE>("ws2_32.dll", "WSASend");
auto f_WSAStartup = WSAStartup;// dllfunctor_stdcall<int, WORD, LPWSADATA>("ws2_32.dll", "WSAStartup");
auto f_ioctlsocket = ioctlsocket;// dllfunctor_stdcall<int, SOCKET, long, u_long*>("ws2_32.dll", "ioctlsocket");
auto f_WSARecvFrom = WSARecvFrom;// dllfunctor_stdcall<int, SOCKET, LPWSABUF, DWORD, LPDWORD, LPDWORD, struct sockaddr*, int*, LPWSAOVERLAPPED, LPWSAOVERLAPPED_COMPLETION_ROUTINE>("ws2_32.dll", "WSARecvFrom");
auto f_WSASendTo = WSASendTo;// dllfunctor_stdcall<int, SOCKET, LPWSABUF, DWORD, LPDWORD, DWORD, struct sockaddr*, int*, LPWSAOVERLAPPED, LPWSAOVERLAPPED_COMPLETION_ROUTINE>("ws2_32.dll", "WSASendTo");

auto f_accept = accept;// dllfunctor_stdcall<SOCKET, SOCKET, struct sockaddr*, int*>("ws2_32.dll", "accept");
auto f_bind = bind;// dllfunctor_stdcall<int, SOCKET, const struct sockaddr*, int>("ws2_32.dll", "bind");
auto f_closesocket = closesocket;// dllfunctor_stdcall<int, SOCKET>("ws2_32.dll", "closesocket");
auto f_connect = connect;// dllfunctor_stdcall<int, SOCKET, const struct sockaddr*, int>("ws2_32.dll", "connect");
auto f_gethostbyname = gethostbyname;// dllfunctor_stdcall<struct hostent*, const char*>("ws2_32.dll", "gethostbyname");
auto f_getpeername = getpeername;// dllfunctor_stdcall<int, SOCKET, struct sockaddr*, int*>("ws2_32.dll", "getpeername");
auto f_getsockname = getsockname;// dllfunctor_stdcall<int, SOCKET, struct sockaddr*, int*>("ws2_32.dll", "getsockname");
auto f_getsockopt = getsockopt;// dllfunctor_stdcall<int, SOCKET, int, int, char*, int*>("ws2_32.dll", "getsockopt");
auto f_htonl = htonl;//  dllfunctor_stdcall<u_long, u_long>("ws2_32.dll", "htonl");
auto f_htons = htons;// dllfunctor_stdcall<u_short, u_short>("ws2_32.dll", "htons");
auto f_listen = listen;// dllfunctor_stdcall<int, SOCKET, int>("ws2_32.dll", "listen");
auto f_ntohs = ntohs;// dllfunctor_stdcall<u_short, u_short>("ws2_32.dll", "ntohs");
auto f_ntohl = ntohl;// dllfunctor_stdcall<u_long, u_long>("ws2_32.dll", "ntohl");
auto f_recv = recv;// dllfunctor_stdcall<int, SOCKET, char*, int, int>("ws2_32.dll", "recv");
auto f_select = select;// dllfunctor_stdcall<int, int, fd_set*, fd_set*, fd_set*, const struct timeval*>("ws2_32.dll", "select");
auto f_send = send;// dllfunctor_stdcall<int, SOCKET, const char*, int, int>("ws2_32.dll", "send");
auto f_setsockopt = setsockopt;// dllfunctor_stdcall<int, SOCKET, int, int, const char*, int>("ws2_32.dll", "setsockopt");
auto f_socket = socket;// dllfunctor_stdcall<SOCKET, int, int, int>("ws2_32.dll", "socket");
auto f_recvfrom = recvfrom;// dllfunctor_stdcall<int, SOCKET, char*, unsigned int, int, struct sockaddr*, int*>("ws2_32.dll", "recvfrom");
auto f_sendto = sendto;// dllfunctor_stdcall<int, SOCKET, char*, unsigned int, int, struct sockaddr*, int*>("ws2_32.dll", "sendto");
auto f_gethostbyaddr = gethostbyaddr;// dllfunctor_stdcall<struct hostent*, struct sockaddr*, int, int>("ws2_32.dll", "gethostbyaddr");
auto f_shutdown = shutdown;// dllfunctor_stdcall<int, SOCKET, int>("ws2_32.dll", "shutdown");
auto f_inet_ntoa = inet_ntoa;// dllfunctor_stdcall<char*, struct sockaddr>("ws2_32.dll", "inet_ntoa");

#ifndef SIO_LOOPBACK_FAST_PATH
const DWORD SIO_LOOPBACK_FAST_PATH = 0x98000010;	// from Win8 SDK
#endif

void EnableFastLoopback(SOCKET socket) {
    // If Win8+ (6.2), use fast path option on loopback
    if (WindowsVersion::getInstance().IsAtLeast_6_2()) {
        int enabled = 1;
        DWORD result_byte_count = -1;
        int result = f_WSAIoctl(socket, SIO_LOOPBACK_FAST_PATH, &enabled, sizeof(enabled), NULL, 0, &result_byte_count, NULL, NULL);
        if (result != 0) {
            throw std::system_error(f_WSAGetLastError(), system_category(), "WSAIoctl failed");
        }
    }
}

static fnWSIOCP_CloseSocketStateRFD* wsiocp_CloseSocketState;
void FDAPI_SetCloseSocketState(fnWSIOCP_CloseSocketStateRFD* func) {
    wsiocp_CloseSocketState = func;
}

int FDAPI_WSAGetLastError(void) {
    return f_WSAGetLastError();
}

int FDAPI_WSAFDIsSet(SOCKET s, fd_set* set) {
    return f_WSAFDIsSet(s, set);
}

BOOL FDAPI_WSAGetOverlappedResult(int rfd, LPWSAOVERLAPPED lpOverlapped, LPDWORD lpcbTransfer, BOOL fWait, LPDWORD lpdwFlags) {
    try {
        SOCKET socket = RFDMap::getInstance().lookupSocket(rfd);
        if (socket != INVALID_SOCKET) {
            return f_WSAGetOverlappedResult(socket, lpOverlapped, lpcbTransfer, fWait, lpdwFlags);
        }
    } CATCH_AND_REPORT();

    errno = EBADF;
    return SOCKET_ERROR;
}

/* This method should only be called to close the sockets duplicated
 * by the child process.
 */
BOOL FDAPI_CloseDuplicatedSocket(int rfd) {
    try {
        SOCKET socket = RFDMap::getInstance().lookupSocket(rfd);
        if (socket != INVALID_SOCKET) {
            RFDMap::getInstance().removeRFDToSocketInfo(rfd);
            RFDMap::getInstance().removeSocketToRFD(socket);
            return f_closesocket(socket);
        }
    } CATCH_AND_REPORT();

    errno = EBADF;
    return FALSE;
}

int FDAPI_WSADuplicateSocket(int rfd, DWORD dwProcessId, LPWSAPROTOCOL_INFO lpProtocolInfo) {
    try {
        SOCKET socket = RFDMap::getInstance().lookupSocket(rfd);
        if (socket != INVALID_SOCKET) {
            return f_WSADuplicateSocket(socket, dwProcessId, lpProtocolInfo);
        }
    } CATCH_AND_REPORT();

    errno = EBADF;
    return SOCKET_ERROR;
}

int FDAPI_WSASocket(int af, int type, int protocol, LPWSAPROTOCOL_INFO lpProtocolInfo, GROUP g, DWORD dwFlags) {
    try {
        SOCKET socket = f_WSASocket(af,
            type,
            protocol,
            lpProtocolInfo,
            g,
            dwFlags);

        if (socket != INVALID_SOCKET) {
            return RFDMap::getInstance().addSocket(socket);
        }
    } CATCH_AND_REPORT();

    return -1;
}

int FDAPI_WSASend(int rfd, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesSent, DWORD dwFlags, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine) {
    try {
        SOCKET socket = RFDMap::getInstance().lookupSocket(rfd);
        if (socket != INVALID_SOCKET) {
            return f_WSASend(socket,
                lpBuffers,
                dwBufferCount,
                lpNumberOfBytesSent,
                dwFlags,
                lpOverlapped,
                lpCompletionRoutine);
        }
    } CATCH_AND_REPORT();

    errno = EBADF;
    return SOCKET_ERROR;
}

int FDAPI_WSARecv(int rfd, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesRecvd, LPDWORD lpFlags, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine) {
    try {
        SOCKET socket = RFDMap::getInstance().lookupSocket(rfd);
        if (socket != INVALID_SOCKET) {
            return f_WSARecv(socket,
                lpBuffers,
                dwBufferCount,
                lpNumberOfBytesRecvd,
                lpFlags,
                lpOverlapped,
                lpCompletionRoutine);
        }
    } CATCH_AND_REPORT();

    errno = EBADF;
    return SOCKET_ERROR;
}

int FDAPI_WSASendTo(int rfd, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesSent, DWORD dwFlags, struct sockaddr* addr, int len, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine) {
    try {
        SOCKET socket = RFDMap::getInstance().lookupSocket(rfd);
        if (socket != INVALID_SOCKET) {
            return f_WSASendTo(socket,
                lpBuffers,
                dwBufferCount,
                lpNumberOfBytesSent,
                dwFlags,
                addr,
                len,
                lpOverlapped,
                lpCompletionRoutine);
        }
    } CATCH_AND_REPORT();

    errno = EBADF;
    return SOCKET_ERROR;
}

int FDAPI_WSARecvFrom(int rfd, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesRecvd, LPDWORD lpFlags, struct sockaddr* addr, int* len,  LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine) {
    try {
        SOCKET socket = RFDMap::getInstance().lookupSocket(rfd);
        if (socket != INVALID_SOCKET) {
            return f_WSARecvFrom(socket,
                lpBuffers,
                dwBufferCount,
                lpNumberOfBytesRecvd,
                lpFlags,
                addr,
                len,
                lpOverlapped,
                lpCompletionRoutine);
        }
    } CATCH_AND_REPORT();

    errno = EBADF;
    return SOCKET_ERROR;
}

int FDAPI_WSAIoctl(int rfd, DWORD dwIoControlCode, LPVOID lpvInBuffer, DWORD cbInBuffer, LPVOID lpvOutBuffer, DWORD cbOutBuffer, LPDWORD lpcbBytesReturned, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine) {
    try {
        SOCKET socket = RFDMap::getInstance().lookupSocket(rfd);
        if (socket != INVALID_SOCKET) {
            if (f_WSAIoctl(socket,
                dwIoControlCode,
                lpvInBuffer,
                cbInBuffer,
                lpvOutBuffer,
                cbOutBuffer,
                lpcbBytesReturned,
                lpOverlapped,
                lpCompletionRoutine) == 0) {
                return 0;
            } else {
                errno = f_WSAGetLastError();
                return SOCKET_ERROR;
            }
        }
    } CATCH_AND_REPORT();

    errno = EBADF;
    return SOCKET_ERROR;
}

void FDAPI_SaveSocketAddrStorage(int rfd, SOCKADDR_STORAGE* socketAddrStorage) {
    SocketInfo* socket_info = RFDMap::getInstance().lookupSocketInfo(rfd);
    if (socket_info != NULL) {
        memcpy(&(socket_info->socketAddrStorage), socketAddrStorage, sizeof(SOCKADDR_STORAGE));
    }
}

BOOL FDAPI_SocketAttachIOCP(int rfd, HANDLE iocph) {
    SOCKET socket = RFDMap::getInstance().lookupSocket(rfd);
    if (socket != INVALID_SOCKET) {
        // Set the socket to nonblocking mode
        DWORD yes = 1;
        if (f_ioctlsocket(socket, FIONBIO, &yes) != SOCKET_ERROR) {
            // Make the socket non-inheritable
            if (SetHandleInformation((HANDLE) socket, HANDLE_FLAG_INHERIT, 0)) {
                // Associate it with the I/O completion port.
                // Use the rfd as the completion key.
                if (CreateIoCompletionPort((HANDLE) socket,
                                           iocph,
                                           (ULONG_PTR) rfd,
                                           0) != NULL) {
                    return TRUE;
                }
            }
        }
        errno = f_WSAGetLastError();
    } else {
        errno = EBADF;
    }
    return FALSE;
}

BOOL FDAPI_SocketAttachIOCP2(int rfd, HANDLE iocph) {
    SOCKET socket = RFDMap::getInstance().lookupSocket(rfd);
    if (socket != INVALID_SOCKET) {
        // Make the socket non-inheritable
        if (SetHandleInformation((HANDLE)socket, HANDLE_FLAG_INHERIT, 0)) {
            // Associate it with the I/O completion port.
            // Use the rfd as the completion key.
            if (CreateIoCompletionPort((HANDLE)socket,
                iocph,
                (ULONG_PTR)rfd,
                0) != NULL) {
                return TRUE;
            }
        }

        errno = f_WSAGetLastError();
    }
    else {
        errno = EBADF;
    }
    return FALSE;
}

BOOL FDAPI_AcceptEx(int listenFD, int acceptFD, PVOID lpOutputBuffer, DWORD dwReceiveDataLength, DWORD dwLocalAddressLength, DWORD dwRemoteAddressLength, LPDWORD lpdwBytesReceived, LPOVERLAPPED lpOverlapped) {
    try {
        SOCKET sListen = RFDMap::getInstance().lookupSocket(listenFD);
        SOCKET sAccept = RFDMap::getInstance().lookupSocket(acceptFD);
        if (sListen != INVALID_SOCKET &&  sAccept != INVALID_SOCKET) {
            LPFN_ACCEPTEX acceptex;
            const GUID wsaid_acceptex = WSAID_ACCEPTEX;
            DWORD bytes;

            if (SOCKET_ERROR == f_WSAIoctl(sListen,
                                           SIO_GET_EXTENSION_FUNCTION_POINTER,
                                           (void *) &wsaid_acceptex,
                                           sizeof(GUID),
                                           &acceptex,
                                           sizeof(LPFN_ACCEPTEX),
                                           &bytes,
                                           NULL,
                                           NULL)) {
                return FALSE;
            }
            return acceptex(sListen, sAccept, lpOutputBuffer, dwReceiveDataLength, dwLocalAddressLength, dwRemoteAddressLength, lpdwBytesReceived, lpOverlapped);
        }
    } CATCH_AND_REPORT();

    return FALSE;
}

BOOL FDAPI_ConnectEx(int rfd, const struct sockaddr *name, int namelen, PVOID lpSendBuffer, DWORD dwSendDataLength, LPDWORD lpdwBytesSent, LPOVERLAPPED lpOverlapped) {
    try {
        SOCKET socket = RFDMap::getInstance().lookupSocket(rfd);
        if (socket != INVALID_SOCKET) {
            LPFN_CONNECTEX connectex;
            const GUID wsaid_connectex = WSAID_CONNECTEX;
            DWORD bytes;

            if (SOCKET_ERROR == f_WSAIoctl(socket,
                                           SIO_GET_EXTENSION_FUNCTION_POINTER,
                                           (void *) &wsaid_connectex,
                                           sizeof(GUID),
                                           &connectex,
                                           sizeof(LPFN_ACCEPTEX),
                                           &bytes,
                                           NULL,
                                           NULL)) {
                return FALSE;
            }

            EnableFastLoopback(socket);

            return connectex(socket, 
                             name,namelen,
                             lpSendBuffer,
                             dwSendDataLength,
                             lpdwBytesSent,
                             lpOverlapped);
        }
    } CATCH_AND_REPORT();

    return FALSE;
}

void FDAPI_GetAcceptExSockaddrs(int rfd, PVOID lpOutputBuffer, DWORD dwReceiveDataLength, DWORD dwLocalAddressLength, DWORD dwRemoteAddressLength, LPSOCKADDR *LocalSockaddr, LPINT LocalSockaddrLength, LPSOCKADDR *RemoteSockaddr, LPINT RemoteSockaddrLength) {
    try {
        SOCKET socket = RFDMap::getInstance().lookupSocket(rfd);
        if (socket != INVALID_SOCKET) {
            LPFN_GETACCEPTEXSOCKADDRS getacceptsockaddrs;
            const GUID wsaid_getacceptsockaddrs = WSAID_GETACCEPTEXSOCKADDRS;
            DWORD bytes;

            if (SOCKET_ERROR == f_WSAIoctl(socket,
                                           SIO_GET_EXTENSION_FUNCTION_POINTER,
                                           (void *) &wsaid_getacceptsockaddrs,
                                           sizeof(GUID),
                                           &getacceptsockaddrs,
                                           sizeof(LPFN_ACCEPTEX),
                                           &bytes,
                                           NULL,
                                           NULL)) {
                return;
            }

            getacceptsockaddrs(lpOutputBuffer, 
                               dwReceiveDataLength,
                               dwLocalAddressLength,
                               dwRemoteAddressLength,
                               LocalSockaddr,
                               LocalSockaddrLength,
                               RemoteSockaddr,
                               RemoteSockaddrLength);
        }
    } CATCH_AND_REPORT();
}

int FDAPI_UpdateAcceptContext(int rfd) {
    try {
        SOCKET socket = RFDMap::getInstance().lookupSocket(rfd);
        if (socket != INVALID_SOCKET) {
            return f_setsockopt(socket,
                                SOL_SOCKET,
                                SO_UPDATE_ACCEPT_CONTEXT,
                                (char*) &socket,
                                sizeof(SOCKET));
        }
    } CATCH_AND_REPORT();

    errno = EBADF;
    return SOCKET_ERROR;
}

void** FDAPI_GetSocketStatePtr(int rfd) {
    SocketInfo* socket_info = RFDMap::getInstance().lookupSocketInfo(rfd);
    if (socket_info == NULL) {
        return NULL;
    } else {
        return &(socket_info->state);
    }
}

void FDAPI_ClearSocketInfo(int rfd) {
    SocketInfo* socketInfo = RFDMap::getInstance().lookupSocketInfo(rfd);

    ASSERT(socketInfo != NULL);
    ASSERT(socketInfo->socket == INVALID_SOCKET);

    if (socketInfo != NULL) {
        if (socketInfo->socket == INVALID_SOCKET) {
            RFDMap::getInstance().removeRFDToSocketInfo(rfd);
            return;
        } else {
            redisLog(REDIS_WARNING, "FDAPI_ClearSocketInfo called on non closed socket.");
        }
    } else {
        redisLog(REDIS_WARNING, "FDAPI_ClearSocketInfo called on non attached socket.");
    }
}

int FDAPI_PipeSetNonBlock(int rfd, int non_blocking) {
    try {
        int crt_fd = RFDMap::getInstance().lookupCrtFD(rfd);
        if (crt_fd != INVALID_FD) {
            HANDLE h = (HANDLE) crt_get_osfhandle(crt_fd);
            if (h == INVALID_HANDLE_VALUE) {
                errno = EBADF;
                return -1;
            }

            if (GetFileType(h) == FILE_TYPE_PIPE) {
                /* h is a pipe or socket.  */
                DWORD state;
                if (GetNamedPipeHandleState(h, &state, NULL, NULL, NULL, NULL, 0)) {
                    /* h is a pipe.  */
                    if ((state & PIPE_NOWAIT) != 0) {
                        if (non_blocking)
                            return 0;
                        state &= ~PIPE_NOWAIT;
                    } else {
                        if (!non_blocking)
                            return 0;
                        state |= PIPE_NOWAIT;
                    }
                    if (SetNamedPipeHandleState(h, &state, NULL, NULL)) {
                        return 0;
                    }
                    errno = EINVAL;
                    return -1;
                } else {
                    /* h is a socket.  */
                    errno = EINVAL;
                    return -1;
                }
            } else {
                /* Win32 does not support non-blocking on regular files.  */
                if (!non_blocking) {
                    return 0;
                }
                errno = ENOTSUP;
                return -1;
            }
            return 0;
        }
    } CATCH_AND_REPORT();

    errno = EBADF;
    return -1;
}

int FDAPI_pipe(int *pfds) {
    int result = -1;
    try {
        // Not passing _O_NOINHERIT, the underlying handles are inheritable by default
        result = crt_pipe(pfds, 8192, _O_BINARY);
        if (result == 0) {
            pfds[0] = RFDMap::getInstance().addCrtFD(pfds[0]);
            pfds[1] = RFDMap::getInstance().addCrtFD(pfds[1]);
        }
    } CATCH_AND_REPORT();

    return result;
}

int FDAPI_socket(int af, int type, int protocol) {
    try {
        SOCKET socket = f_socket(af, type, protocol);
        if (socket != INVALID_SOCKET) {
            return RFDMap::getInstance().addSocket(socket);
        } else {
            errno = f_WSAGetLastError();
            return -1;
        }
    } CATCH_AND_REPORT();

    errno = EBADF;
    return -1;
}

// In unix a fd is a fd. All are closed with close().
int FDAPI_close(int rfd) {
    try {
        SocketInfo* socketInfo = RFDMap::getInstance().lookupSocketInfo(rfd);
        if (socketInfo != NULL) {

            ASSERT(socketInfo->socket != INVALID_SOCKET);

            if (socketInfo->socket != INVALID_SOCKET) {
                SOCKET socket = socketInfo->socket;
                socketInfo->socket = INVALID_SOCKET;

                if (socketInfo->state != NULL) {
                    if (wsiocp_CloseSocketState != NULL) {
                        if (wsiocp_CloseSocketState(rfd)) {
                            RFDMap::getInstance().removeRFDToSocketInfo(rfd);
                        }
                    }
                } else {
                    RFDMap::getInstance().removeRFDToSocketInfo(rfd);
                }
                RFDMap::getInstance().removeSocketToRFD(socket);
                return f_closesocket(socket);
            }
        } else {
            int crt_fd = RFDMap::getInstance().lookupCrtFD(rfd);
            if (crt_fd != INVALID_FD) {
                RFDMap::getInstance().removeCrtFD(crt_fd);
                return crt_close(crt_fd);
            }
        }
    } CATCH_AND_REPORT();

    errno = EBADF;
    return -1;
}

int FDAPI_open(const char * _Filename, int _OpenFlag, int flags = 0) {
    try {
        int crt_fd = crt_open(_Filename, _OpenFlag, flags);
        if (crt_fd != INVALID_FD) {
            return RFDMap::getInstance().addCrtFD(crt_fd);
        }
    } CATCH_AND_REPORT();

    errno = GetLastError();
    return -1;
}

int FDAPI_accept(int rfd, struct sockaddr *addr, socklen_t *addrlen) {
    try {
        SOCKET socket = RFDMap::getInstance().lookupSocket(rfd);
        if (socket != INVALID_SOCKET) {
            SOCKET sAccept = f_accept(socket, addr, addrlen);
            if (sAccept != INVALID_SOCKET) {
                return RFDMap::getInstance().addSocket(sAccept);
            } else {
                errno = f_WSAGetLastError();
                if ((errno == ENOENT) || (errno == WSAEWOULDBLOCK)) {
                    errno = EAGAIN;
                    return -1;
                }
            }
        }
    } CATCH_AND_REPORT();

    errno = EBADF;
    return -1;
}

int FDAPI_setsockopt(int rfd, int level, int optname, const void *optval, socklen_t optlen) {
    try {
        SOCKET socket = RFDMap::getInstance().lookupSocket(rfd);
        if (socket != INVALID_SOCKET) {
            if (f_setsockopt(socket, level, optname, (const char*) optval, optlen) == 0) {
                return 0;
            } else {
                errno = f_WSAGetLastError();
                return -1;
            }
        }
    } CATCH_AND_REPORT();

    errno = EBADF;
    return -1;
}

int FDAPI_fcntl(int rfd, int cmd, int flags = 0 ) {
    try {
        SocketInfo* socket_info = RFDMap::getInstance().lookupSocketInfo(rfd);
        if (socket_info != NULL && socket_info->socket != INVALID_SOCKET) {
            switch (cmd) {
                case F_GETFL:
                {
                    // Since in WinSock there is no way to determine if a socket
                    // is blocking, we keep track of this separately.
                    return socket_info->flags;
                }
                case F_SETFL:
                {
                    u_long fionbio_flags = (flags & O_NONBLOCK);
                    if (SOCKET_ERROR == f_ioctlsocket(socket_info->socket,
                        FIONBIO,
                        &fionbio_flags)) {
                        errno = f_WSAGetLastError();
                        return -1;
                    } else {
                        socket_info->flags = flags;
                        return 0;
                    }
                    break;
                }
                default:
                {
                    ASSERT(cmd == F_GETFL || cmd == F_SETFL);
                    return -1;
                }
            }
        }
    } CATCH_AND_REPORT();

    errno = EBADF;
    return -1;
}

int FDAPI_poll(struct pollfd *fds, nfds_t nfds, int timeout) {
    try {
        struct pollfd* pollCopy = new struct pollfd[nfds];
        if (pollCopy == NULL) {
            errno = ENOMEM;
            return -1;
        }

        // NOTE: Treating the fds.fd as a Redis file descriptor and converting to a SOCKET for WSAPoll. 
        for (nfds_t n = 0; n < nfds; n ++) {
            pollCopy[n].fd = RFDMap::getInstance().lookupSocket((RFD)(fds[n].fd));
            pollCopy[n].events = fds[n].events;
            pollCopy[n].revents = fds[n].revents;
        }

        if (WindowsVersion::getInstance().IsAtLeast_6_0()) {
            static auto f_WSAPoll = dllfunctor_stdcall<int, WSAPOLLFD*, ULONG, INT>("ws2_32.dll", "WSAPoll");

            // WSAPoll implementation has a bug that cause the client
            // to wait forever on a non-existant endpoint
            // See https://github.com/MSOpenTech/redis/issues/214
            int ret = f_WSAPoll(pollCopy, nfds, timeout);

            for (nfds_t n = 0; n < nfds; n++) {
                fds[n].events = pollCopy[n].events;
                fds[n].revents = pollCopy[n].revents;
            }

            delete pollCopy;
            pollCopy = NULL;

            return ret;
        } else {
            int ret;
            fd_set readSet;
            fd_set writeSet;
            fd_set excepSet;

            FD_ZERO(&readSet);
            FD_ZERO(&writeSet);
            FD_ZERO(&excepSet);

            if (nfds >= FD_SETSIZE) {
                errno = EINVAL;
                return -1;
            }

            nfds_t i;
            for (i = 0; i < nfds; i++) {
                if (fds[i].fd == INVALID_SOCKET) {
                    continue;
                }
                if (pollCopy[i].fd >= FD_SETSIZE) {
                    errno = EINVAL;
                    return -1;
                }

                if (pollCopy[i].events & POLLIN) FD_SET(pollCopy[i].fd, &readSet);
                if (pollCopy[i].events & POLLOUT) FD_SET(pollCopy[i].fd, &writeSet);
                if (pollCopy[i].events & POLLERR) FD_SET(pollCopy[i].fd, &excepSet);
            }

            if (timeout < 0) {
                ret = w_select(0, &readSet, &writeSet, &excepSet, NULL);
            } else {
                struct timeval tv;
                tv.tv_sec = timeout / 1000;
                tv.tv_usec = 1000 * (timeout % 1000);
                ret = w_select(0, &readSet, &writeSet, &excepSet, &tv);
            }

            if (ret < 0) {
                return ret;
            }

            for (i = 0; i < nfds; i++) {
                fds[i].revents = 0;

                if (f_WSAFDIsSet(pollCopy[i].fd, &readSet)) fds[i].revents |= POLLIN;
                if (f_WSAFDIsSet(pollCopy[i].fd, &writeSet)) fds[i].revents |= POLLOUT;
                if (f_WSAFDIsSet(pollCopy[i].fd, &excepSet)) fds[i].revents |= POLLERR;
            }

            delete pollCopy;
            pollCopy = NULL;

            return ret;
        }
    } CATCH_AND_REPORT();

    errno = EBADF;
    return -1;
}

int FDAPI_shutdown(int rfd, int how) {
    try {
        SOCKET socket = RFDMap::getInstance().lookupSocket(rfd);
        if (socket != INVALID_SOCKET) {
            return f_shutdown(socket, how);
        }
    } CATCH_AND_REPORT();

    errno = EBADF;
    return -1;
}

int FDAPI_getsockopt(int rfd, int level, int optname, void *optval, socklen_t *optlen) {
    try {
        SOCKET socket = RFDMap::getInstance().lookupSocket(rfd);
        if (socket != INVALID_SOCKET) {
            return f_getsockopt(socket, level, optname, (char*) optval, optlen);
        }
    } CATCH_AND_REPORT();

    errno = EBADF;
    return -1;
}

int FDAPI_connect(int rfd, const struct sockaddr *addr, size_t addrlen) {
    try {
        SOCKET socket = RFDMap::getInstance().lookupSocket(rfd);
        if (socket != INVALID_SOCKET) {
            EnableFastLoopback(socket);
            int result = f_connect(socket, addr, (int) addrlen);
            errno = f_WSAGetLastError();
            if ((errno == WSAEINVAL) || (errno == WSAEWOULDBLOCK) || (errno == WSA_IO_PENDING)) {
                errno = EINPROGRESS;
            }
            return result;
        }
    } CATCH_AND_REPORT();

    errno = EBADF;
    return -1;
}

ssize_t FDAPI_read(int rfd, void *buf, size_t count) {
    try {
        SOCKET socket = RFDMap::getInstance().lookupSocket(rfd);
        if (socket != INVALID_SOCKET) {
            int retval = f_recv(socket, (char*) buf, (unsigned int) count, 0);
            if (retval == -1) {
                errno = GetLastError();
                if (errno == WSAEWOULDBLOCK) {
                    errno = EAGAIN;
                }
            }
            return retval;
        } else {
            int crt_fd = RFDMap::getInstance().lookupCrtFD(rfd);
            if (crt_fd != INVALID_FD) {
                int retval = crt_read(crt_fd, buf, (unsigned int) count);
                if (retval == -1) {
                    errno = GetLastError();
                }
                return retval;
            } else {
                errno = EBADF;
                return 0;
            }
        }
    } CATCH_AND_REPORT();

    errno = EBADF;
    return -1;
}

ssize_t FDAPI_write(int rfd, const void *buf, size_t count) {
    try {
        SOCKET socket = RFDMap::getInstance().lookupSocket(rfd);
        if (socket != INVALID_SOCKET) {
            int ret = f_send(socket, (char*) buf, (unsigned int) count, 0);
            if (ret == SOCKET_ERROR) {
                set_errno_from_last_error();
            }
            return ret;
        } else {
            int crt_fd = RFDMap::getInstance().lookupCrtFD(rfd);
            if (crt_fd != INVALID_FD) {
                if (crt_fd == _fileno(stdout)) {
                    DWORD bytesWritten = 0;
                    if (FALSE != ParseAndPrintANSIString(GetStdHandle(STD_OUTPUT_HANDLE), buf, (DWORD) count, &bytesWritten)) {
                        return (int) bytesWritten;
                    } else {
                        errno = GetLastError();
                        return 0;
                    }
                } else if (crt_fd == _fileno(stderr)) {
                    DWORD bytesWritten = 0;
                    if (FALSE != ParseAndPrintANSIString(GetStdHandle(STD_ERROR_HANDLE), buf, (DWORD) count, &bytesWritten)) {
                        return (int) bytesWritten;
                    } else {
                        errno = GetLastError();
                        return 0;
                    }
                } else {
                    int retval = crt_write(crt_fd, buf, (unsigned int) count);
                    if (retval == -1) {
                        errno = GetLastError();
                    }
                    return retval;
                }
            } else {
                errno = EBADF;
                return 0;
            }
        }
    } CATCH_AND_REPORT();

    errno = EBADF;
    return -1;
}

ssize_t FDAPI_recvfrom(int rfd, const void* buf, size_t count, int flags, struct sockaddr* addr, socklen_t* addrlen) {
    try {
        SOCKET socket = RFDMap::getInstance().lookupSocket(rfd);
        if (socket != INVALID_SOCKET) {
            int retval = f_recvfrom(socket, (char*)buf, (unsigned int)count, flags, addr, addrlen);
            if (retval == -1) {
                errno = GetLastError();
                if (errno == WSAEWOULDBLOCK) {
                    errno = EAGAIN;
                }
            }
            return retval;
        }
    } CATCH_AND_REPORT();

    errno = EBADF;
    return -1;
}

ssize_t FDAPI_sendto(int rfd, void* buf, size_t count, int flags, struct sockaddr* addr, socklen_t addrlen) {
    try {
        SOCKET socket = RFDMap::getInstance().lookupSocket(rfd);
        if (socket != INVALID_SOCKET) {
            int ret = f_sendto(socket, (char*)buf, (unsigned int)count, flags, addr, addrlen);
            if (ret == SOCKET_ERROR) {
                set_errno_from_last_error();
            }
            return ret;
        }
    } CATCH_AND_REPORT();

    errno = EBADF;
    return -1;
}

int FDAPI_fsync(int rfd) {
    try {
        int crt_fd = RFDMap::getInstance().lookupCrtFD(rfd);
        if (crt_fd != INVALID_FD) {
            HANDLE h = (HANDLE) crt_get_osfhandle(crt_fd);
            if (h == INVALID_HANDLE_VALUE) {
                errno = EBADF;
                return -1;
            }

            if (!FlushFileBuffers(h)) {
                DWORD err = GetLastError();
                switch (err) {
                    case ERROR_INVALID_HANDLE:
                        errno = EINVAL;
                        break;

                    default:
                        errno = EIO;
                }
                return -1;
            }
        }
        return 0;
    } CATCH_AND_REPORT();

    errno = EBADF;
    return -1;
}

int FDAPI_fstat64(int rfd, struct __stat64 *buffer) {
    try {
        int crt_fd = RFDMap::getInstance().lookupCrtFD(rfd);
        if (crt_fd != INVALID_FD) {
            return _fstat64(crt_fd, buffer);
        }
    } CATCH_AND_REPORT();

    errno = EBADF;
    return -1;
}

int FDAPI_listen(int rfd, int backlog) {
    try {
        SOCKET socket = RFDMap::getInstance().lookupSocket(rfd);
        if (socket != INVALID_SOCKET) {
            EnableFastLoopback(socket);
            int result = f_listen(socket, backlog);
            if (result != 0){
                errno = f_WSAGetLastError();
            }
            return result;
        } else {
            errno = EBADF;
            return -1;
        }
    } CATCH_AND_REPORT();

    errno = EBADF;
    return -1;
}

int FDAPI_ftruncate(int rfd, PORT_LONGLONG length) {
    try {
        int crt_fd = RFDMap::getInstance().lookupCrtFD(rfd);
        if (crt_fd != INVALID_FD) {
            HANDLE h = (HANDLE) crt_get_osfhandle(crt_fd);

            if (h == INVALID_HANDLE_VALUE) {
                errno = EBADF;
                return -1;
            }

            LARGE_INTEGER l, o;
            l.QuadPart = length;

            if (!SetFilePointerEx(h, l, &o, FILE_BEGIN)) return -1;
            if (!SetEndOfFile(h)) return -1;

            return 0;
        }
    } CATCH_AND_REPORT();

    errno = EBADF;
    return -1;
}

int FDAPI_bind(int rfd, const struct sockaddr *addr, socklen_t addrlen) {
    try {
        SOCKET socket = RFDMap::getInstance().lookupSocket(rfd);
        if (socket != INVALID_SOCKET) {
            return f_bind(socket, addr, addrlen);
        } else {
            errno = EBADF;
            return 0;
        }
    } CATCH_AND_REPORT();

    errno = EBADF;
    return -1;
}

int FDAPI_getpeername(int rfd, struct sockaddr *addr, socklen_t * addrlen) {
    try {
        SOCKET socket = RFDMap::getInstance().lookupSocket(rfd);
        if (socket != INVALID_SOCKET) {
            int result = f_getpeername(socket, addr, addrlen);
            // Workaround for getpeername failing to retrieve the endpoint address
            if (result != 0) {
                SocketInfo* socket_info = RFDMap::getInstance().lookupSocketInfo(rfd);
                if (socket_info != NULL) {
                    memcpy(addr, &(socket_info->socketAddrStorage), sizeof(SOCKADDR_STORAGE));
                    *addrlen = sizeof(SOCKADDR_STORAGE);
                    return 0;
                }
            }
            return result;
        }
    } CATCH_AND_REPORT();

    errno = EBADF;
    return SOCKET_ERROR;
}

int FDAPI_getsockname(int rfd, struct sockaddr* addrsock, int* addrlen) {
    try {
        SOCKET socket = RFDMap::getInstance().lookupSocket(rfd);
        if (socket != INVALID_SOCKET) {
            return f_getsockname(socket, addrsock, addrlen);
        }
    } CATCH_AND_REPORT();

    errno = EBADF;
    return SOCKET_ERROR;
}

int FDAPI_setmode(int fd, int mode) {
    return crt_setmode(fd, mode);
}

size_t FDAPI_fwrite(const void *buffer, size_t size, size_t count, FILE *file) {
    return crt_fwrite(buffer, size, count, file);
}

int FDAPI_fclose(FILE *file) {
    int crt_fd = crt_fileno(file);
    if (crt_fd != INVALID_FD) {
        RFDMap::getInstance().removeCrtFD(crt_fd);
    }
    return crt_fclose(file);
}

int FDAPI_fileno(FILE *file) {
    int crt_fd = crt_fileno(file);
    if (crt_fd != INVALID_FD) {
        // If crt_fd is already mapped, addCrtFD() will return the existing rfd.
        return RFDMap::getInstance().addCrtFD(crt_fd);
    }
    return -1;
}

int FDAPI_select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout) {
    try {
        if (readfds != NULL) {
            for (u_int r = 0; r < readfds->fd_count; r++) {
                readfds->fd_array[r] = RFDMap::getInstance().lookupSocket((RFD) readfds->fd_array[r]);
            }
        }
        if (writefds != NULL) {
            for (u_int r = 0; r < writefds->fd_count; r++) {
                writefds->fd_array[r] = RFDMap::getInstance().lookupSocket((RFD) writefds->fd_array[r]);
            }
        }
        if (exceptfds != NULL) {
            for (u_int r = 0; r < exceptfds->fd_count; r++) {
                exceptfds->fd_array[r] = RFDMap::getInstance().lookupSocket((RFD) exceptfds->fd_array[r]);
            }
        }

        return f_select(nfds, readfds, writefds, exceptfds, timeout);
    } CATCH_AND_REPORT();

    errno = EBADF;
    return SOCKET_ERROR;
}

int FDAPI_isatty(int rfd) {
    try {
        int crt_fd = RFDMap::getInstance().lookupCrtFD(rfd);
        if (crt_fd != INVALID_FD) {
            return crt_isatty(crt_fd);
        }
    } CATCH_AND_REPORT();

    errno = EBADF;
    return -1;
}

int FDAPI_access(const char *pathname, int mode) {
    return crt_access(pathname, mode);
}

u_int64 FDAPI_lseek64(int rfd, u_int64 offset, int whence) {
    try {
        int crt_fd = RFDMap::getInstance().lookupCrtFD(rfd);
        if (crt_fd != INVALID_FD) {
            return crt_lseek64(crt_fd, offset, whence);
        }
    } CATCH_AND_REPORT();

    errno = EBADF;
    return -1;
}

intptr_t FDAPI_get_osfhandle(RFD rfd) {
    try {
        int crt_fd = RFDMap::getInstance().lookupCrtFD(rfd);
        if (crt_fd != INVALID_FD) {
            return crt_get_osfhandle(crt_fd);
        }
    } CATCH_AND_REPORT();

    errno = EBADF;
    return -1;
}

int FDAPI_open_osfhandle(intptr_t osfhandle, int flags) {
    try {
        int crt_fd = crt_open_osfhandle(osfhandle, flags);
        if (crt_fd != INVALID_FD) {
            return RFDMap::getInstance().addCrtFD(crt_fd);
        }
    } CATCH_AND_REPORT();

    errno = EBADF;
    return -1;
}

const char* FDAPI_inet_ntop(int af, const void *src, char *dst, size_t size) {

    static auto f_WSAAddressToStringA = WSAAddressToStringA;// dllfunctor_stdcall<int, LPSOCKADDR, DWORD, LPWSAPROTOCOL_INFO, LPSTR, LPDWORD>("ws2_32.dll", "WSAAddressToStringA");
    struct sockaddr_in srcaddr;

    memset(&srcaddr, 0, sizeof(struct sockaddr_in));
    memcpy(&(srcaddr.sin_addr), src, sizeof(srcaddr.sin_addr));

    srcaddr.sin_family = af;
    if (f_WSAAddressToStringA((struct sockaddr*) &srcaddr, sizeof(struct sockaddr_in), 0, dst, (LPDWORD)&size) != 0) {
        return NULL;
    }
    return dst;

}

int FDAPI_inet_pton(int family, const char* src, void* dst) {
    static auto f_WSAStringToAddressA = WSAStringToAddressA;// dllfunctor_stdcall<int, LPSTR, INT, LPWSAPROTOCOL_INFO, LPSOCKADDR, LPINT>("ws2_32.dll", "WSAStringToAddressA");
    struct sockaddr ss;
    int size = sizeof(ss);
    ZeroMemory(&ss, sizeof(ss));

    char src_copy[INET6_ADDRSTRLEN + 1];
    strncpy(src_copy, src, INET6_ADDRSTRLEN + 1);
    src_copy[INET6_ADDRSTRLEN] = 0;
    /* Non-Const API*/
    if (f_WSAStringToAddressA(src_copy, family, NULL, (struct sockaddr *)&ss, &size) == 0) {
        switch (family) {
            case AF_INET:
                *(struct in_addr *)dst = ((struct sockaddr_in *)&ss)->sin_addr;
                return 1;
            case AF_INET6:
                *(struct in6_addr *)dst = ((struct sockaddr_in6 *)&ss)->sin6_addr;
                return 1;
        }
    }
    return 0;
}

BOOL ParseStorageAddress(const char *ip, int port, SOCKADDR_STORAGE* pStorageAddr) {
    struct addrinfo hints, *res;
    int status;
    char port_buffer[6];

    sprintf(port_buffer, "%hu", port);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    // Setting AI_PASSIVE will give you a wildcard address if addr is NULL
    hints.ai_flags = AI_NUMERICSERV | AI_PASSIVE;

    if ((status = getaddrinfo(ip, port_buffer, &hints, &res)) != 0) {
        return FALSE;
    }

    // Note, we're taking the first valid address, there may be more than one
    memcpy(pStorageAddr, res->ai_addr, res->ai_addrlen);

    freeaddrinfo(res);
    return TRUE;
}

int InitWinsock() {
    WSADATA t_wsa;
    WORD wVers;
    int iError;

    wVers = MAKEWORD(2, 2);
    iError = f_WSAStartup(wVers, &t_wsa);

    if (iError != NO_ERROR || LOBYTE(t_wsa.wVersion) != 2 || HIBYTE(t_wsa.wVersion) != 2) {
        exit(1);
    } else {
        return 0;
    }
}

int CleanupWinsock() {
    return f_WSACleanup();
}

class Win32_FDSockMap {
public:
    static Win32_FDSockMap& getInstance() {
        static Win32_FDSockMap instance; // Instantiated on first use. Guaranteed to be destroyed.
        return instance;
    }

private:
    Win32_FDSockMap() {
        InitWinsock();

        w_accept = FDAPI_accept;
        w_access = FDAPI_access;
        w_bind = FDAPI_bind;
        w_connect = FDAPI_connect;
        w_fcntl = FDAPI_fcntl;
        w_fdapi_fstat64 = (fdapi_fstat) FDAPI_fstat64;
        w_fsync = FDAPI_fsync;
        w_ftruncate = FDAPI_ftruncate;
        w_getsockopt = FDAPI_getsockopt;
        w_getpeername = FDAPI_getpeername;
        w_getsockname = FDAPI_getsockname;
        w_inet_ntop = FDAPI_inet_ntop;
        w_inet_pton = FDAPI_inet_pton;
        w_isatty = FDAPI_isatty;
        w_listen = FDAPI_listen;
        w_lseek64 = FDAPI_lseek64;
        w_open = FDAPI_open;
        w_pipe = FDAPI_pipe;
        w_poll = FDAPI_poll;
        w_read = FDAPI_read;
        w_select = FDAPI_select;
        w_setsockopt = FDAPI_setsockopt;
        w_socket = FDAPI_socket;
        w_write = FDAPI_write;
        w_sendto = FDAPI_sendto;
        w_recvfrom = FDAPI_recvfrom;
        w_shutdown = FDAPI_shutdown;
    }

    ~Win32_FDSockMap() {
        CleanupWinsock();
    }

    Win32_FDSockMap(Win32_FDSockMap const&);    // Don't implement to guarantee singleton semantics
    void operator=(Win32_FDSockMap const&);     // Don't implement to guarantee singleton semantics
};

// guarantee global initialization
static class Win32_FDSockMap& init = Win32_FDSockMap::getInstance();

