/**
 * @file tr64c_winsocks.c
 * @author Daniel Starke
 * @date 2018-06-21
 * @version 2018-08-17
 * 
 * DISCLAIMER
 * This file has no copyright assigned and is placed in the Public Domain.
 * All contributions are also assumed to be in the Public Domain.
 * Other contributions are not permitted.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */
#ifndef PCF_IS_WIN
#error "WinSocks backend is not supported for this target."
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>


#ifdef _MSC_VER
#pragma comment(lib, "Ws2_32.lib")
#endif


/**
 * Internal list of IP addresses of a single host.
 */
struct sIpAddress {
	ADDRINFOT * list;
	ADDRINFOT * entry;
};


/**
 * Internal network handles.
 */
struct sNetHandle {
	ADDRINFOT * list;
	SOCKET socket;
};


#include "tr64c.h"


/**
 * Checks if the given path exists and is not a directory.
 * 
 * @param[in] src - check this path
 * @return 1 if src exists and is not a directory, else 0
 */
int isFile(const TCHAR * src) {
	if (src == NULL) return 0;
	DWORD dwAttrib = GetFileAttributes(src);
	return (dwAttrib != INVALID_FILE_ATTRIBUTES && (dwAttrib & FILE_ATTRIBUTE_DIRECTORY) == 0) ? 1 : 0;
}


/**
 * Reads the given file as a single null-terminated allocated UTF-8 string.
 * 
 * @param[in] src - input file path
 * @param[out] len - optional pointer for the resulting length
 * @return null-terminated string on success, else NULL
 */
char * readFileToString(const TCHAR * src, size_t * len) {
	if (src == NULL) return NULL;
	HANDLE hFile = INVALID_HANDLE_VALUE;
	LARGE_INTEGER lpFileSize;
	DWORD lpNumberOfBytesRead;
	char * str = NULL;
	char * res = NULL;
	
	hFile = CreateFile(src, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) goto onError;
	
	if (GetFileSizeEx(hFile, &lpFileSize) == 0) goto onError;
	if (lpFileSize.QuadPart > 0xFFFFFFFE) goto onError; /* 32-bit limit */
	
	str = (char *)malloc((size_t)(lpFileSize.QuadPart) + 1);
	if (str == NULL) goto onError;
	
	if (ReadFile(hFile, str, (DWORD)(lpFileSize.QuadPart), &lpNumberOfBytesRead, NULL) == 0) goto onError;
	if (lpNumberOfBytesRead != (DWORD)(lpFileSize.QuadPart)) goto onError;
	str[lpNumberOfBytesRead] = 0;
	
	res = str;
	if (len != NULL) *len = (size_t)lpNumberOfBytesRead;
onError:
	if (hFile != INVALID_HANDLE_VALUE) CloseHandle(hFile);
	if (res == NULL) {
		if (str != NULL) free(str);
		if (len != NULL) *len = 0;
	}
	return res;
}


/**
 * Write a null-terminated UTF-8 string to the given file. Existing files will be overwritten.
 * 
 * @param[in] dst - output file path
 * @param[in] str - null-terminated input string
 * @return 1 on success, else 0
 */
int writeStringToFile(const TCHAR * dst, const char * str) {
	return writeStringNToFile(dst, str, strlen(str));
}


/**
 * Write a UTF-8 string to the given file. Existing files will be overwritten.
 * 
 * @param[in] dst - output file path
 * @param[in] str - input string
 * @param[in] len - length of the input string in bytes
 * @return 1 on success, else 0
 */
int writeStringNToFile(const TCHAR * dst, const char * str, const size_t len) {
	if (dst == NULL || str == NULL) return 0;
	HANDLE hFile = INVALID_HANDLE_VALUE;
	DWORD lpNumberOfBytesWritten;
	int res = 0;
	
	hFile = CreateFile(dst, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) goto onError;
	
	if (WriteFile(hFile, str, (DWORD)len, &lpNumberOfBytesWritten, NULL) == 0) goto onError;
	if (lpNumberOfBytesWritten != (DWORD)len) goto onError;
	
	res = 1;
onError:
	if (hFile != INVALID_HANDLE_VALUE) CloseHandle(hFile);
	return res;
}


/**
 * Initializes the backend API.
 * 
 * @return 1 on success, else 0
 */
int initBackend(void) {
	WORD wVersionRequested = MAKEWORD(2, 2);
	WSADATA wsaData;
	const int err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0) {
		return 0;
	}
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
		return 0;
	}
	return 1;
}


/**
 * De-initializes the backend API.
 * 
 * @return 1 on success, else 0
 */
void deinitBackend(void) {
	WSACleanup();
}


/**
 * Helper function to output the error returned from WSAGetLastError() to the given file descriptor.
 * 
 * @param[in,out] fd - output to this file descriptor
 */
static void printLastWsaError(FILE * fd) {
	TCHAR * str = NULL;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
		(LPTSTR)&str,
		0,
		NULL
	);
	_ftprintf(fd, _T("%s\n"), str);
	LocalFree(str);
}


/**
 * Helper function to output the given address to the passed file descriptor.
 * 
 * @param[in] fd - output to this file descriptor
 * @param[in] addr - output this address
 * @param[in] addrLen - length of addr in bytes
 */
static void printAddress(FILE * fd, void * addr, size_t addrLen) {
	TCHAR addrStrBuf[64] = {0};
	DWORD addrStrBufSize = (DWORD)sizeof(addrStrBuf);
	WSAAddressToString((LPSOCKADDR)addr, (DWORD)addrLen, NULL, addrStrBuf, &addrStrBufSize);
	_ftprintf(fd, _T("%s"), addrStrBuf);
}


/**
 * Performs a local discovery for TR-064 compatible devices (SSPD search). The buffer of ctx is send
 * out for the discovery request and used to receive the answers.
 * 
 * @param[in,out] ctx - context to use
 * @param[in] localIf - perform discovery on interface with this IP
 * @param[in] visitor - callback function called for each response message
 * @param[in,out] user - user defined callback function parameter
 * @return 1 on success, else 0
 * @see http://www.winsocketdotnetworkprogramming.com/winsock2programming/winsock2advancedmulticast9a.html
 */
static int discover(struct tTr64RequestCtx * ctx, const char * localIf, int (* visitor)(const char *, const size_t, void *), void * user) {
	if (ctx == NULL || localIf == NULL || visitor == NULL) return 0;
	if (ctx->verbose > 3) _ftprintf(ferr, MSGT(MSGT_DBG_ENTER_DISCOVER));
	int sRes, joinedGroup = 0, ret = 0;
	tTr64Response response = {0};
	struct sockaddr_in inAddr = {0};
	struct sockaddr_in outAddr = {0};
	struct ip_mreq mreq = {0};
	SOCKET sock = INVALID_SOCKET;
	char * endPtr;
	const unsigned long port = strtoul(ctx->port, &endPtr, 10);
	DWORD startTime, durationStart;
	
	ctx->status = 400;
	ctx->duration = (size_t)-1;
	durationStart = GetTickCount();
	
	/* verify given port */
	if (endPtr == NULL || *endPtr != 0 || port < 1 || port > 0xFFFF) {
		if (ctx->verbose > 0) _ftprintf(ferr, MSGT(MSGT_ERR_OPT_SSDP_BAD_PORT));
		goto onError;
	}
	
	/* create socket */
	sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock == INVALID_SOCKET) {
		if (ctx->verbose > 0) _ftprintf(ferr, MSGT(MSGT_ERR_SOCK_NEW));
		if (ctx->verbose > 1) printLastWsaError(ferr);
		goto onError;
	}
	
	/* configure the socket */
	{
		/* enable port number re-usage */
		BOOL val = TRUE;
		sRes = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char *)(&val), sizeof(val));
		if (sRes != 0) {
			if (ctx->verbose > 0) _ftprintf(ferr, MSGT(MSGT_ERR_SOCK_ON_REUSE));
			if (ctx->verbose > 1) printLastWsaError(ferr);
			goto onError;
		}
		/* disable packet fragmentation */
		sRes = setsockopt(sock, IPPROTO_IP, IP_DONTFRAGMENT, (const char *)(&val), sizeof(val));
		if (sRes != 0) {
			if (ctx->verbose > 0) _ftprintf(ferr, MSGT(MSGT_ERR_SOCK_OFF_FRAG));
			if (ctx->verbose > 1) printLastWsaError(ferr);
			goto onError;
		}
	}
	{
		/* disable loop-back for multicasts */
		BOOL val = FALSE;
		sRes = setsockopt(sock, IPPROTO_IP, IP_MULTICAST_LOOP, (const char *)(&val), sizeof(val));
		if (sRes != 0) {
			if (ctx->verbose > 0) _ftprintf(ferr, MSGT(MSGT_ERR_SOCK_OFF_MC_LB));
			if (ctx->verbose > 1) printLastWsaError(ferr);
			goto onError;
		}
	}
	{
		/* receive timeout */
		DWORD val = TIMEOUT_RESOLUTION;
		sRes = setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)(&val), sizeof(val));
		if (sRes != 0) {
			if (ctx->verbose > 0) _ftprintf(ferr, MSGT(MSGT_ERR_SOCK_SET_RECV_TOUT));
			if (ctx->verbose > 1) printLastWsaError(ferr);
			goto onError;
		}
		/* send timeout */
		val = (DWORD)(ctx->timeout);
		sRes = setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char *)(&val), sizeof(val));
		if (sRes != 0) {
			if (ctx->verbose > 0) _ftprintf(ferr, MSGT(MSGT_ERR_SOCK_SET_SEND_TOUT));
			if (ctx->verbose > 1) printLastWsaError(ferr);
			goto onError;
		}
	}
	
	/* bind to SSDP multicast port on local interface to receive the responses */
	inAddr.sin_family = AF_INET;
	/* make sure we send the request over the right interface */
	inAddr.sin_addr.s_addr = inet_addr(localIf);
	/* need to bind to a random port to receive the responses or it will clash with the Windows SSDP service or other UDP listeners on this port */
	inAddr.sin_port = 0;
	sRes = bind(sock, (SOCKADDR *)(&inAddr), sizeof(inAddr));
	if (sRes != 0) {
		if (ctx->verbose > 0) _ftprintf(ferr, MSGT(MSGT_ERR_SOCK_BIND_SSDP));
		if (ctx->verbose > 1) printLastWsaError(ferr);
		goto onError;
	}
	if (ctx->verbose > 2) {
		_ftprintf(ferr, (TCHAR *)fmsg[MSGT_INFO_SOCK_BOUND_SSDP]);
		printAddress(ferr, &inAddr, sizeof(inAddr));
		_ftprintf(ferr, _T(".\n"));
	}
	
	/* join the SSDP multicast group */
	mreq.imr_multiaddr.s_addr = inet_addr(ctx->host);
	mreq.imr_interface.s_addr = inet_addr(localIf);
	sRes = setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (const char *)(&mreq), sizeof(mreq));
	if (sRes != 0) {
		if (ctx->verbose > 0) _ftprintf(ferr, MSGT(MSGT_ERR_SOCK_JOIN_MC_GROUP));
		if (ctx->verbose > 1) printLastWsaError(ferr);
		goto onError;
	}
	if (ctx->verbose > 2) {
		char * maddr = strdup(inet_ntoa(mreq.imr_multiaddr));
		if (maddr != NULL) {
			fuprintf(ferr, MSGU(MSGU_INFO_SOCK_JOINED_MC_GROUP), maddr, inet_ntoa(mreq.imr_interface), localIf);
			free(maddr);
		}
	}
	joinedGroup = 1;
	
	{
		/* set multicast TTL */
		DWORD val = MULTICAST_TTL;
		sRes = setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, (const char *)(&val), sizeof(val));
		if (sRes != 0) {
			if (ctx->verbose > 0) _ftprintf(ferr, MSGT(MSGT_ERR_SOCK_SET_MC_TTL));
			if (ctx->verbose > 1) printLastWsaError(ferr);
			goto onError;
		}
	}
	
	if (signalReceived != 0) goto onError;
	
	/* send out discovery request */
	outAddr.sin_family = AF_INET;
	outAddr.sin_addr.s_addr = inet_addr(ctx->host);
	outAddr.sin_port = htons((u_short)port);
	do {
		sRes = sendto(sock, ctx->buffer, (int)(ctx->length), 0, (SOCKADDR *)(&outAddr), (int)sizeof(outAddr));
		if (sRes <= 0) {
			if (ctx->verbose > 0) {
				_ftprintf(ferr, MSGT(MSGT_ERR_SOCK_SEND_SSDP_REQ));
				if (ctx->verbose > 1) printLastWsaError(ferr);
			}
			goto onError;
		}
		if (ctx->verbose > 2) _ftprintf(ferr, MSGT(MSGT_INFO_SSDP_SENT), (unsigned)sRes);
	} while ((size_t)sRes != ctx->length);
	
	if (signalReceived != 0) goto onError;
	
	/* receive responses until error or timeout */
	startTime = GetTickCount();
	for (;;) {
		struct sockaddr_in src = {0};
		int srcLen = (int)sizeof(src);
		sRes = recvfrom(sock, ctx->buffer, (int)(ctx->capacity), 0, (SOCKADDR *)(&src), &srcLen);
		if (sRes <= 0 && WSAGetLastError() != WSAETIMEDOUT) break;
		if (src.sin_port == htons((u_short)port)) {
			ctx->length = (size_t)sRes;
			if (ctx->verbose > 2) _ftprintf(ferr, MSGT(MSGT_INFO_SSDP_RECV), (unsigned)sRes);
			/* check if we have received the whole response */
			response.content.start = NULL;
			response.content.length = 0;
			response.status = 0;
			switch (p_http(ctx->buffer, ctx->length, NULL, httpResponseVisitor, &response)) {
			case PHRT_SUCCESS:
				ctx->status = response.status;
				if (response.status != 200) {
					if (ctx->verbose > 1) {
						const tHttpStatusMsg * item = (const tHttpStatusMsg *)bs_staticArray(&(response.status), httpStatMsg, cmpHttpStatusMsg);
						if (item != NULL) {
							_ftprintf(ferr, MSGT(MSGT_ERR_HTTP_STATUS_STR), (unsigned)response.status, item->string);
						} else  {
							_ftprintf(ferr, MSGT(MSGT_ERR_HTTP_STATUS), (unsigned)response.status);
						}
					}
					break;
				} else if (response.content.start != NULL && response.content.start != ctx->buffer && response.content.length > 0) {
					ctx->content = (char *)response.content.start;
					/* limit to actual content length */
					ctx->length = (size_t)(response.content.start + response.content.length - ctx->buffer);
				}
				if (visitor(ctx->buffer, ctx->length, user) != 1) break;
				break;
			case PHRT_UNEXPECTED_END:
				/* incomplete response -> ignore */
				break;
			default:
				goto onError;
				break;
			}
		}
		/* check configured timeout */
		const DWORD val = UINT_OVERFLOW_OP(GetTickCount(), -, startTime);
		if (val > (DWORD)(ctx->timeout)) break; /* timeout */
		if (signalReceived != 0) goto onError;
	}
	
	ret = 1;
onError:
	ctx->duration = (size_t)(UINT_OVERFLOW_OP(GetTickCount(), -, durationStart));
	if (sock != INVALID_SOCKET) {
		if (joinedGroup != 0) {
			sRes = setsockopt(sock, IPPROTO_IP, IP_DROP_MEMBERSHIP, (const char *)(&mreq), sizeof(mreq));
			if (sRes != 0 && ctx->verbose > 0) {
				_ftprintf(ferr, MSGT(MSGT_ERR_SOCK_LEAVE_MC_GROUP));
				if (ctx->verbose > 1) printLastWsaError(ferr);
			}
		}
		closesocket(sock);
	}
	return ret;
}


/**
 * Resolves the host and port strings to native addresses within the given context.
 * 
 * @param[in,out] ctx - context to use
 * @return 1 on success, else 0
 */
static int resolve(tTr64RequestCtx * ctx) {
	DWORD durationStart;
	if (ctx == NULL) return 0;
	
	TCHAR * nativeHost = NULL;
	TCHAR * nativePort = NULL;
	tIpAddress * res = NULL;
	
	ADDRINFOT hints = {0};
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	
	ctx->duration = (size_t)-1;
	durationStart = GetTickCount();
	
	nativeHost = _tfromUtf8(ctx->host);
	if (nativeHost == NULL) goto onError;
	
	nativePort = _tfromUtf8(ctx->port);
	if (nativePort == NULL) goto onError;
	
	res = (tIpAddress *)malloc(sizeof(tIpAddress));
	if (res == NULL) goto onError;
	res->entry = NULL;
	
	if (GetAddrInfo(nativeHost, nativePort, &hints, &(res->list)) != 0) goto onError;
	
	free(nativeHost);
	free(nativePort);
	ctx->address = res;
	ctx->duration = (size_t)(UINT_OVERFLOW_OP(GetTickCount(), -, durationStart));
	
	return 1;
onError:
	ctx->duration = (size_t)(UINT_OVERFLOW_OP(GetTickCount(), -, durationStart));
	if (nativeHost != NULL) free(nativeHost);
	if (nativePort != NULL) free(nativePort);
	if (res != NULL) free(res);
	return 0;
}


/**
 * Performs a HTTP request for the parameters in the given context. The internal buffer is used to
 * provide data which shall be sent to the host (with HTTP header). The result is stored in the
 * same buffer (with HTTP header). The internal socket will be left open.
 * The function sets ctx->auth to the needed authentication response if the request failed due to a
 * 401 status code. Re-sending the request with the proper authentication response will clear the
 * ctx->auth field to avoid an infinite loop if the credentials are wrong.
 * 
 * @param[in,out] ctx - context to use
 * @return 1 on success, 0 on error
 * @remarks The calling function should check if ctx->auth is set and ctx->status == 401 to perform
 * a second request with the authentication response provided by ctx->auth.
 */
static int request(tTr64RequestCtx * ctx) {
	if (ctx == NULL || ctx->length < 1) return 0;
	if (ctx->verbose > 3) _ftprintf(ferr, MSGT(MSGT_DBG_ENTER_REQUEST));
	const ADDRINFOT * addr = NULL;
	int sRes, res = 0, auth = 0;
	tTr64Response response = {0};
	DWORD startTime, durationStart;
	
	ctx->status = 400;
	ctx->duration = (size_t)-1;
	durationStart = GetTickCount();
	
	if (ctx->auth != NULL) {
		auth = 1; /* performing authentication of the previous request */
		free(ctx->auth);
		ctx->auth = NULL;
	}
	
	ctx->content = NULL;
	
	if (ctx->net->socket != INVALID_SOCKET && ctx->net->list != ctx->address->list) {
		shutdown(ctx->net->socket, SD_BOTH);
		closesocket(ctx->net->socket);
		ctx->net->socket = INVALID_SOCKET;
		ctx->net->list = NULL;
	}
	
	if (ctx->address == NULL || ctx->address->list == NULL) {
		if (ctx->verbose > 0) _ftprintf(ferr, MSGT(MSGT_ERR_OPT_NO_ADDR));
		goto onError;
	}
	ctx->net->list = ctx->address->list;
	
	if (ctx->address->entry == NULL) {
		/* restart iteration over resolved addresses */
		ctx->address->entry = ctx->address->list;
	}
	
onTryNext:
	/* try to connect to one of the given addresses */
	addr = ctx->address->entry;
	if (ctx->net->socket == INVALID_SOCKET) {
		/* create socket */
		ctx->net->socket = socket(addr->ai_family, SOCK_STREAM, IPPROTO_TCP);
		if (ctx->net->socket == INVALID_SOCKET) {
			if (ctx->verbose > 0) _ftprintf(ferr, MSGT(MSGT_ERR_SOCK_NEW));
			if (ctx->verbose > 1) printLastWsaError(ferr);
			goto onError;
		}
		
		/* configure the socket */
		{
			/* keep-alive */
			BOOL val = TRUE;
			sRes = setsockopt(ctx->net->socket, SOL_SOCKET, SO_KEEPALIVE, (const char *)(&val), sizeof(val));
			if (sRes != 0) {
				if (ctx->verbose > 0) _ftprintf(ferr, MSGT(MSGT_ERR_SOCK_SET_ALIVE));
				if (ctx->verbose > 1) printLastWsaError(ferr);
				goto onError;
			}
			/* disable Nagle algorithm */
			sRes = setsockopt(ctx->net->socket, IPPROTO_TCP, TCP_NODELAY, (const char *)(&val), sizeof(val));
			if (sRes != 0) {
				if (ctx->verbose > 0) _ftprintf(ferr, MSGT(MSGT_ERR_SOCK_OFF_NAGLE));
				if (ctx->verbose > 1) printLastWsaError(ferr);
				goto onError;
			}
		}
		{
			/* receive timeout */
			DWORD val = TIMEOUT_RESOLUTION;
			sRes = setsockopt(ctx->net->socket, SOL_SOCKET, SO_RCVTIMEO, (const char *)(&val), sizeof(val));
			if (sRes != 0) {
				if (ctx->verbose > 0) _ftprintf(ferr, MSGT(MSGT_ERR_SOCK_SET_RECV_TOUT));
				if (ctx->verbose > 1) printLastWsaError(ferr);
				goto onError;
			}
			/* send timeout */
			val = (DWORD)(ctx->timeout);
			sRes = setsockopt(ctx->net->socket, SOL_SOCKET, SO_SNDTIMEO, (const char *)(&val), sizeof(val));
			if (sRes != 0) {
				if (ctx->verbose > 0) _ftprintf(ferr, MSGT(MSGT_ERR_SOCK_SET_SEND_TOUT));
				if (ctx->verbose > 1) printLastWsaError(ferr);
				goto onError;
			}
		}
	
		/* connect */
		sRes = connect(ctx->net->socket, addr->ai_addr, (int)(addr->ai_addrlen));
		if (sRes != 0) {
			if (addr->ai_next == NULL) {
				if (ctx->verbose > 0) _ftprintf(ferr, MSGT(MSGT_ERR_SOCK_CONNECT));
				if (ctx->verbose > 1) printLastWsaError(ferr);
				goto onError;
			}
			if (addr->ai_family != addr->ai_next->ai_family) {
				shutdown(ctx->net->socket, SD_BOTH);
				closesocket(ctx->net->socket);
				ctx->net->socket = INVALID_SOCKET;
			}
			ctx->address->entry = addr->ai_next;
			goto onTryNext;
		}
	}
	if (signalReceived != 0) goto onError;
	
	/* send HTTP request */
	sRes = 0;
	for (size_t i = 0; i < ctx->length; i += (size_t)sRes) {
		sRes = send(ctx->net->socket, ctx->buffer + i, (int)(ctx->length - i), 0);
		if (sRes < 0) {
			switch (WSAGetLastError()) {
			case WSAEINPROGRESS:
				if (signalReceived != 0) goto onError;
				/* try again */
				sRes = 0;
				break;
			case WSAETIMEDOUT:
				if (ctx->verbose > 0) _ftprintf(ferr, MSGT(MSGT_ERR_SOCK_SEND_TOUT));
				if (ctx->verbose > 1) printLastWsaError(ferr);
				ctx->status = 408;
				goto onError;
				break;
			default:
				if (ctx->verbose > 0) _ftprintf(ferr, MSGT(MSGT_ERR_HTTP_SEND_REQ));
				if (ctx->verbose > 1) printLastWsaError(ferr);
				goto onError;
				break;
			}
		}
	}
	if (signalReceived != 0) goto onError;
	
	/* receive HTTP response */
	sRes = 0;
	startTime = GetTickCount();
	for (ctx->length = 0; ctx->length < ctx->capacity; ) {
		/*
		 * Receiving the last byte of the HTTP response may take up to 400ms if the peer did not set the push bit.
		 * This is due to Windows' TCP Acknowledgment Delay algorithm (see http://www.icpdas.com/root/support/faq/card/software/FAQ_Disable_TCP_ACK_Delay_en.pdf).
		 */
		if (response.content.start != NULL && response.content.length > 0) {
			sRes = recv(ctx->net->socket, ctx->buffer + ctx->length, (int)(response.content.start + response.content.length - ctx->buffer - ctx->length), 0);
		} else {
			sRes = recv(ctx->net->socket, ctx->buffer + ctx->length, (int)(ctx->capacity - ctx->length), 0);
		}
		if (sRes < 0) {
			switch (WSAGetLastError()) {
			case WSAEINPROGRESS:
				if (signalReceived != 0) goto onError;
				/* try again */
				continue;
				break;
			case WSAETIMEDOUT:
				goto onReceiveTimeout;
				break;
			default:
				if (ctx->verbose > 0) _ftprintf(ferr, MSGT(MSGT_ERR_HTTP_RECV_RESP));
				if (ctx->verbose > 1) printLastWsaError(ferr);
				goto onError;
				break;
			}
			break; /* timeout -> assume successful completion */
		} else if (sRes == 0) {
			/* peer closed the connection */
			break;
		}
		if (ctx->verbose > 3) _ftprintf(ferr, MSGT(MSGT_DBG_SOCK_RECV), (unsigned)sRes);
		if ((size_t)(ctx->length + sRes) > MAX_RESPONSE_SIZE) goto onError; /* received response exceeds our defined limits */
		ctx->length += (size_t)sRes;
		/* increase input buffer if needed */
		if (ctx->length >= ctx->capacity) {
			const size_t newCapacity = ctx->capacity << 1;
			if (newCapacity == 0) goto onError; /* overflow */
			if (arrayFieldResize(ctx, buffer, newCapacity) != 1) {
				if (ctx->verbose > 0) _ftprintf(ferr, MSGT(MSGT_ERR_NO_MEM));
				goto onError;
			}
		}
		/* check if we have already received the whole response */
		memset(&response, 0, sizeof(response));
		switch (p_http(ctx->buffer, ctx->length, NULL, httpResponseVisitor, &response)) {
		case PHRT_SUCCESS:
			ctx->status = response.status;
			if (response.status == 401 && auth == 0) {
				httpAuthentication(ctx, &response);
				goto onError;
			} else if (response.status != 200) {
				if (ctx->verbose > 1) {
					const tHttpStatusMsg * item = (const tHttpStatusMsg *)bs_staticArray(&(response.status), httpStatMsg, cmpHttpStatusMsg);
					if (item != NULL) {
						_ftprintf(ferr, MSGT(MSGT_ERR_HTTP_STATUS_STR), (unsigned)response.status, item->string);
					} else  {
						_ftprintf(ferr, MSGT(MSGT_ERR_HTTP_STATUS), (unsigned)response.status);
					}
				}
				goto onError;
			} else if (response.content.start != NULL && response.content.start != ctx->buffer && response.content.length > 0) {
				ctx->content = (char *)response.content.start;
				/* limit to actual content length */
				ctx->length = (size_t)(response.content.start + response.content.length - ctx->buffer);
			}
			goto onSuccess;
			break;
		case PHRT_UNEXPECTED_END:
			/* incomplete response */
			if (response.content.start != NULL && response.content.length > 0 && (response.content.start + response.content.length) > (ctx->buffer + ctx->capacity)) {
				const size_t newCapacity = (size_t)(response.content.start + response.content.length - ctx->buffer);
				if (newCapacity > MAX_RESPONSE_SIZE) goto onError; /* received response exceeds our defined limits */
				if (arrayFieldResize(ctx, buffer, newCapacity) != 1) {
					if (ctx->verbose > 0) _ftprintf(ferr, MSGT(MSGT_ERR_NO_MEM));
					goto onError;
				}
			}
			break;
		default:
			goto onError;
			break;
		}
		/* check configured timeout */
onReceiveTimeout:
		{
			const DWORD val = UINT_OVERFLOW_OP(GetTickCount(), -, startTime);
			if (val > (DWORD)(ctx->timeout)) {
				if (ctx->verbose > 0) _ftprintf(ferr, MSGT(MSGT_ERR_SOCK_RECV_TOUT));
				if (ctx->verbose > 1) printLastWsaError(ferr);
				ctx->status = 408;
				goto onError; /* incomplete */
			}
			if (signalReceived != 0) goto onError;
		}
	}
onSuccess:
	res = 1;
onError:
	ctx->duration = (size_t)(UINT_OVERFLOW_OP(GetTickCount(), -, durationStart));
	if (ctx->address != NULL) ctx->address->entry = NULL;
	if (res == 0) {
		/* reset socket on error */
		if (ctx->net->socket != INVALID_SOCKET) {
			shutdown(ctx->net->socket, SD_BOTH);
			closesocket(ctx->net->socket);
			ctx->net->socket = INVALID_SOCKET;
		}
	}
	return res;
}


/**
 * Close outstanding connections for a fresh start.
 * 
 * @param[in,out] ctx - context to use
 * @return 1 on success, else 0
 */
static int reset(tTr64RequestCtx * ctx) {
	if (ctx == NULL) return 0;
	if (ctx->verbose > 3) _ftprintf(ferr, MSGT(MSGT_DBG_ENTER_RESET));
	/* reset resolved address list iterator */
	if (ctx->address != NULL) {
		ctx->address->entry = NULL;
	}
	/* reset socket */
	if (ctx->net != NULL && ctx->net->socket != INVALID_SOCKET) {
		shutdown(ctx->net->socket, SD_BOTH);
		closesocket(ctx->net->socket);
		ctx->net->list = NULL;
		ctx->net->socket = INVALID_SOCKET;
	}
	return 1;
}


/**
 * Prints the resolved addresses to the given file descriptor in the native Unicode format as a
 * comma separated list.
 * 
 * @param[in] ctx - context to use
 * @param[in] fd - file descriptor to print to
 */
static void printAddresses(const tTr64RequestCtx * ctx, FILE * fd) {
	if (ctx == NULL || ctx->address == NULL) return;
	if (ctx->verbose > 3) _ftprintf(ferr, MSGT(MSGT_DBG_ENTER_PRINTADDRESS));
	int first = 1;
	for (const ADDRINFOT * addr = ctx->address->list; addr != NULL; addr = addr->ai_next) {
		if (first == 1) {
			first = 0;
		} else {
			_fputts(_T(", "), fd);
		}
		printAddress(fd, addr->ai_addr, addr->ai_addrlen);
	}
}


/**
 * Create a new HTTP request context based on the given URL.
 * 
 * @param[in] url - base on this URL
 * @param[in] user - user name
 * @param[in] pass - password
 * @param[in] format - output format type
 * @param[in] timeout - network timeouts in milliseconds
 * @param[in] verbose - verbosity level
 * @return Handle on success, else NULL.
 */
tTr64RequestCtx * newTr64Request(const char * url, const char * user, const char * pass, const tFormat format, const size_t timeout, const int verbose) {
	if (verbose > 3) _ftprintf(ferr, MSGT(MSGT_DBG_ENTER_NEWTR64REQUEST));
	if (url == NULL) return NULL;
	
	tTr64RequestCtx * res = (tTr64RequestCtx *)malloc(sizeof(tTr64RequestCtx));
	if (res == NULL) goto onError;
	memset(res, 0, sizeof(*res));
	
	if (p_url(url, (size_t)-1, urlVisitor, res) != PURT_SUCCESS || res->host == NULL) {
		if (verbose > 0) _ftprintf(ferr, MSGT(MSGT_ERR_URL_FMT));
		goto onError;
	}
	if (res->protocol == NULL) res->protocol = strdup(DEFAULT_PROTOCOL);
	if (strnicmpInternal(res->protocol, "http", 4) != 0) {
		if (verbose > 0) _ftprintf(ferr, MSGT(MSGT_ERR_URL_PROT));
		goto onError;
	}
	if (res->port == NULL) res->port = strdup(DEFAULT_PORT);
	if (user != NULL) {
		if (res->user != NULL) free(res->user);
		res->user = strdup(user);
		if (res->user == NULL) {
			if (verbose > 0) _ftprintf(ferr, MSGT(MSGT_ERR_NO_MEM));
			goto onError;
		}
	}
	if (pass != NULL) {
		if (res->pass != NULL) free(res->pass);
		res->pass = strdup(pass);
		if (res->pass == NULL) {
			if (verbose > 0) _ftprintf(ferr, MSGT(MSGT_ERR_NO_MEM));
			goto onError;
		}
	}
	
	res->format = format;
	res->timeout = timeout;
	
	res->discover = discover;
	res->resolve = resolve;
	res->net = (tNetHandle *)malloc(sizeof(tNetHandle));
	if (res->net == NULL) {
		if (verbose > 0) _ftprintf(ferr, MSGT(MSGT_ERR_NO_MEM));
		goto onError;
	}
	res->net->list = NULL;
	res->net->socket = INVALID_SOCKET;
	res->request = request;
	res->reset = reset;
	res->printAddress = printAddresses;
	res->verbose = verbose;
	
	if (arrayFieldInit(res, buffer, BUFFER_SIZE) != 1) {
		if (verbose > 0) _ftprintf(ferr, MSGT(MSGT_ERR_NO_MEM));
		goto onError;
	}
	
	return res;
onError:
	freeTr64Request(res);
	return NULL;
}


/**
 * Frees the given HTTP request context. The context is invalid after this call.
 * 
 * @param[in,out] ctx - context to free
 */
void freeTr64Request(tTr64RequestCtx * ctx) {
	if (ctx == NULL) return;
	if (ctx->verbose > 3) _ftprintf(ferr, MSGT(MSGT_DBG_ENTER_FREETR64REQUEST));
	if (ctx->protocol != NULL) free(ctx->protocol);
	if (ctx->user != NULL) free(ctx->user);
	if (ctx->pass != NULL) free(ctx->pass);
	if (ctx->host != NULL) free(ctx->host);
	if (ctx->port != NULL) free(ctx->port);
	if (ctx->path != NULL) free(ctx->path);
	if (ctx->method != NULL) free(ctx->method);
	if (ctx->auth != NULL) free(ctx->auth);
	if (ctx->address != NULL) {
		if (ctx->address->list != NULL) FreeAddrInfo(ctx->address->list);
		free(ctx->address);
	}
	if (ctx->net != NULL) {
		if (ctx->net->socket != INVALID_SOCKET) {
			shutdown(ctx->net->socket, SD_BOTH);
			closesocket(ctx->net->socket);
		}
		free(ctx->net);
	}
	if (ctx->buffer != NULL) free(ctx->buffer);
	free(ctx);
}
