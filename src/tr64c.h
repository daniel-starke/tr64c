/**
 * @file tr64c.h
 * @author Daniel Starke
 * @date 2018-06-21
 * @version 2018-08-16
 * @todo WinHTTP backend: https://social.msdn.microsoft.com/Forums/en-US/e141be2b-f621-4419-a6fb-8d86134f1f43/httpsendrequest-amp-internetreadfile-in-c?forum=vclanguage
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
#ifndef __TR64C_H__
#define __TR64C_H__

#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bsearch.h"
#include "cvutf8.h"
#include "parser.h"
#include "target.h"
#include "tchar.h"
#include "version.h"


/** Maximal HTTP response size in bytes. This prevents infinite buffer size growth. */
#define MAX_RESPONSE_SIZE 0x100000


/** Defines the default timeout for network operations in milliseconds. */
#define DEFAULT_TIMEOUT 1000


/** Defines the default protocol for TR-064. */
#define DEFAULT_PROTOCOL "http"


/** Defines the default port for TR-064. */
#define DEFAULT_PORT "49000"


/** Initial HTTP request/TR-064 query buffer size in bytes. */
#define BUFFER_SIZE 0x10000


/** Increase read line buffer by this size in bytes. */
#define LINE_BUFFER_STEP 256


/** Initial array sizes in number of elements. */
#define INIT_ARRAY_SIZE 8


/** Maximal depth of a XML node path in number of nodes. */
#define MAX_XML_DEPTH 16


/** Timeout resolution in milliseconds. Needed to handle SIGINT/SIGTERM quickly. */
#define TIMEOUT_RESOLUTION 100


/** Multicast time to live. */
#define MULTICAST_TTL 3


/** Calculates x OP y correctly on unsigned integers even on number overflow. */
#define UINT_OVERFLOW_OP(x, op, y) ((PCF_TYPEOF(x))((x) op (y)) & ((PCF_TYPEOF(x))-1))


/** Returns the given UTF-8 error message string. */
#define MSGU(x) ((const char *)fmsg[(x)])


/** Returns the given native (TCHAR) error message string. */
#define MSGT(x) ((const TCHAR *)fmsg[(x)])


typedef enum {
	GETOPT_UTF8 = 1,
	GETOPT_VERSION = 2
} tLongOption;


typedef enum {
	F_TEXT = 0,
	F_CSV,
	F_JSON,
	F_XML
} tFormat;


typedef enum {
	M_QUERY = 0,
	M_SCAN,
	M_LIST,
	M_INTERACTIVE
} tMode;


typedef enum {
	MSGT_SUCCESS = 0,
	MSGT_ERR_NO_MEM,
	MSGT_ERR_OPT_NO_ARG,
	MSGT_ERR_OPT_BAD_FORMAT,
	MSGT_ERR_OPT_BAD_TIMEOUT,
	MSGT_ERR_OPT_NO_SERVICE,
	MSGT_ERR_OPT_NO_ACTION,
	MSGT_ERR_OPT_NO_ACTION_ARG,
	MSGT_ERR_OPT_SSDP_BAD_PORT,
	MSGT_ERR_OPT_AMB_C,
	MSGT_ERR_OPT_AMB_S,
	MSGT_ERR_OPT_AMB_X,
	MSGT_ERR_OPT_ACTION_AMB,
	MSGT_ERR_OPT_BAD_ACTION,
	MSGU_ERR_OPT_NO_IN_ARG,
	MSGU_ERR_OPT_AMB_IN_ARG,
	MSGT_ERR_OPT_NO_SSDP_ADDR,
	MSGT_ERR_OPT_NO_ADDR,
	MSGT_ERR_FMT_DEV_DESC,
	MSGT_ERR_GET_DEV_DESC,
	MSGU_ERR_DEV_DESC_FMT,
	MSGT_ERR_FMT_SRVC_DESC,
	MSGT_ERR_GET_SRVC_DESC,
	MSGU_ERR_DEV_SRVC_FMT,
	MSGT_ERR_NO_DEV_IN_DESC,
	MSGU_ERR_NO_TYPE_FOR_ARG,
	MSGT_ERR_FMT_SSDP,
	MSGT_ERR_BACKEND_INIT,
	MSGT_ERR_SOCK_NEW,
	MSGT_ERR_SOCK_NON_BLOCK,
	MSGT_ERR_SOCK_ON_REUSE,
	MSGT_ERR_SOCK_OFF_MC_LB,
	MSGT_ERR_SOCK_OFF_FRAG,
	MSGT_ERR_SOCK_SET_RECV_TOUT,
	MSGT_ERR_SOCK_SET_SEND_TOUT,
	MSGT_ERR_SOCK_SET_MC_TTL,
	MSGT_ERR_SOCK_SET_ALIVE,
	MSGT_ERR_SOCK_OFF_NAGLE,
	MSGT_ERR_SOCK_BIND_SSDP,
	MSGT_ERR_SOCK_JOIN_MC_GROUP,
	MSGT_ERR_SOCK_SEND_SSDP_REQ,
	MSGT_ERR_SOCK_LEAVE_MC_GROUP,
	MSGT_ERR_SOCK_CONNECT,
	MSGT_ERR_SOCK_SEND_TOUT,
	MSGT_ERR_SOCK_RECV_TOUT,
	MSGT_ERR_HTTP_SEND_REQ,
	MSGT_ERR_HTTP_RECV_RESP,
	MSGT_ERR_HTTP_STATUS,
	MSGT_ERR_HTTP_STATUS_STR,
	MSGT_ERR_HTTP_FMT_AUTH,
	MSGT_ERR_HTTP_AUTH,
	MSGT_ERR_URL_FMT,
	MSGT_ERR_URL_PROT,
	MSGT_ERR_FMT_QUERY,
	MSGT_ERR_GET_QUERY_RESP,
	MSGT_ERR_GET_QUERY_RESP_STR,
	MSGT_ERR_QUERY_RESP_FMT,
	MSGT_ERR_QUERY_RESP_ACTION,
	MSGT_ERR_QUERY_RESP_ARG,
	MSGT_ERR_QUERY_RESP_ARG_BAD_ESC,
	MSGT_ERR_QUERY_PRINT,
	MSGT_ERR_BAD_CMD,
	MSGT_WARN_CACHE_READ,
	MSGT_WARN_CACHE_FMT,
	MSGT_WARN_CACHE_UNESC,
	MSGT_WARN_CACHE_NO_MEM,
	MSGT_WARN_CACHE_WRITE,
	MSGT_WARN_OPT_LOW_TIMEOUT,
	MSGT_WARN_LIST_NO_MEM,
	MSGT_WARN_CMD_BAD_ESC,
	MSGT_INFO_SIGTERM,
	MSGU_INFO_DEV_DESC_REQ,
	MSGT_INFO_DEV_DESC_DUR,
	MSGU_INFO_SRVC_DESC_REQ,
	MSGT_INFO_SRVC_DESC_DUR,
	MSGT_INFO_SOCK_BOUND_SSDP,
	MSGU_INFO_SOCK_JOINED_MC_GROUP,
	MSGT_INFO_SSDP_SENT,
	MSGT_INFO_SSDP_RECV,
	MSGT_DBG_SOCK_RECV,
	MSGT_DBG_BAD_TOKEN,
	MSGU_DBG_SELECTED_QUERY,
	MSGT_DBG_PARSE_QUERY_RESP,
	MSGT_DBG_OUT_QUERY_RESP,
	MSGT_DBG_ENTER_DISCOVER,
	MSGT_DBG_ENTER_REQUEST,
	MSGT_DBG_ENTER_RESET,
	MSGT_DBG_ENTER_PRINTADDRESS,
	MSGT_DBG_ENTER_NEWTR64REQUEST,
	MSGT_DBG_ENTER_FREETR64REQUEST,
	MSG_COUNT
} tMessage;


typedef enum {
	PCS_START,
	PCS_WITHIN_OBJECT,
	PCS_WITHIN_DEVICE,
	PCS_WITHIN_SERVICE,
	PCS_WITHIN_ACTION,
	PCS_WITHIN_ARG,
	PCS_END
} tPCacheState;


typedef enum {
	JT_NULL,
	JT_NUMBER,
	JT_BOOLEAN,
	JT_STRING
} tJsonType;

typedef enum {
	HAF_NONE      = 0x0000,
	HAF_CRED      = 0x0001,
	HAF_DIGEST    = 0x0002,
	HAF_REALM     = 0x0004,
	HAF_NONCE     = 0x0008,
	HAF_QOP       = 0x0010,
	HAF_AUTH      = 0x0020,
	HAF_AUTH_INT  = 0x0040,
	HAF_ALGORITHM = 0x0080,
	HAF_MD5       = 0x0100,
	HAF_MD5_SESS  = 0x0200,
	HAF_OPAQUE    = 0x0400,
	HAF_NEED      = HAF_CRED | HAF_DIGEST | HAF_REALM | HAF_NONCE,
	HAF_RFC2617   = HAF_CRED | HAF_DIGEST | HAF_REALM | HAF_NONCE | HAF_QOP
} tHttpAuthFlag;


typedef struct {
	size_t status;
	TCHAR * string;
} tHttpStatusMsg;


typedef struct {
	char * url;
	char * user;
	char * pass;
	TCHAR * cache;
	char * device;
	char * service;
	char * action;
	char ** args;
	int argCount;
#ifdef UNICODE
	int narrow;
#endif /* UNICODE */
	size_t timeout;
	int verbose;
	tFormat format;
	tMode mode;
} tOptions;


typedef struct {
	char * str;
	int strLen;
#ifdef UNICODE
	wchar_t * tmp;
	int tmpLen;
#endif /* UNICODE */
} tReadLineBuf;


typedef struct sIpAddress tIpAddress; /* internal, back-end specific */
typedef struct sNetHandle tNetHandle; /* internal, back-end specific */


typedef struct {
	tPToken content;
	size_t status;
	struct {
		tPToken realm;
		tPToken nonce;
		tPToken opaque;
		tHttpAuthFlag flags;
	} auth;
} tTr64Response;


typedef struct tTr64RequestCtx {
	char * protocol; /**< allocated protocol string (e.g. HTTP) */
	char * user; /**< allocated user name */
	char * pass; /**< allocated password */
	char * host; /**< allocated host name */
	char * port; /**< allocated port number */
	char * path; /**< allocated host path */
	char * method; /**< allocated HTTP method (e.g. POST) */
	tFormat format; /**< output format type */
	size_t timeout; /**< network timeout in milliseconds */
	size_t duration; /**< measured time span the requested option took in milliseconds */
	size_t status; /**< HTTP response status */
	size_t cnonce; /**< HTTP authentication client nonce (internal) */
	size_t nc; /**< HTTP authentication nonce count (internal) */
	char * auth; /**< HTTP authentication response (internal) */
	int discoveryCount; /**< SSDP response count */
	int (* discover)(struct tTr64RequestCtx *, const char *, int (*)(const char *, const size_t, void *), void *); /**< perform a simple service discovery */
	tIpAddress * address; /**< resolved host IP/port addresses */
	int (* resolve)(struct tTr64RequestCtx *); /**< host/port resolver */
	void (* printAddress)(const struct tTr64RequestCtx *, FILE *); /**< prints out the resolved addresses as string */
	tNetHandle * net; /**< internal network handles (e.g. sockets) */
	int (* request)(struct tTr64RequestCtx *); /**< HTTP request handler (also used for HTTPS if supported) */
	int (* reset)(struct tTr64RequestCtx *); /**< resets all network handles */
	char * content; /**< pointer to the HTTP payload in buffer */
	char * buffer; /**< for input and output */
	size_t capacity; /**< total capacity of buffer */
	size_t length; /**< currently used space of buffer */
	int verbose; /**< verbosity level */
} tTr64RequestCtx;


typedef struct {
	char * name; /**< argument name */
	char * var; /**< variable name */
	char * value; /**< argument value used for queries */
	char * type; /**< argument type */
	char * dir; /**< argument direction (in/out) */
} tTrArgument;


typedef struct {
	char * name; /**< action name */
	tTrArgument * arg; /**< argument array */
	size_t capacity; /**< total capacity of arg in number of elements */
	size_t length; /**< number of elements in arg */
} tTrAction;


typedef struct {
	char * name; /**< service name */
	char * type; /**< service type */
	char * path; /**< path to the service */
	char * control; /**< control URL */
	tTrAction * action; /**< actions array */
	size_t capacity; /**< total capacity of action in number of elements */
	size_t length; /**< number of elements in action */
} tTrService;


typedef struct {
	char * name; /**< name of the device */
	tTrService * service; /**< service array */
	size_t capacity; /**< total capacity of service in number of elements */
	size_t length; /**< number of elements in service */
} tTrDevice;


typedef struct {
	char * name; /**< root device name */
	char * url; /**< URL to the object */
	tTrDevice * device; /**< device array */
	size_t capacity; /**< total capacity of device in number of elements */
	size_t length; /**< number of elements in device */
} tTrObject;


typedef struct {
	tTrObject * object;
	tTrDevice * device;
	tTrService * service;
	tTrAction * action;
	tTrArgument * arg;
	tPCacheState state;
} tPTrObjectCacheCtx;


typedef struct {
	tPToken xmlPath[MAX_XML_DEPTH];
	tTrObject * object;
	tTrDevice * device;
	tTrService * service;
	tPToken content;
	tMessage lastError; /* only MSGT_ values without arguments are allowed */
} tPTrObjectDeviceCtx;


typedef struct {
	tPToken xmlPath[MAX_XML_DEPTH];
	tTrService * service;
	tTrAction * action;
	tTrArgument * arg;
	tPToken content;
	tPToken stateVarName;
	tMessage lastError; /* only MSGT_ values without arguments are allowed */
} tPTrObjectServiceCtx;


typedef struct {
	tPToken xmlPath[MAX_XML_DEPTH];
	tPToken soapNs;
	tPToken userNs;
	const tTrService * service;
	tTrAction * action;
	tPToken content;
	size_t depth;
	tMessage lastError; /* only MSGT_ values without arguments are allowed */
} tPTrQueryRespCtx;


typedef struct tTrQueryHandler {
	tTr64RequestCtx * ctx;
	tTrObject * obj;
	int (* query)(struct tTrQueryHandler *, const tOptions *, int);
	int (* output)(FILE *, struct tTrQueryHandler *, const tTrAction *);
	char * buffer; /**< for requests */
	size_t capacity; /**< total capacity of buffer */
	size_t length; /**< currently used space of buffer */
} tTrQueryHandler;


extern volatile int signalReceived;
extern FILE * fin;
extern FILE * fout;
extern FILE * ferr;
extern const void * fmsg[MSG_COUNT];
extern const tHttpStatusMsg httpStatMsg[44];


#ifdef UNICODE
int fuprintf(FILE * fd, const char * fmt, ...);
#else /* UNICODE */
#define fuprintf fprintf
#endif /* UNICODE */


/* helper functions */
void printHelp(void);
void handleSignal(int signum);
int arrayInit(void ** array, size_t * capacity, size_t * length, const size_t itemSize, const size_t size);
#define arrayFieldInit(obj, field, size) arrayInit((void **)(&((obj)->field)), &((obj)->capacity), &((obj)->length), sizeof(*((obj)->field)), size)
int arrayResize(void ** array, size_t * capacity, const size_t itemSize, const size_t size);
#define arrayFieldResize(obj, field, size) arrayResize((void **)(&((obj)->field)), &((obj)->capacity), sizeof(*((obj)->field)), size)
char * strndupInternal(const char * str, const size_t n);
int strnicmpInternal(const char * lhs, const char * rhs, const size_t n);
int cmpHttpStatusMsg(const tHttpStatusMsg * item, const size_t * value);
int fputUtf8(FILE * fd, const char * str);
int fputUtf8N(FILE * fd, const char * str, const size_t len);
int parseActionPath(tOptions * opt, int argIndex);
int urlVisitor(const tPUrlTokenType type, const tPToken * token, void * param);
int httpResponseVisitor(const tPHttpTokenType type, const tPToken * tokens, void * param);
int httpAuthentication(tTr64RequestCtx * ctx, const tTr64Response * resp);
int formatToBuffer(char ** buffer, size_t * capacity, size_t * length, const char * fmt, ...);
int formatToCtxBuffer(tTr64RequestCtx * ctx, const char * fmt, ...);
int formatToQryBuffer(tTrQueryHandler * ctx, const char * fmt, ...);
tTrObject * newTrObject(tTr64RequestCtx * ctx, const tOptions * opt);
int setArgValue(tTrArgument * arg, const char * str);
int setArgValueN(tTrArgument * arg, const char * str, const size_t len);
void freeTrObject(tTrObject * obj);
tTrQueryHandler * newTrQueryHandler(tTr64RequestCtx * ctx, tTrObject * obj, const tOptions * opt);
void freeTrQueryHandler(tTrQueryHandler * qry);


/* console command handlers */
int handleQuery(tOptions * opt);
int handleScan(tOptions * opt);
int handleList(tOptions * opt);
int handleInteractive(tOptions * opt);


/* I/O operations */
int isFile(const TCHAR * src);
char * readFileToString(const TCHAR * src, size_t * len);
int writeStringToFile(const TCHAR * dst, const char * str);
int writeStringNToFile(const TCHAR * dst, const char * str, const size_t len);
int initBackend(void);
void deinitBackend(void);
tTr64RequestCtx * newTr64Request(const char * url, const char * user, const char * pass, const tFormat format, const size_t timeout, const int verbose);
void freeTr64Request(tTr64RequestCtx * ctx);


#endif /* __TR64C_H__ */
