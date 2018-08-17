/**
 * @file tr64c_posix.c
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
#ifndef PCF_IS_LINUX
#error "POSIX backend is not supported for this target."
#endif
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>


/**
 * Internal list of IP addresses of a single host.
 */
struct sIpAddress {
	struct addrinfo * list;
	struct addrinfo * entry;
};


/**
 * Internal network handles.
 */
struct sNetHandle {
	struct addrinfo * list;
	int socket;
};


#include "tr64c.h"


#ifdef UNICODE
#error "Build configuration not supported. Please undefine UNICODE."
#endif


/**
 * Checks if the given path exists and is not a directory.
 * 
 * @param[in] src - check this path
 * @return 1 if src exists and is not a directory, else 0
 */
int isFile(const TCHAR * src) {
	struct stat stats;
	if (stat(src, &stats) == 0 && !S_ISDIR(stats.st_mode)) return 1;
	return 0;
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
	FILE * fd = NULL;
	struct stat stats;
	char * str = NULL;
	char * res = NULL;
	
	fd = fopen(src, "r");
	if (fd == NULL) goto onError;
	
	if (fstat(fileno(fd), &stats) != 0) goto onError;
	
	str = (char *)malloc((size_t)(stats.st_size) + 1);
	if (str == NULL) goto onError;
	
	if (fread(str, (size_t)(stats.st_size), 1, fd) < 1) goto onError;
	str[stats.st_size] = 0;
	
	res = str;
	if (len != NULL) *len = (size_t)(stats.st_size);
onError:
	if (fd != NULL) fclose(fd);
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
	FILE * fd = NULL;
	int res = 0;
	
	fd = fopen(dst, "w");
	if (fd == NULL) goto onError;
	
	if (fwrite(str, len, 1, fd) < 1) goto onError;
	
	res = 1;
onError:
	if (fd != NULL) fclose(fd);
	return res;
}


/**
 * Initializes the backend API.
 * 
 * @return 1 on success, else 0
 */
int initBackend(void) {
	return 1;
}


/**
 * De-initializes the backend API.
 * 
 * @return 1 on success, else 0
 */
void deinitBackend(void) {
}


/**
 * Helper function to output the last errno to the given file descriptor.
 * 
 * @param[in,out] fd - output to this file descriptor
 * @param[in] err - error to translate
 */
static void printLastError(FILE * fd) {
	_ftprintf(fd, _T("%s\n"), strerror(errno));
}


/**
 * Helper function to output the given error to the given file descriptor.
 * 
 * @param[in,out] fd - output to this file descriptor
 * @param[in] err - error to translate
 */
static void printWsaError(FILE * fd, int err) {
	_ftprintf(fd, _T("%s\n"), gai_strerror(err));
}


/**
 * Helper function to return a point in time in milliseconds.
 * 
 * @return time point in milliseconds
 */
static uint64_t getTimePoint() {
	struct timeval t;
	if (gettimeofday(&t, NULL) != 0) return 0;
	return (((uint64_t)t.tv_sec) * 1000) + (((uint64_t)t.tv_usec) / 1000);
}


/**
 * Helper function to output the given address to the passed file descriptor.
 * 
 * @param[in] fd - output to this file descriptor
 * @param[in] addr - output this address
 * @param[in] addrLen - length of addr in bytes
 */
static void printAddress(FILE * fd, void * addr, size_t addrLen) {
	char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
	int err;
	
	err = getnameinfo((const struct sockaddr *)addr, (socklen_t)addrLen, hbuf, sizeof(hbuf), sbuf, sizeof(sbuf), NI_NUMERICHOST | NI_NUMERICSERV);
	if (err != 0) {
		printWsaError(ferr, err);
		return;
	}
	_ftprintf(fd, _T("%s:%s"), hbuf, sbuf);
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
	struct timeval timeoutBase = {
		.tv_sec = TIMEOUT_RESOLUTION / 1000,
		.tv_usec = (TIMEOUT_RESOLUTION % 1000) * 1000
	};
	struct timeval timeout;
	fd_set event;
	int sock = -1;
	ssize_t size;
	char * endPtr;
	const unsigned long port = strtoul(ctx->port, &endPtr, 10);
	uint64_t startTime, durationStart;
	
	ctx->status = 400;
	ctx->duration = (size_t)-1;
	durationStart = getTimePoint();
	
	/* verify given port */
	if (endPtr == NULL || *endPtr != 0 || port < 1 || port > 0xFFFF) {
		if (ctx->verbose > 0) _ftprintf(ferr, MSGT(MSGT_ERR_OPT_SSDP_BAD_PORT));
		goto onError;
	}
	
	/* create socket */
	sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock < 0) {
		if (ctx->verbose > 0) _ftprintf(ferr, MSGT(MSGT_ERR_SOCK_NEW));
		if (ctx->verbose > 1) printLastError(ferr);
		goto onError;
	}
	
	/* configure the socket */
	{
		/* enable port number re-usage */
		int val = 1;
		sRes = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char *)(&val), sizeof(val));
		if (sRes != 0) {
			if (ctx->verbose > 0) _ftprintf(ferr, MSGT(MSGT_ERR_SOCK_ON_REUSE));
			if (ctx->verbose > 1) printLastError(ferr);
			goto onError;
		}
	}
#if defined(IP_MTU_DISCOVER) && defined(IP_PMTUDISC_DO)
	{
		/* disable packet fragmentation */
		int val = IP_PMTUDISC_DO;
		sRes = setsockopt(sock, IPPROTO_IP, IP_MTU_DISCOVER, (const char *)(&val), sizeof(val));
		if (sRes != 0) {
			if (ctx->verbose > 0) _ftprintf(ferr, MSGT(MSGT_ERR_SOCK_OFF_FRAG));
			if (ctx->verbose > 1) printLastError(ferr);
			goto onError;
		}
	}
#endif
	{
		/* disable loop-back for multicasts */
		int val = 0;
		sRes = setsockopt(sock, IPPROTO_IP, IP_MULTICAST_LOOP, (const char *)(&val), sizeof(val));
		if (sRes != 0) {
			if (ctx->verbose > 0) _ftprintf(ferr, MSGT(MSGT_ERR_SOCK_OFF_MC_LB));
			if (ctx->verbose > 1) printLastError(ferr);
			goto onError;
		}
	}
	{
		/* send timeout */
		struct timeval val = {
			.tv_sec = ctx->timeout / 1000,
			.tv_usec = (ctx->timeout % 1000) * 1000
		};
		sRes = setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char *)(&val), sizeof(val));
		if (sRes != 0) {
			if (ctx->verbose > 0) _ftprintf(ferr, MSGT(MSGT_ERR_SOCK_SET_SEND_TOUT));
			if (ctx->verbose > 1) printLastError(ferr);
			goto onError;
		}
	}
	
	/* bind to SSDP multicast port on local interface to receive the responses */
	inAddr.sin_family = AF_INET;
	/* make sure we send the request over the right interface */
	inAddr.sin_addr.s_addr = inet_addr(localIf);
	/* need to bind to a random port to receive the responses or it will clash with the Windows SSDP service or other UDP listeners on this port */
	inAddr.sin_port = 0;
	sRes = bind(sock, (const struct sockaddr *)(&inAddr), (socklen_t)sizeof(inAddr));
	if (sRes != 0) {
		if (ctx->verbose > 0) _ftprintf(ferr, MSGT(MSGT_ERR_SOCK_BIND_SSDP));
		if (ctx->verbose > 1) printLastError(ferr);
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
		if (ctx->verbose > 1) printLastError(ferr);
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
		int val = MULTICAST_TTL;
		sRes = setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, (const char *)(&val), sizeof(val));
		if (sRes != 0) {
			if (ctx->verbose > 0) _ftprintf(ferr, MSGT(MSGT_ERR_SOCK_SET_MC_TTL));
			if (ctx->verbose > 1) printLastError(ferr);
			goto onError;
		}
	}
	
	if (signalReceived != 0) goto onError;
	
	/* send out discovery request */
	outAddr.sin_family = AF_INET;
	outAddr.sin_addr.s_addr = inet_addr(ctx->host);
	outAddr.sin_port = htons((uint16_t)port);
	do {
		size = sendto(sock, ctx->buffer, ctx->length, 0, (const struct sockaddr *)(&outAddr), (socklen_t)sizeof(outAddr));
		if (size <= 0) {
			if (ctx->verbose > 0) {
				_ftprintf(ferr, MSGT(MSGT_ERR_SOCK_SEND_SSDP_REQ));
				if (ctx->verbose > 1) printLastError(ferr);
			}
			goto onError;
		}
		if (ctx->verbose > 2) _ftprintf(ferr, MSGT(MSGT_INFO_SSDP_SENT), (unsigned)size);
	} while ((size_t)size != ctx->length);
	
	if (signalReceived != 0) goto onError;
	
	{
		/* configure socket non-blocking */
		int flags = fcntl(sock, F_GETFL, 0);
		if (flags == -1) {
			if (ctx->verbose > 0) _ftprintf(ferr, MSGT(MSGT_ERR_SOCK_NON_BLOCK));
			if (ctx->verbose > 1) printLastError(ferr);
			goto onError;
		}
		if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) != 0) {
			if (ctx->verbose > 0) _ftprintf(ferr, MSGT(MSGT_ERR_SOCK_NON_BLOCK));
			if (ctx->verbose > 1) printLastError(ferr);
			goto onError;
		}
	}
	
	/* receive responses until error or timeout */
	startTime = getTimePoint();
	for (;;) {
		struct sockaddr_in src = {0};
		socklen_t srcLen = (socklen_t)sizeof(src);
		FD_ZERO(&event);
		FD_SET(sock, &event);
		/* wait for data or timeout */
		timeout = timeoutBase;
		sRes = select(sock + 1, &event, NULL, NULL, &timeout);
		if (sRes < 0) {
			goto onError;
		} else if (sRes == 0) {
			goto onReceiveTimeout;
		}
		/* get received data */
		size = recvfrom(sock, ctx->buffer, ctx->capacity, 0, (struct sockaddr *)(&src), &srcLen);
		if (size <= 0 && errno != EAGAIN && errno != EWOULDBLOCK) break;
		if (src.sin_port == htons((uint16_t)port)) {
			ctx->length = (size_t)size;
			if (ctx->verbose > 2) _ftprintf(ferr, MSGT(MSGT_INFO_SSDP_RECV), (unsigned)size);
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
onReceiveTimeout:
		/* check configured timeout */
		{
			const uint64_t val = UINT_OVERFLOW_OP(getTimePoint(), -, startTime);
			if (val > (uint64_t)(ctx->timeout)) break; /* timeout */
		}
		if (signalReceived != 0) goto onError;
	}
	
	ret = 1;
onError:
	ctx->duration = (size_t)(UINT_OVERFLOW_OP(getTimePoint(), -, durationStart));
	if (sock != -1) {
		if (joinedGroup != 0) {
			sRes = setsockopt(sock, IPPROTO_IP, IP_DROP_MEMBERSHIP, (const char *)(&mreq), sizeof(mreq));
			if (sRes != 0 && ctx->verbose > 0) {
				_ftprintf(ferr, MSGT(MSGT_ERR_SOCK_LEAVE_MC_GROUP));
				if (ctx->verbose > 1) printLastError(ferr);
			}
		}
		close(sock);
	}
	return ret;
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
	const struct addrinfo * addr = NULL;
	struct timeval timeoutBase = {
		.tv_sec = TIMEOUT_RESOLUTION / 1000,
		.tv_usec = (TIMEOUT_RESOLUTION % 1000) * 1000
	};
	struct timeval timeout;
	fd_set event;
	ssize_t size;
	int sRes, res = 0, auth = 0;
	tTr64Response response = {0};
	uint64_t startTime, durationStart;
	
	ctx->status = 400;
	ctx->duration = (size_t)-1;
	durationStart = getTimePoint();
	
	if (ctx->auth != NULL) {
		auth = 1; /* performing authentication of the previous request */
		free(ctx->auth);
		ctx->auth = NULL;
	}
	
	ctx->content = NULL;
	
	if (ctx->net->socket != -1 && ctx->net->list != ctx->address->list) {
		shutdown(ctx->net->socket, SHUT_RDWR);
		close(ctx->net->socket);
		ctx->net->socket = -1;
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
	if (ctx->net->socket == -1) {
		/* create socket */
		ctx->net->socket = socket(addr->ai_family, SOCK_STREAM, IPPROTO_TCP);
		if (ctx->net->socket == -1) {
			if (ctx->verbose > 0) _ftprintf(ferr, MSGT(MSGT_ERR_SOCK_NEW));
			if (ctx->verbose > 1) printLastError(ferr);
			goto onError;
		}
		
		/* configure the socket */
		{
			/* keep-alive */
			int val = 1;
			sRes = setsockopt(ctx->net->socket, SOL_SOCKET, SO_KEEPALIVE, (const char *)(&val), sizeof(val));
			if (sRes != 0) {
				if (ctx->verbose > 0) _ftprintf(ferr, MSGT(MSGT_ERR_SOCK_SET_ALIVE));
				if (ctx->verbose > 1) printLastError(ferr);
				goto onError;
			}
			/* disable Nagle algorithm */
			sRes = setsockopt(ctx->net->socket, IPPROTO_TCP, TCP_NODELAY, (const char *)(&val), sizeof(val));
			if (sRes != 0) {
				if (ctx->verbose > 0) _ftprintf(ferr, MSGT(MSGT_ERR_SOCK_OFF_NAGLE));
				if (ctx->verbose > 1) printLastError(ferr);
				goto onError;
			}
		}
		{
			/* send timeout */
			struct timeval val = {
				.tv_sec = ctx->timeout / 1000,
				.tv_usec = (ctx->timeout % 1000) * 1000
			};
			sRes = setsockopt(ctx->net->socket, SOL_SOCKET, SO_SNDTIMEO, (const char *)(&val), sizeof(val));
			if (sRes != 0) {
				if (ctx->verbose > 0) _ftprintf(ferr, MSGT(MSGT_ERR_SOCK_SET_SEND_TOUT));
				if (ctx->verbose > 1) printLastError(ferr);
				goto onError;
			}
		}
	
		/* connect */
		sRes = connect(ctx->net->socket, (const struct sockaddr *)(addr->ai_addr), (socklen_t)(addr->ai_addrlen));
		if (sRes != 0) {
			if (addr->ai_next == NULL) {
				if (ctx->verbose > 0) _ftprintf(ferr, MSGT(MSGT_ERR_SOCK_CONNECT));
				if (ctx->verbose > 1) printLastError(ferr);
				goto onError;
			}
			if (addr->ai_family != addr->ai_next->ai_family) {
				shutdown(ctx->net->socket, SHUT_RDWR);
				close(ctx->net->socket);
				ctx->net->socket = -1;
			}
			ctx->address->entry = addr->ai_next;
			goto onTryNext;
		}
	}
	if (signalReceived != 0) goto onError;
	
	/* send HTTP request */
	size = 0;
	for (size_t i = 0; i < ctx->length; i += (size_t)size) {
		size = send(ctx->net->socket, ctx->buffer + i, ctx->length - i, 0);
		if (size < 0) {
			switch (errno) {
			case EINTR:
				if (signalReceived != 0) goto onError;
				/* try again */
				size = 0;
				break;
			case EAGAIN:
#if EAGAIN != EWOULDBLOCK
			case EWOULDBLOCK:
#endif
				if (ctx->verbose > 0) _ftprintf(ferr, MSGT(MSGT_ERR_SOCK_SEND_TOUT));
				if (ctx->verbose > 1) printLastError(ferr);
				ctx->status = 408;
				goto onError;
				break;
			default:
				if (ctx->verbose > 0) _ftprintf(ferr, MSGT(MSGT_ERR_HTTP_SEND_REQ));
				if (ctx->verbose > 1) printLastError(ferr);
				goto onError;
				break;
			}
		}
	}
	if (signalReceived != 0) goto onError;
	
	{
		/* configure socket non-blocking */
		int flags = fcntl(ctx->net->socket, F_GETFL, 0);
		if (flags == -1) {
			if (ctx->verbose > 0) _ftprintf(ferr, MSGT(MSGT_ERR_SOCK_NON_BLOCK));
			if (ctx->verbose > 1) printLastError(ferr);
			goto onError;
		}
		if (fcntl(ctx->net->socket, F_SETFL, flags | O_NONBLOCK) != 0) {
			if (ctx->verbose > 0) _ftprintf(ferr, MSGT(MSGT_ERR_SOCK_NON_BLOCK));
			if (ctx->verbose > 1) printLastError(ferr);
			goto onError;
		}
	}
	
	/* receive HTTP response */
	size = 0;
	startTime = getTimePoint();
	for (ctx->length = 0; ctx->length < ctx->capacity; ) {
		FD_ZERO(&event);
		FD_SET(ctx->net->socket, &event);
		/* wait for data or timeout */
		timeout = timeoutBase;
		sRes = select(ctx->net->socket + 1, &event, NULL, NULL, &timeout);
		if (sRes < 0) {
			goto onError;
		} else if (sRes == 0) {
			goto onReceiveTimeout;
		}
		/* get received data */
		if (response.content.start != NULL && response.content.length > 0) {
			size = recv(ctx->net->socket, ctx->buffer + ctx->length, response.content.start + response.content.length - ctx->buffer - ctx->length, 0);
		} else {
			size = recv(ctx->net->socket, ctx->buffer + ctx->length, ctx->capacity - ctx->length, 0);
		}
		if (size < 0) {
			switch (errno) {
			case EINTR:
				if (signalReceived != 0) goto onError;
				/* try again */
				continue;
				break;
			case EAGAIN:
#if EAGAIN != EWOULDBLOCK
			case EWOULDBLOCK:
#endif
				goto onReceiveTimeout;
				break;
			default:
				if (ctx->verbose > 0) _ftprintf(ferr, MSGT(MSGT_ERR_HTTP_RECV_RESP));
				if (ctx->verbose > 1) printLastError(ferr);
				goto onError;
				break;
			}
			break; /* timeout -> assume successful completion */
		} else if (size == 0) {
			/* peer closed the connection */
			break;
		}
		if (ctx->verbose > 3) _ftprintf(ferr, MSGT(MSGT_DBG_SOCK_RECV), (unsigned)size);
		if ((size_t)(ctx->length + size) > MAX_RESPONSE_SIZE) goto onError; /* received response exceeds our defined limits */
		ctx->length += (size_t)size;
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
			const uint64_t val = UINT_OVERFLOW_OP(getTimePoint(), -, startTime);
			if (val > (uint64_t)(ctx->timeout)) {
				if (ctx->verbose > 0) _ftprintf(ferr, MSGT(MSGT_ERR_SOCK_RECV_TOUT));
				if (ctx->verbose > 1) printLastError(ferr);
				ctx->status = 408;
				goto onError; /* incomplete */
			}
			if (signalReceived != 0) goto onError;
		}
	}
onSuccess:
	res = 1;
onError:
	ctx->duration = (size_t)(UINT_OVERFLOW_OP(getTimePoint(), -, durationStart));
	if (ctx->address != NULL) ctx->address->entry = NULL;
	if (res == 0) {
		/* reset socket on error */
		if (ctx->net->socket != -1) {
			shutdown(ctx->net->socket, SHUT_RDWR);
			close(ctx->net->socket);
			ctx->net->socket = -1;
		}
	}
	return res;
}


/**
 * Resolves the host and port strings to native addresses within the given context.
 * 
 * @param[in,out] ctx - context to use
 * @return 1 on success, else 0
 */
static int resolve(tTr64RequestCtx * ctx) {
	uint64_t durationStart;
	if (ctx == NULL) return 0;
	
	tIpAddress * res = NULL;
	
	struct addrinfo hints = {0};
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	
	ctx->duration = (size_t)-1;
	durationStart = getTimePoint();
	
	res = (tIpAddress *)malloc(sizeof(tIpAddress));
	if (res == NULL) goto onError;
	res->entry = NULL;
	
	if (getaddrinfo(ctx->host, ctx->port, &hints, &(res->list)) != 0) goto onError;
	
	ctx->address = res;
	ctx->duration = (size_t)(UINT_OVERFLOW_OP(getTimePoint(), -, durationStart));
	
	return 1;
onError:
	ctx->duration = (size_t)(UINT_OVERFLOW_OP(getTimePoint(), -, durationStart));
	if (res != NULL) free(res);
	return 0;
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
	if (ctx->net != NULL && ctx->net->socket != -1) {
		shutdown(ctx->net->socket, SHUT_RDWR);
		close(ctx->net->socket);
		ctx->net->list = NULL;
		ctx->net->socket = -1;
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
	for (const struct addrinfo * addr = ctx->address->list; addr != NULL; addr = addr->ai_next) {
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
	res->net->socket = -1;
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
		if (ctx->address->list != NULL) freeaddrinfo(ctx->address->list);
		free(ctx->address);
	}
	if (ctx->net != NULL) {
		if (ctx->net->socket != -1) {
			shutdown(ctx->net->socket, SHUT_RDWR);
			close(ctx->net->socket);
		}
		free(ctx->net);
	}
	if (ctx->buffer != NULL) free(ctx->buffer);
	free(ctx);
}
