/**
 * @file tr64c.c
 * @author Daniel Starke
 * @date 2018-06-21
 * @version 2018-08-14
 * @todo Implement transaction session support.
 * @todo Apply output format to --list.
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
#include <stdarg.h>
#include <time.h>
#include "getopt.h"
#include "hmd5.h"
#include "mingw-unicode.h"


#if defined(PCF_IS_WIN)
#define PATH_SEPS _T("\\/")
#elif defined(PCF_IS_LINUX)
#define PATH_SEPS _T("/")
#else
#error "Unsupported target platform."
#endif

#if defined(BACKEND_WINSOCKS)
#include "tr64c-winsocks.c" /* includes tr64c.h */
#elif defined(BACKEND_POSIX)
#include "tr64c-posix.c" /* includes tr64c.h */
#else
#error "Unsupported backend."
#endif


volatile int signalReceived = 0;
FILE * fin = NULL;
FILE * fout = NULL;
FILE * ferr = NULL;
const void * fmsg[MSG_COUNT] = {
	/* MSGT_SUCCESS                    */ _T(""), /* never used for output */
	/* MSGT_ERR_NO_MEM                 */ _T("Error: Failed to allocate memory.\n"),
	/* MSGT_ERR_OPT_NO_ARG             */ _T("Error: Option argument is missing for '%s'.\n"),
	/* MSGT_ERR_OPT_BAD_FORMAT         */ _T("Error: Invalid format value. (%s)\n"),
	/* MSGT_ERR_OPT_BAD_TIMEOUT        */ _T("Error: Invalid timeout value. (%s)"),
	/* MSGT_ERR_OPT_NO_SERVICE         */ _T("Error: Missing service name.\n"),
	/* MSGT_ERR_OPT_NO_ACTION          */ _T("Error: Missing action name.\n"),
	/* MSGT_ERR_OPT_NO_ACTION_ARG      */ _T("Error: Missing action argument variable.\n"),
	/* MSGT_ERR_OPT_SSDP_BAD_PORT      */ _T("Error: Invalid port given for local discovery scan.\n"),
	/* MSGT_ERR_OPT_AMB_C              */ _T("Error: Unknown or ambiguous option '-%c'.\n"),
	/* MSGT_ERR_OPT_AMB_S              */ _T("Error: Unknown or ambiguous option '%s'.\n"),
	/* MSGT_ERR_OPT_AMB_X              */ _T("Error: Unknown option character '0x%02X'.\n"),
	/* MSGT_ERR_OPT_ACTION_AMB         */ _T("Error: Requested action is ambiguous. Please specify the device.\n"),
	/* MSGT_ERR_OPT_BAD_ACTION         */ _T("Error: Requested action is invalid.\n"),
	/* MSGU_ERR_OPT_NO_IN_ARG          */    "Error: Required input argument variable \"%s\" is missing.\n",
	/* MSGU_ERR_OPT_AMB_IN_ARG         */    "Error: Invalid multiple argument variable definition for \"%s\".\n",
	/* MSGT_ERR_OPT_NO_SSDP_ADDR       */ _T("Error: Missing local interface IP address to perform discovery on.\n"),
	/* MSGT_ERR_OPT_NO_ADDR            */ _T("Error: No address given.\n"),
	/* MSGT_ERR_FMT_DEV_DESC           */ _T("Error: Failed to format HTTP GET request for device description.\n"),
	/* MSGT_ERR_GET_DEV_DESC           */ _T("Error: Failed to retrieve device description from device (%u).\n"),
	/* MSGU_ERR_DEV_DESC_FMT           */    "Error: The received device description file format is invalid.\nPath: /%s\n",
	/* MSGT_ERR_FMT_SRVC_DESC          */ _T("Error: Failed to format HTTP GET request for service description.\n"),
	/* MSGT_ERR_GET_SRVC_DESC          */ _T("Error: Failed to retrieve service description from device (%u).\n"),
	/* MSGU_ERR_DEV_SRVC_FMT           */    "Error: The received service description file format is invalid.\nPath: %s\n",
	/* MSGT_ERR_NO_DEV_IN_DESC         */ _T("Error: No device found in device description.\n"),
	/* MSGU_ERR_NO_TYPE_FOR_ARG        */    "Error: No type for argument variable \"%s\" given in service description.\n",
	/* MSGT_ERR_FMT_SSDP               */ _T("Error: Failed to format SSDP request.\n"),
	/* MSGT_ERR_BACKEND_INIT           */ _T("Error: Failed to initialize backend API.\n"),
	/* MSGT_ERR_SOCK_NEW               */ _T("Error: Failed to create socket.\n"),
	/* MSGT_ERR_SOCK_NON_BLOCK         */ _T("Error: Failed to configure socket non-blocking.\n"),
	/* MSGT_ERR_SOCK_ON_REUSE          */ _T("Error: Failed to enable re-use address for the socket.\n"),
	/* MSGT_ERR_SOCK_OFF_MC_LB         */ _T("Error: Failed to disable loop-back for multicasts.\n"),
	/* MSGT_ERR_SOCK_OFF_FRAG          */ _T("Error: Failed to disable packet fragmentation.\n"),
	/* MSGT_ERR_SOCK_SET_RECV_TOUT     */ _T("Error: Failed to set receive timeout for the socket.\n"),
	/* MSGT_ERR_SOCK_SET_SEND_TOUT     */ _T("Error: Failed to set send timeout for the socket.\n"),
	/* MSGT_ERR_SOCK_SET_MC_TTL        */ _T("Error: Failed to set multicast TTL for the socket.\n"),
	/* MSGT_ERR_SOCK_SET_ALIVE         */ _T("Error: Failed to set keep-alive for the socket.\n"),
	/* MSGT_ERR_SOCK_OFF_NAGLE         */ _T("Error: Failed to disable the Nagle algorithm for the socket.\n"),
	/* MSGT_ERR_SOCK_BIND_SSDP         */ _T("Error: Failed to bind to the SSDP multicast port of the given local interface.\n"),
	/* MSGT_ERR_SOCK_JOIN_MC_GROUP     */ _T("Error: Failed to join the SSDP multicast group.\n"),
	/* MSGT_ERR_SOCK_SEND_SSDP_REQ     */ _T("Error: Failed to send SSDP discovery request.\n"),
	/* MSGT_ERR_SOCK_LEAVE_MC_GROUP    */ _T("Error: Failed to leave the SSDP multicast group.\n"),
	/* MSGT_ERR_SOCK_CONNECT           */ _T("Error: Failed to connect to the given host.\n"),
	/* MSGT_ERR_SOCK_SEND_TOUT         */ _T("Error: Request to server timed out.\n"),
	/* MSGT_ERR_SOCK_RECV_TOUT         */ _T("Error: Response from server timed out.\n"),
	/* MSGT_ERR_HTTP_SEND_REQ          */ _T("Error: Failed to send request to server.\n"),
	/* MSGT_ERR_HTTP_RECV_RESP         */ _T("Error: Failed to get response from server.\n"),
	/* MSGT_ERR_HTTP_STATUS            */ _T("Error: Received HTTP response with status code %u.\n"),
	/* MSGT_ERR_HTTP_FMT_AUTH          */ _T("Error: Failed to format HTTP authentication response.\n"),
	/* MSGT_ERR_HTTP_AUTH              */ _T("Error: Failed HTTP authentication.\n"),
	/* MSGT_ERR_URL_FMT                */ _T("Error: Failed to parse the given URL.\n"),
	/* MSGT_ERR_URL_PROT               */ _T("Error: Unsupported protocol in given URL.\n"),
	/* MSGT_ERR_FMT_QUERY              */ _T("Error: Failed to format HTTP request for query.\n"),
	/* MSGT_ERR_GET_QUERY_RESP         */ _T("Error: Failed to retrieve query response from server (%u).\n"),
	/* MSGT_ERR_QUERY_RESP_FMT         */ _T("Error: The retrieve query response file format is invalid.\n"),
	/* MSGT_ERR_QUERY_RESP_ACTION      */ _T("Error: Action name mismatch in query response.\n"),
	/* MSGT_ERR_QUERY_RESP_ARG         */ _T("Error: Invalid action argument variable in query response.\n"),
	/* MSGT_ERR_QUERY_RESP_ARG_BAD_ESC */ _T("Error: Invalid escape sequence in argument value of query response.\n"),
	/* MSGT_ERR_QUERY_PRINT            */ _T("Error: Failed to write formatted query response.\n"),
	/* MSGT_WARN_CACHE_READ            */ _T("Warning: Failed to read cache file content.\n"),
	/* MSGT_WARN_CACHE_FMT             */ _T("Warning: The cache file format is invalid.\n"),
	/* MSGT_WARN_CACHE_UNESC           */ _T("Warning: Failed to unescape field from cache file.\n"),
	/* MSGT_WARN_CACHE_NO_MEM          */ _T("Warning: Failed to allocate memory to output cache file.\n"),
	/* MSGT_WARN_CACHE_WRITE           */ _T("Warning: Failed to output cache file.\n"),
	/* MSGT_WARN_OPT_LOW_TIMEOUT       */ _T("Warning: Timeout value is less than recommended (>=1000ms).\n"),
	/* MSGT_WARN_LIST_NO_MEM           */ _T("Warning: Failed to allocate memory for list output.\n"),
	/* MSGT_WARN_CMD_BAD_ESC           */ _T("Warning: Invalid escape sequence in command-line at column %u.\n"),
	/* MSGT_WARN_BAD_CMD               */ _T("Warning: Invalid command was ignored.\n"),
	/* MSGT_INFO_SIGTERM               */ _T("Info: Received signal. Finishing current operation.\n"),
	/* MSGU_INFO_DEV_DESC_REQ          */    "Info: Requesting /%s from device.\n",
	/* MSGT_INFO_DEV_DESC_DUR          */ _T("Info: Finished device description request in %u ms.\n"),
	/* MSGU_INFO_SRVC_DESC_REQ         */    "Info: Requesting %s from device.\n",
	/* MSGT_INFO_SRVC_DESC_DUR         */ _T("Info: Finished service description request in %u ms.\n"),
	/* MSGT_INFO_SOCK_BOUND_SSDP       */ _T("Info: Bound to SSDP multicast address "),
	/* MSGU_INFO_SOCK_JOINED_MC_GROUP  */    "Info: Joined SSDP multicast group for address %s on interface %s (%s).\n",
	/* MSGT_INFO_SSDP_SENT             */ _T("Info: Sent %u bytes as multicast SSDP request.\n"),
	/* MSGT_INFO_SSDP_RECV             */ _T("Info: Received %u bytes SSDP response.\n"),
	/* MSGT_DBG_SOCK_RECV              */ _T("Debug: Received %u bytes from server.\n"),
	/* MSGT_DBG_BAD_TOKEN              */ _T("Debug: Unexpected token at line %u column %u.\n"),
	/* MSGU_DBG_SELECTED_QUERY         */  "Debug: Selected query action is %s::%s::%s.\n",
	/* MSGT_DBG_PARSE_QUERY_RESP       */ _T("Debug: Parsing query response.\n"),
	/* MSGT_DBG_OUT_QUERY_RESP         */ _T("Debug: Output query response.\n"),
	/* MSGT_DBG_ENTER_DISCOVER         */ _T("Debug: Enter discover().\n"),
	/* MSGT_DBG_ENTER_REQUEST          */ _T("Debug: Enter request().\n"),
	/* MSGT_DBG_ENTER_RESET            */ _T("Debug: Enter reset().\n"),
	/* MSGT_DBG_ENTER_PRINTADDRESS     */ _T("Debug: Enter printAddress().\n"),
	/* MSGT_DBG_ENTER_NEWTR64REQUEST   */ _T("Debug: Enter newTr64Request().\n"),
	/* MSGT_DBG_ENTER_FREETR64REQUEST  */ _T("Debug: Enter freeTr64Request().\n")
};


/**
 * Main entry point.
 */
int _tmain(int argc, TCHAR ** argv) {
	int res, ret = EXIT_FAILURE;
	tOptions opt = {0}; /* initialize all options with zero */
	TCHAR * strNum;
	static int (* handler[])(tOptions *) = {
		handleQuery,
		handleScan,
		handleList,
		handleInteractive
	};
	struct option longOptions[] = {
#ifdef UNICODE
		{_T("utf8"),        no_argument,       NULL,    GETOPT_UTF8},
#endif /* UNICODE */
		{_T("version"),     no_argument,       NULL, GETOPT_VERSION},
		{_T("cache"),       required_argument, NULL,        _T('c')},
		{_T("format"),      required_argument, NULL,        _T('f')},
		{_T("help"),        no_argument,       NULL,        _T('h')},
		{_T("interactive"), no_argument,       NULL,        _T('i')},
		{_T("list"),        no_argument,       NULL,        _T('l')},
		{_T("host"),        required_argument, NULL,        _T('o')},
		{_T("password"),    required_argument, NULL,        _T('p')},
		{_T("scan"),        no_argument,       NULL,        _T('s')},
		{_T("timeout"),     required_argument, NULL,        _T('t')},
		{_T("user"),        required_argument, NULL,        _T('u')},
		{_T("verbose"),     no_argument,       NULL,        _T('v')},
		{NULL, 0, NULL, 0}
	};

	/* ensure that the environment does not change the argument parser behavior */
	putenv("POSIXLY_CORRECT=");
	
	/* set the output file descriptors */
	fin  = stdin;
	fout = stdout;
	ferr = stderr;
	
#ifdef UNICODE
	/* http://msdn.microsoft.com/en-us/library/z0kc8e3z(v=vs.80).aspx */
	_setmode(_fileno(fout), _O_U16TEXT);
	_setmode(_fileno(ferr), _O_U16TEXT);
#endif /* UNICODE */

	if (argc < 2) {
		printHelp();
		return EXIT_FAILURE;
	}

	opt.verbose++;
	opt.timeout = DEFAULT_TIMEOUT;
	opt.format = F_CSV;
	while (1) {
		res = getopt_long(argc, argv, _T(":c:f:hil:o:p:st:u:v"), longOptions, NULL);

		if (res == -1) break;
		switch (res) {
#ifdef UNICODE
		case GETOPT_UTF8:
			_setmode(_fileno(fout), _O_U8TEXT);
			_setmode(_fileno(ferr), _O_U8TEXT);
			opt.narrow = 1;
			break;
#endif /* UNICODE */
		case GETOPT_VERSION:
			_putts(_T2(PROGRAM_VERSION_STR));
			goto onSuccess;
			break;
		case _T('c'):
			opt.cache = optarg;
			break;
		case _T('f'):
			for (TCHAR * ch = optarg; *ch != 0; ch++) *ch = _totupper(*ch);
			if (_tcscmp(optarg, _T("CSV")) == 0) {
				opt.format = F_CSV;
			} else if (_tcscmp(optarg, _T("JSON")) == 0) {
				opt.format = F_JSON;
			} else if (_tcscmp(optarg, _T("XML")) == 0) {
				opt.format = F_XML;
			} else {
				_ftprintf(ferr, MSGT(MSGT_ERR_OPT_BAD_FORMAT), optarg);
				goto onError;
			}
			break;
		case _T('h'):
			printHelp();
			goto onSuccess;
			break;
		case _T('i'):
			opt.mode = M_INTERACTIVE;
			break;
		case _T('l'):
			opt.mode = M_LIST;
			break;
		case _T('o'):
			opt.url = _ttoUtf8(optarg);
			if (opt.url == NULL) goto onOutOfMemory;
			break;
		case _T('p'):
			opt.pass = _ttoUtf8(optarg);
			if (opt.pass == NULL) goto onOutOfMemory;
			/* clear password in command-line */
			for (TCHAR * ptr = optarg; *ptr != 0; ptr++) *ptr = _T('*');
			break;
		case _T('s'):
			opt.mode = M_SCAN;
			break;
		case _T('t'):
			opt.timeout = (size_t)_tcstol(optarg, &strNum, 10);
			if (opt.timeout < TIMEOUT_RESOLUTION || strNum == NULL || *strNum != 0) {
				_ftprintf(ferr, MSGT(MSGT_ERR_OPT_BAD_TIMEOUT), optarg);
				goto onError;
			}
			break;
		case _T('u'):
			opt.user = _ttoUtf8(optarg);
			if (opt.user == NULL) goto onOutOfMemory;
			/* clear user in command-line */
			for (TCHAR * ptr = optarg; *ptr != 0; ptr++) *ptr = _T('*');
			break;
		case _T('v'):
			opt.verbose++;
			break;
		case _T(':'):
			_ftprintf(ferr, MSGT(MSGT_ERR_OPT_NO_ARG), argv[optind - 1]);
			goto onError;
			break;
		case _T('?'):
			if (_istprint(optopt) != 0) {
				_ftprintf(ferr, MSGT(MSGT_ERR_OPT_AMB_C), optopt);
			} else if (optopt == 0) {
				_ftprintf(ferr, MSGT(MSGT_ERR_OPT_AMB_S), argv[optind - 1]);
			} else {
				_ftprintf(ferr, MSGT(MSGT_ERR_OPT_AMB_X), (int)optopt);
			}
			goto onError;
			break;
		default:
			abort();
		}
	}
	
	if (optind >= argc && opt.mode == M_QUERY) {
		_ftprintf(ferr, MSGT(MSGT_ERR_OPT_NO_ACTION_ARG));
		goto onError;
	}
	
	if (optind < argc) {
		opt.args = (char **)calloc(1, sizeof(*opt.args) * (argc - optind));
		if (opt.args == NULL) goto onOutOfMemory;
		for (int i = optind, j = 0; i < argc; i++, j++) {
			opt.args[j] = _ttoUtf8(argv[i]);
			if (opt.args[j] == NULL) goto onOutOfMemory;
			opt.argCount++;
		}
		if (parseActionPath(&opt, 0) != 1) goto onOutOfMemory;
	}
	
	/* initialize backend API */
	if (initBackend() != 1) {
		if (opt.verbose > 0) _ftprintf(ferr, MSGT(MSGT_ERR_BACKEND_INIT));
		goto onError;
	}
	
	/* initialize random number generator */
	srand(time(NULL));
	
	/* install signal handlers */
	signalReceived = 0;
	signal(SIGINT, handleSignal);
	signal(SIGTERM, handleSignal);
	
	/* execute the requested operation */
	if (handler[opt.mode](&opt)	!= 1) goto onError;

onSuccess:
	ret = EXIT_SUCCESS;
onOutOfMemory:
	if (ret != EXIT_SUCCESS) {
		if (opt.verbose > 0) _ftprintf(ferr, MSGT(MSGT_ERR_NO_MEM));
	}
onError:
	/* cleanup */
	if (opt.url != NULL) free(opt.url);
	if (opt.user != NULL) free(opt.user);
	if (opt.pass != NULL) free(opt.pass);
	if (opt.device != NULL) free(opt.device);
	if (opt.service != NULL) free(opt.service);
	if (opt.action != NULL) free(opt.action);
	if (opt.args != NULL) {
		for (int i = 0; i < opt.argCount; i++) {
			if (opt.args[i] != NULL) free(opt.args[i]);
		}
		free(opt.args);
	}
	deinitBackend();
	return ret;
}


/**
 * Write the help for this application to standard out.
 */
void printHelp(void) {
	_tprintf(
	_T("tr64c [options] [[<device>/]<service/action> [<variable=value> ...]]\n")
	_T("\n")
	_T("-c, --cache <file>\n")
	_T("      Cache action descriptions of the device in this file.\n")
	_T("-f, --format <string>\n")
	_T("      Defines the output format for queries. Possible values are:\n")
	_T("      CSV  - comma-separated values (default)\n")
	_T("      JSON - JavaScript Object Notation\n")
	_T("      XML  - Extensible Markup Language\n")
	_T("-h, --help\n")
	_T("      Print short usage instruction.\n")
	_T("-i, --interactive\n")
	_T("      Run in interactive mode.\n")
	_T("-l, --list\n")
	_T("      List services and actions available on the device.\n")
	_T("-o, --host <URL>\n")
	_T("      Device address to connect to in the format http://<host>:<port>/<file>.\n")
	_T("      The protocol defaults to http if omitted.\n")
	_T("      The port defaults to 49000 if omitted.\n")
	_T("      For scan mode set this parameter to the local interface IP address on\n")
	_T("      which the local discovery shall be performed on.\n")
	_T("-p, --password <string>\n")
	_T("      Use this password to authenticate to the device.\n")
	_T("-s, --scan\n")
	_T("      Perform a local device discovery scan.\n")
	_T("-u, --user <string>\n")
	_T("      Use this user name to authenticate to the device.\n")
#ifdef UNICODE
	_T("    --utf8\n")
	_T("      Sets the encoding for console inputs/outputs to UTF-8.\n")
	_T("      The default is UTF-16.\n")
#endif /* UNICODE */
	_T("-t, --timeout <number>\n")
	_T("      Timeout for network operations in milliseconds.\n")
	_T("-v\n")
	_T("      Increases verbosity.\n")
	_T("    --version\n")
	_T("      Outputs the program version.\n")
	_T("\n")
	_T("tr64c ") _T2(PROGRAM_VERSION_STR) _T("\n")
	_T("https://github.com/daniel-starke/tr64c\n")
	);
}


/**
 * Handles external signals.
 * 
 * @param[in] signum - received signal number
 */
void handleSignal(int signum) {
	PCF_UNUSED(signum)
	_ftprintf(fout, MSGT(MSGT_INFO_SIGTERM));
	signalReceived++;
}


/**
 * Initializes the given array with the passed size.
 * 
 * @param[in] array - pointer to an array
 * @param[in] capacity - pointer to the capacity variable of the array
 * @param[in] length - pointer to the length variable of the array
 * @param[in] itemSize - array item size in bytes
 * @param[in] size - array size in number of items
 * @return 1 on success, else 0
 * @remarks The passed arguments are not checked.
 */
int arrayInit(void ** array, size_t * capacity, size_t * length, const size_t itemSize, const size_t size) {
	void * mem = malloc(itemSize * size);
	if (mem == NULL) return 0;
	*array = mem;
	*capacity = size;
	*length = 0;
	return 1;
}


/**
 * Resizes a given array.
 * 
 * @param[in] array - pointer to an array
 * @param[in] capacity - pointer to the capacity variable of the array
 * @param[in] itemSize - array item size in bytes
 * @param[in] size - new array size in number of items
 * @return 1 on success, else 0
 * @remarks The passed arguments are not checked.
 */
int arrayResize(void ** array, size_t * capacity, const size_t itemSize, const size_t size) {
	void * mem = realloc(*array, itemSize * size);
	if (mem == NULL) return 0;
	*array = mem;
	*capacity = size;
	return 1;
}


/**
 * Duplicates the given string by taking at most the number of characters given. The result is
 * guaranteed to be null-terminated.
 * 
 * @param[in] str - string to duplicate
 * @param[in] n - max number of characters to copy
 * @return Newly allocated string or NULL on error
 */
char * strndupInternal(const char * str, const size_t n) {
	if (str == NULL) return NULL;
	size_t len = 0;
	for (const char * ptr = str; len < n && *ptr != 0; len++, ptr++);
	char * res = (char *)malloc(len + 1);
	if (res == NULL) return NULL;
	if (len > 0) memcpy(res, str, len);
	res[len] = 0;
	return res;
}


/**
 * Compares the left-hand string with the right-hand string case-insensitive.
 * 
 * @param[in] lhs - left-hand string
 * @param[in] rhs - right-hand string
 * @param[in] n - maximum number of characters to compare
 * @return  0 if equal
 * @return <0 if lhs comes before rhs
 * @return >0 if rhs comes before lhs
 */
int strnicmpInternal(const char * lhs, const char * rhs, size_t n) {
	if (lhs == NULL || rhs == NULL) {
		errno = EFAULT;
		return INT_MAX;
	}
	const unsigned char * left = (const unsigned char *)lhs;
	const unsigned char * right = (const unsigned char *)rhs;
	for (;n > 0 && *right != 0 && toupper(*left) == toupper(*right); n--, left++, right++);
	if (n > 0 && *right == 0) {
		return toupper(*left);
	} else if (n == 0 && *right != 0) {
		return -toupper(*right);
	} else if (n == 0 && *right == 0) {
		return 0;
	}
	return toupper(*left) - toupper(*right);
}


/**
 * The function reads a line and sets the passed string pointer.
 * New space is allocated if the string can not hold more characters.
 *
 * @param[in,out] strPtr - points to the output string
 * @param[in,out] lenPtr - pointer to the size of the input string in characters
 * @param[in] fd - file descriptor to read from
 * @return Number of characters read from file descriptor or -1 on error.
 */
static int getLineNarrow(char ** strPtr, int * lenPtr, FILE * fd) {
	char * buf, * bufPos, * resStr;
	int totalLen, size, toRead, res;
	if (strPtr == NULL || lenPtr == NULL || fd == NULL) return -1;
	if (*strPtr == NULL || *lenPtr <= 0) {
		*strPtr = (char *)malloc(sizeof(char) * LINE_BUFFER_STEP);
		if (*strPtr == NULL) {
			return -1;
		}
		**strPtr = 0;
		*lenPtr = LINE_BUFFER_STEP;
	}
	buf = *strPtr;
	bufPos = buf;
	size = *lenPtr;
	toRead = *lenPtr;
	totalLen = 0;
	while ((resStr = fgets(bufPos, toRead, fd)) != NULL) {
		res = (int)strlen(bufPos);
		totalLen += res;
		if ((res + 1) == toRead && bufPos[res - 1] != '\n') {
			size += LINE_BUFFER_STEP;
			resStr = (char *)realloc(buf, sizeof(char) * (size_t)size);
			if (resStr == NULL) {
				if (buf != NULL) free(buf);
				*strPtr = NULL;
				*lenPtr = 0;
				return -1;
			}
			buf = resStr;
			toRead = LINE_BUFFER_STEP + 1;
			bufPos = buf + totalLen;
		} else {
			break;
		}
	}
	if (resStr == NULL) {
		if (feof(fd) != 0) {
			*buf = 0;
			totalLen = 0;
		} else {
			totalLen += (int)strlen(bufPos);
		}
	}
	*strPtr = buf;
	*lenPtr = size;
	return totalLen;
}


#ifdef UNICODE
/**
 * The function reads a line and sets the passed string pointer.
 * New space is allocated if the string can not hold more characters.
 *
 * @param[in,out] strPtr - points to the output string
 * @param[in,out] lenPtr - pointer to the size of the input string in characters
 * @param[in] fd - file descriptor to read from
 * @return Number of characters read from file descriptor or -1 on error.
 */
static int getLineWide(wchar_t ** strPtr, int * lenPtr, FILE * fd) {
	wchar_t * buf, * bufPos, * resStr;
	int totalLen, size, toRead, res;
	if (strPtr == NULL || lenPtr == NULL || fd == NULL) return -1;
	if (*strPtr == NULL || *lenPtr <= 0) {
		*strPtr = (wchar_t *)malloc(sizeof(wchar_t) * LINE_BUFFER_STEP);
		if (*strPtr == NULL) {
			return -1;
		}
		**strPtr = 0;
		*lenPtr = LINE_BUFFER_STEP;
	}
	buf = *strPtr;
	bufPos = buf;
	size = *lenPtr;
	toRead = *lenPtr;
	totalLen = 0;
	while ((resStr = fgetws(bufPos, toRead, fd)) != NULL) {
		res = (int)wcslen(bufPos);
		totalLen += res;
		if ((res + 1) == toRead && bufPos[res - 1] != L'\n') {
			size += LINE_BUFFER_STEP;
			resStr = (wchar_t *)realloc(buf, sizeof(wchar_t) * (size_t)size);
			if (resStr == NULL) {
				if (buf != NULL) free(buf);
				*strPtr = NULL;
				*lenPtr = 0;
				return -1;
			}
			buf = resStr;
			toRead = LINE_BUFFER_STEP + 1;
			bufPos = buf + totalLen;
		} else {
			break;
		}
	}
	if (resStr == NULL) {
		if (feof(fd) != 0) {
			*buf = 0;
			totalLen = 0;
		} else {
			totalLen += (int)wcslen(bufPos);
		}
	}
	*strPtr = buf;
	*lenPtr = size;
	return totalLen;
}
#endif /* UNICODE */


/**
 * The function reads a line and sets the passed string pointer.
 * New space is allocated if the string can not hold more characters.
 *
 * @param[in,out] line - read line buffer
 * @param[in] fd - file descriptor to read from
 * @param[in] narrow - set 1 to use narrow input mode
 * @return Number of characters read from file descriptor or -1 on error.
 */
static int getLineUtf8(tReadLineBuf * line, FILE * fd, int narrow) {
	if (line == NULL || fd == NULL) return -1;
#ifdef UNICODE
	int res;
	if (narrow == 0) {
		res = getLineWide(&(line->tmp), &(line->tmpLen), fd);
		if (res <= 0) return res;
		if (line->str != NULL) free(line->str);
		line->str = _ttoUtf8N(line->tmp, (size_t)res);
		if (line->str == NULL) return -1;
		line->strLen = (int)strlen(line->str) + 1;
		return line->strLen;
	}
#else /* UNICODE */
	PCF_UNUSED(narrow);
#endif /* UNICODE */
	return getLineNarrow(&(line->str), &(line->strLen), fd);
}


/**
 * The function frees the allocated read line buffers within the given structure.
 * 
 * @param[in] line - free this read line buffer
 * @remarks The pointed structure itself is not freed.
 */
static void freeGetLine(tReadLineBuf * line) {
	if (line == NULL) return;
	if (line->str != NULL) free(line->str);
#ifdef UNICODE
	if (line->tmp != NULL) free(line->tmp);
#endif /* UNICODE */
}


/**
 * Prints out the given UTF-8 string. This transforms the string into the native Unicode format.
 * 
 * @param[in] fd - file descriptor to print to
 * @param[in] str - string to print
 * @return same as _ftprintf()
 * @remarks Console output may be cut off if output string is more that 4 KiB as TCHAR string.
 */
int fputUtf8(FILE * fd, const char * str) {
	if (fd == NULL || str == NULL) return -1;
#ifdef UNICODE
	int result;
	TCHAR * out = _tfromUtf8(str);
	if (out == NULL) return -1;
	/* we assume that out is at most 4 KiB */
	result = _ftprintf(fd, _T("%s"), out);
	free(out);
	return result;
#else
	return fputs(str, fd);
#endif
}


/**
 * Prints out the given UTF-8 string. This transforms the string into the native Unicode format.
 * 
 * @param[in] fd - file descriptor to print to
 * @param[in] str - string to print
 * @param[in] len - string length in number of bytes
 * @return same as _ftprintf()
 */
int fputUtf8N(FILE * fd, const char * str, const size_t len) {
	if (fd == NULL || str == NULL) return -1;
#ifdef UNICODE
	int result = 0, ok;
	TCHAR * out = _tfromUtf8N(str, len);
	if (out == NULL) return -1;
	/* workaround buffer size limitation of the console in Windows */
	const size_t chunk = 4096 / sizeof(*out);
	for (size_t i = 0; i < len; i += chunk) {
		const size_t l = ((i + chunk) > len) ? len - i : chunk;
		ok = _ftprintf(fd, _T("%.*s"), (unsigned)l, out + i);
		if (ok < 0) {
			free(out);
			return ok;
		}
		result += ok;
	}
	free(out);
	return result;
#else
	return _ftprintf(fd, _T("%.*s"), (int)len, str);
#endif
}


/**
 * Parses the [device/]service/action path of the option arguments. Existing values are replaced.
 * 
 * @param[in,out] opt - parse arguments and set fields
 * @param[in] argIndex - use argument at this index
 * @return 1 on success, else 0
 */
int parseActionPath(tOptions * opt, int argIndex) {
	if (opt == NULL || argIndex < 0 || argIndex >= opt->argCount) return 0;
	const size_t sepLen = 1;
	char * sep1 = strchr(opt->args[argIndex], '/');
	
	if (opt->device != NULL) {
		free(opt->device);
		opt->device = NULL;
	}
	if (opt->service != NULL) {
		free(opt->service);
		opt->service = NULL;
	}
	if (opt->action != NULL) {
		free(opt->action);
		opt->action = NULL;
	}
	
	if (sep1 != NULL) {
		char * sep2 = strchr(sep1 + 1, '/');
		if (sep2 != NULL) {
			opt->device = strndupInternal(opt->args[argIndex], (size_t)(sep1 - opt->args[argIndex]));
			if (opt->device == NULL) goto onError;
			opt->service = strndupInternal(sep1 + sepLen, (size_t)(sep2 - sep1 - sepLen));
			if (opt->service == NULL) goto onError;
			opt->action = strdup(sep2 + sepLen);
			if (opt->action == NULL) goto onError;
		} else {
			opt->service = strndupInternal(opt->args[argIndex], (size_t)(sep1 - opt->args[argIndex]));
			if (opt->service == NULL) goto onError;
			opt->action = strdup(sep1 + sepLen);
			if (opt->action == NULL) goto onError;
		}
	}
	
	return 1;
onError:
	return 0;
}


/**
 * Helper callback function to parse a URL into tokens.
 * 
 * @param[in] type - token type
 * @param[in] tokens - tokens passed
 * @param[in,out] param - user defined callback data
 * @return 1 to continue
 * @remarks param shall point to a valid tTr64RequestCtx variable.
 */
int urlVisitor(const tPUrlTokenType type, const tPToken * token, void * param) {
	tTr64RequestCtx * ctx = (tTr64RequestCtx *)param;
	tPToken thisToken = *token;
	char ** field = NULL;
	switch (type) {
	case PUTT_PROTOCOL: field = &(ctx->protocol); break;
	case PUTT_USER: field = &(ctx->user); break;
	case PUTT_PASS: field = &(ctx->pass); break;
	case PUTT_HOST: field = &(ctx->host); break;
	case PUTT_PORT: field = &(ctx->port); break;
	case PUTT_PATH: field = &(ctx->path); break;
	case PUTT_SEARCH:
	case PUTT_HASH:
		if (ctx->path == NULL) {
			thisToken.length = strlen(thisToken.start);
			field = &(ctx->path);
		}
		break;
	}
	if (field != NULL) {
		*field = p_copyToken(&thisToken);
		if (*field == NULL) {
			if (ctx->verbose > 0) _ftprintf(ferr, MSGT(MSGT_ERR_NO_MEM));
			return 0;
		}
	}
	return 1;
}


/**
 * Helper callback function to parse a HTTP message and return the expected body.
 * 
 * @param[in] type - token type
 * @param[in] tokens - tokens passed (1 for body; 2 parameter)
 * @param[in,out] param - user defined callback data (expects tTr64Response)
 * @return 1 to continue
 * @remarks param shall point to a zeroed tPToken variable.
 */
int httpResponseVisitor(const tPHttpTokenType type, const tPToken * tokens, void * param) {
	tTr64Response * resp = (tTr64Response *)param;
	if (resp == NULL) return 0;
	if (type == PHTT_EXPECTED) {
		resp->content = *tokens;
	} else if (type == PHTT_BODY && resp->content.start != NULL) {
		resp->content = *tokens;
	} else if (type == PHTT_STATUS) {
		resp->status = (size_t)strtoul(tokens[1].start, NULL, 10);
	} else if (type == PHTT_PARAMETER && p_cmpTokenI(tokens, "WWW-Authenticate") == 0) {
		/* parse authentication parameters */
		typedef enum {
			HAS_START,
			HAS_FIELD,
			HAS_SEP,
			HAS_VALUE,
			HAS_END
		} tHttpAuthState;
		tPToken * field = NULL;
		tHttpAuthFlag flag = HAF_NONE;
		tPToken token, qop, algorithm;
		const char * ptr;
		const char * ch;
		char quote = 0;
		const size_t length = tokens[1].length;
		tHttpAuthState state = HAS_START;
		size_t n, k;
		for (n = 0, ptr = tokens[1].start; n < length;) {
#ifdef AUTH_DEBUG
#define FLAG_STR(f, x) if ((f & HAF_##x) != 0) { if (first != 1) fputs("|", stderr); first = 0; fputs(#x, stderr); }
#define FLAGS_STR(f) \
			first = 1; \
			FLAG_STR(f, CRED)     FLAG_STR(f, DIGEST) \
			FLAG_STR(f, REALM)    FLAG_STR(f, NONCE) \
			FLAG_STR(f, QOP)      FLAG_STR(f, AUTH) \
			FLAG_STR(f, AUTH_INT) FLAG_STR(f, ALGORITHM) \
			FLAG_STR(f, MD5)      FLAG_STR(f, MD5_SESS) \
			FLAG_STR(f, OPAQUE) \
			if (first == 1) fputs("NONE", stderr);
			const char * stateStr[] = {"START", "FIELD", "SEP", "VALUE", "END"};
			int first;
			fprintf(stderr, "char =");
			if (isprint(*ptr) != 0) {
				fprintf(stderr, " '%c'", *ptr);
			} else {
				fprintf(stderr, "    ");
			}
			fprintf(stderr, " 0x%02X, n = %3u, state = %-5s, flags = ", *ptr, (unsigned)n, stateStr[state]);
			FLAGS_STR(resp->auth.flags)
			fputs("\n", stderr);
			if (*ptr != tokens[1].start[n]) {
				fprintf(stderr, "Error: n position does not match ptr\n");
				abort();
			}
#endif /* AUTH_DEBUG */
			switch (state) {
			case HAS_START:
				if ((n + 1) >= length) {
					state = HAS_END;
				} else if (isblank(*ptr) == 0 && *ptr != ',') {
					token.start = ptr;
					state = HAS_FIELD;
				}
				break;
			case HAS_FIELD:
				if (isblank(*ptr) != 0 || *ptr == '=' || (n + 1) >= length || ((resp->auth.flags & HAF_CRED) != 0 && *ptr == ',')) {
					token.length = ptr - token.start;
					if ((n + 1) >= length) token.length++;
					quote = 0;
					if ((resp->auth.flags & HAF_CRED) == 0) {
						resp->auth.flags = (tHttpAuthFlag)(resp->auth.flags | HAF_CRED);
						if (p_cmpTokenI(&token, "Digest") == 0) {
							resp->auth.flags = (tHttpAuthFlag)(resp->auth.flags | HAF_DIGEST);
							flag = HAF_NONE;
							field = NULL;
							state = HAS_START;
						} else {
							return 1; /* expected method */
						}
					} else if (p_cmpTokenI(&token, "realm") == 0) {
						field = &(resp->auth.realm);
						flag = HAF_REALM;
						state = HAS_SEP;
					} else if (p_cmpTokenI(&token, "nonce") == 0) {
						field = &(resp->auth.nonce);
						flag = HAF_NONCE;
						state = HAS_SEP;
					} else if (p_cmpTokenI(&token, "qop") == 0) {
						field = &qop;
						flag = HAF_QOP;
						state = HAS_SEP;
					} else if (p_cmpTokenI(&token, "algorithm") == 0) {
						field = &algorithm;
						flag = HAF_ALGORITHM;
						state = HAS_SEP;
					} else if (p_cmpTokenI(&token, "opaque") == 0) {
						field = &(resp->auth.opaque);
						flag = HAF_OPAQUE;
						state = HAS_SEP;
					} else {
						field = NULL;
						flag = HAF_NONE;
						state = HAS_SEP;
					}
					token.start = NULL;
					if (state == HAS_SEP) continue; /* re-evaluate */
				}
				break;
			case HAS_SEP:
				if (*ptr == ',' || (n + 1) >= length) {
					field = NULL;
					flag = HAF_NONE;
					state = HAS_START;
					continue;
				} else if (*ptr == '=') {
					state = HAS_VALUE;
				} else if (isblank(*ptr) == 0) {
					return 1; /* unexpected token */
				}
				break;
			case HAS_VALUE:
				if (isblank(*ptr) == 0) {
					if (token.start == NULL) {
						/* find first valid character */
						if (*ptr == '"') {
							quote = *ptr;
							token.start = ptr + 1;
						} else {
							token.start = ptr;
						}
					} else if ((quote != 0 && *ptr == quote) || (quote == 0 && (*ptr == ',' || (n + 1) >= length))) {
						/* end of value */
						token.length = ptr - token.start;
						if (quote == 0 && (n + 1) >= length) token.length++;
						if (flag != HAF_NONE && field != NULL) {
							*field = token;
							resp->auth.flags = (tHttpAuthFlag)(resp->auth.flags | flag);
							switch (flag) {
							case HAF_QOP:
								/* parse qop value */
								token.start = NULL;
								for (k = 0, ch = qop.start; k < qop.length; k++, ch++) {
#ifdef AUTH_DEBUG
									fprintf(stderr, "QOP: char =");
									if (isprint(*ch) != 0) {
										fprintf(stderr, " '%c'", *ch);
									} else {
										fprintf(stderr, "    ");
									}
									fprintf(stderr, " 0x%02X, k = %3u, flags = ", *ch, (unsigned)k);
									FLAGS_STR(resp->auth.flags)
									fputs("\n", stderr);
									if (*ch != qop.start[k]) {
										fprintf(stderr, "Error: k position does not match ch\n");
										abort();
									}
#endif /* AUTH_DEBUG */
									if (isblank(*ch) == 0 && *ch != ',') {
										if (token.start == NULL) token.start = ch;
										if ((k + 1) >= qop.length) {
											token.length = ch - token.start + 1;
											goto onQopParam;
										}
									} else if (token.start != NULL) {
										token.length = ch - token.start;
onQopParam:
										if (p_cmpTokenI(&token, "auth") == 0) {
											resp->auth.flags = (tHttpAuthFlag)(resp->auth.flags | HAF_AUTH);
										} else if (p_cmpTokenI(&token, "auth-int") == 0) {
											resp->auth.flags = (tHttpAuthFlag)(resp->auth.flags | HAF_AUTH_INT);
										}
										token.start = NULL;
									}
								}
								break;
							case HAF_ALGORITHM:
								/* parse algorithm value */
								token.start = NULL;
								for (k = 0, ch = algorithm.start; k < algorithm.length; k++, ch++) {
#ifdef AUTH_DEBUG
									fprintf(stderr, "ALGORITHM: char =");
									if (isprint(*ch) != 0) {
										fprintf(stderr, " '%c'", *ch);
									} else {
										fprintf(stderr, "    ");
									}
									fprintf(stderr, " 0x%02X, k = %3u, flags = ", *ch, (unsigned)k);
									FLAGS_STR(resp->auth.flags)
									fputs("\n", stderr);
									if (*ch != algorithm.start[k]) {
										fprintf(stderr, "Error: k position does not match ch\n");
										abort();
									}
#endif /* AUTH_DEBUG */
									if (isblank(*ch) == 0 && *ch != ',') {
										if (token.start == NULL) token.start = ch;
										if ((k + 1) >= algorithm.length) {
											token.length = ch - token.start + 1;
											goto onAlgorithmParam;
										}
									} else if (token.start != NULL) {
										token.length = ch - token.start;
onAlgorithmParam:
										if (p_cmpTokenI(&token, "MD5") == 0) {
											resp->auth.flags = (tHttpAuthFlag)(resp->auth.flags | HAF_MD5);
										} else if (p_cmpTokenI(&token, "MD5-sess") == 0) {
											resp->auth.flags = (tHttpAuthFlag)(resp->auth.flags | HAF_MD5_SESS);
										}
										token.start = NULL;
									}
								}
								break;
							default:
								break;
							}
						}
						field = NULL;
						state = HAS_START;
					}
				}
				break;
			case HAS_END:
				break;
			}
			n++;
			ptr++;
		}
		if (state != HAS_START && state != HAS_END) return 1; /* unexpected end */
	}
	return 1;
}


/**
 * Converts the given MD5 to a hex string.
 * 
 * @param[out] str - pointer to 33 byte output string
 * @param[in] md5 - pointer to 16 byte input MD5
 */
void md5ToHex(char * str, const uint8_t * md5) {
	/* needs to be lower-case for HTTP digest authentication */
	static const char hex[] = "0123456789abcdef";
	for (size_t i = 0; i < 16; i++, md5++) {
		*str++ = hex[(*md5 >> 4) & 0x0F];
		*str++ = hex[*md5 & 0x0F];
	}
	*str = 0;
}


/**
 * Helper function to build a HTTP digest authentication request from the given server response.
 * 
 * @param[in,out] ctx - context to use
 * @param[in] resp - previous HTTP request resp
 * @return 1 on success, else 0
 * @remarks No support for auth-int and MD5-sess.
 * @see https://tools.ietf.org/html/rfc2617
 */
int httpAuthentication(tTr64RequestCtx * ctx, const tTr64Response * resp) {
	if (ctx == NULL || resp == NULL || ctx->method == NULL || ctx->path == NULL || ctx->user == NULL || ctx->pass == NULL) return 0;
	static const char * authRfc2617 = "Authorization: Digest username=\"%s\",realm=\"%s\",nonce=\"%s\",uri=\"%s\",qop=\"auth\",nc=%s,cnonce=\"%s\",response=\"%s\"\r\n";
	static const char * authRfc2617Opaque = "Authorization: Digest username=\"%s\",realm=\"%s\",nonce=\"%s\",uri=\"%s\",qop=\"auth\",nc=%s,cnonce=\"%s\",response=\"%s\",opaque=\"%s\"\r\n";
	static const char * authRfc2069 = "Authorization: Digest username=\"%s\",realm=\"%s\",nonce=\"%s\",uri=\"%s\",qop=\"\",response=\"%s\"\r\n";
	static const char auth[] = "auth"; /* only "" and "auth" is supported (i.e. no auth-int) */
	tHMd5Ctx a1, a2, k;
	uint8_t a1Data[16], a2Data[16], kData[16];
	char a1Str[33], a2Str[33], kStr[33];
	char nc[9];
	char cnonce[9];
	char sep[1] = {':'};
	const char * realm = NULL;
	const char * nonce = NULL;
	const char * opaque = NULL;
	int ok, res = 0;
	if ((resp->auth.flags & HAF_NEED) != HAF_NEED) return 0; /* missing fields */
	/* calculate digest */
	h_initMd5(&a1);
	h_updateMd5(&a1, (const uint8_t *)(ctx->user), strlen(ctx->user));
	h_updateMd5(&a1, (const uint8_t *)(sep), 1);
	h_updateMd5(&a1, (const uint8_t *)(resp->auth.realm.start), resp->auth.realm.length);
	h_updateMd5(&a1, (const uint8_t *)(sep), 1);
	h_updateMd5(&a1, (const uint8_t *)(ctx->pass), strlen(ctx->pass));
	h_finalMd5(&a1, a1Data);
	md5ToHex(a1Str, a1Data);
	h_initMd5(&a2);
	h_updateMd5(&a2, (const uint8_t *)(ctx->method), strlen(ctx->method));
	h_updateMd5(&a2, (const uint8_t *)(sep), 1);
	h_updateMd5(&a2, (const uint8_t *)(ctx->path), strlen(ctx->path));
	h_finalMd5(&a2, a2Data);
	md5ToHex(a2Str, a2Data);
	/* copy parameters for output as our input will be overwritten on output */
	realm = strndupInternal(resp->auth.realm.start, resp->auth.realm.length);
	if (realm == NULL) {
		if (ctx->verbose > 1) _ftprintf(ferr, MSGT(MSGT_ERR_NO_MEM));
		goto onError;
	}
	nonce = strndupInternal(resp->auth.nonce.start, resp->auth.nonce.length);
	if (nonce == NULL) {
		if (ctx->verbose > 1) _ftprintf(ferr, MSGT(MSGT_ERR_NO_MEM));
		goto onError;
	}
	if ((resp->auth.flags & HAF_RFC2617) == HAF_RFC2617) {
		/* use RFC 2617 auth */
		if (ctx->cnonce == 0) ctx->cnonce = (size_t)((rand() << 16) ^ rand());
		ctx->nc++;
		snprintf(cnonce, sizeof(cnonce), "%08X", (unsigned)(ctx->cnonce & 0xFFFFFFFF));
		snprintf(nc, sizeof(nc), "%08u", (unsigned)(ctx->nc % 100000000));
		h_initMd5(&k);
		h_updateMd5(&k, (const uint8_t *)(a1Str), 32);
		h_updateMd5(&k, (const uint8_t *)(sep), 1);
		h_updateMd5(&k, (const uint8_t *)(resp->auth.nonce.start), resp->auth.nonce.length);
		h_updateMd5(&k, (const uint8_t *)(sep), 1);
		if ((resp->auth.flags & HAF_AUTH) != 0) {
			h_updateMd5(&k, (const uint8_t *)(nc), sizeof(nc) - 1);
			h_updateMd5(&k, (const uint8_t *)(sep), 1);
			h_updateMd5(&k, (const uint8_t *)(cnonce), sizeof(cnonce) - 1);
			h_updateMd5(&k, (const uint8_t *)(sep), 1);
			h_updateMd5(&k, (const uint8_t *)(auth), sizeof(auth) - 1);
			h_updateMd5(&k, (const uint8_t *)(sep), 1);
		}
		h_updateMd5(&k, (const uint8_t *)(a2Str), 32);
		h_finalMd5(&k, kData);
		md5ToHex(kStr, kData);
		/* build response string */
		if ((resp->auth.flags & HAF_OPAQUE) != 0) {
			/* copy parameter for output as our input will be overwritten on output */
			opaque = strndupInternal(resp->auth.opaque.start, resp->auth.opaque.length);
			if (opaque == NULL) {
				if (ctx->verbose > 1) _ftprintf(ferr, MSGT(MSGT_ERR_NO_MEM));
				goto onError;
			}
			ctx->length = 0;
			ok = formatToCtxBuffer(ctx, authRfc2617Opaque, ctx->user, realm, nonce, ctx->path, nc, cnonce, kStr, opaque);
		} else {
			ctx->length = 0;
			ok = formatToCtxBuffer(ctx, authRfc2617, ctx->user, realm, nonce, ctx->path, nc, cnonce, kStr);
		}
	} else {
		/* fall back to RFC 2069 */
		h_initMd5(&k);
		h_updateMd5(&k, (const uint8_t *)(a1Str), 32);
		h_updateMd5(&k, (const uint8_t *)(sep), 1);
		h_updateMd5(&k, (const uint8_t *)(resp->auth.nonce.start), resp->auth.nonce.length);
		h_updateMd5(&k, (const uint8_t *)(sep), 1);
		h_updateMd5(&k, (const uint8_t *)(a2Str), 32);
		h_finalMd5(&k, kData);
		md5ToHex(kStr, kData);
		/* build response string */
		ctx->length = 0;
		ok = formatToCtxBuffer(ctx, authRfc2069, ctx->user, realm, nonce, ctx->path, kStr);
	}
	if (ok != 1) {
		if (ctx->verbose > 1)  _ftprintf(ferr, MSGT(MSGT_ERR_HTTP_FMT_AUTH));
		goto onError;
	}
	if (ctx->auth != NULL) free(ctx->auth);
	ctx->auth = strndupInternal(ctx->buffer, ctx->length);
	if (ctx->auth == NULL) {
		if (ctx->verbose > 1) _ftprintf(ferr, MSGT(MSGT_ERR_NO_MEM));
		goto onError;
	}
	res = 1;
onError:
	if (realm != NULL) free((void *)realm);
	if (nonce != NULL) free((void *)nonce);
	if (opaque != NULL) free((void *)opaque);
	return res;
}


/**
 * Add formatted string to the given buffer. The function automatically increases the
 * buffer if insufficient. The function may fail if no memory could be allocated.
 * The length parameter is used to append a new string.
 * 
 * @param[in,out] buffer - add to the pointed buffer
 * @param[in,out] capacity - capacity of the buffer
 * @param[in,out] length - length of the buffer
 * @param[in] fmt - format string
 * @param[in] ap - variable list of arguments for fmt
 * @return 1 on success, else 0
 */
static int formatToBufferVar(char ** buffer, size_t * capacity, size_t * length, const char * fmt, va_list ap) {
	if (buffer == NULL || *buffer == NULL || capacity == NULL || length == NULL || fmt == NULL) return 0;
	/* try to fit into the remaining buffer */
	const size_t remLen = (size_t)(*capacity - *length);
	va_list ap2;
	va_copy(ap2, ap);
	int resLen = vsnprintf(*buffer + *length, remLen, fmt, ap2);
	va_end(ap2);
	if (resLen < 0) resLen = (int)(remLen + 1);
	if ((size_t)(resLen + 1) >= remLen) {
		/* buffer was insufficient -> increase and retry */
		const size_t minCapacity = *capacity + (resLen * 2);
		size_t newCapacity = PCF_MAX(BUFFER_SIZE, *capacity * 2);
		for (; newCapacity < minCapacity; newCapacity *= 2);
		if (arrayResize((void **)buffer, capacity, sizeof(**buffer), newCapacity) != 1) return 0;
		*capacity = newCapacity;
		resLen = vsnprintf(*buffer + *length, newCapacity, fmt, ap);
		if (resLen < 0 || (size_t)(resLen + 1) >= newCapacity) return 0;
	}
	*length += (size_t)resLen;
	return 1;
}


/**
 * Add formatted string to the given buffer. The function automatically increases the
 * buffer if insufficient. The function may fail if no memory could be allocated.
 * The length parameter is used to append a new string.
 * 
 * @param[in,out] buffer - add to the pointed buffer
 * @param[in,out] capacity - capacity of the buffer
 * @param[in,out] length - length of the buffer
 * @param[in] fmt - format string
 * @param[in] ... - variable list of arguments for fmt
 * @return 1 on success, else 0
 */
int formatToBuffer(char ** buffer, size_t * capacity, size_t * length, const char * fmt, ...) {
	va_list ap;
	int result;
	va_start(ap, fmt);
	result = formatToBufferVar(buffer, capacity, length, fmt, ap);
	va_end(ap);
	return result;
}


/**
 * Add formatted string to the given context buffer. The function automatically increases the
 * buffer if insufficient. The function may fail if no memory could be allocated.
 * The length parameter is used to append a new string.
 * 
 * @param[in] ctx - add to buffer in this context
 * @param[in] fmt - format string
 * @param[in] ... - variable list of arguments for fmt
 * @return 1 on success, else 0
 */
int formatToCtxBuffer(tTr64RequestCtx * ctx, const char * fmt, ...) {
	va_list ap;
	int result;
	va_start(ap, fmt);
	result = formatToBufferVar(&(ctx->buffer), &(ctx->capacity), &(ctx->length), fmt, ap);
	va_end(ap);
	return result;
}


/**
 * Add formatted string to the given query handler buffer. The function automatically increases the
 * buffer if insufficient. The function may fail if no memory could be allocated.
 * The length parameter is used to append a new string.
 * 
 * @param[in] qry - add to buffer in this query
 * @param[in] fmt - format string
 * @param[in] ... - variable list of arguments for fmt
 * @return 1 on success, else 0
 */
int formatToQryBuffer(tTrQueryHandler * qry, const char * fmt, ...) {
	va_list ap;
	int result;
	va_start(ap, fmt);
	result = formatToBufferVar(&(qry->buffer), &(qry->capacity), &(qry->length), fmt, ap);
	va_end(ap);
	return result;
}


#ifdef UNICODE
static struct {
	char * buffer;
	size_t capacity;
	size_t length;
} fuprintfBuf = {
	/* .buffer   = */ NULL,
	/* .capacity = */ 0,
	/* .length   = */ 0
};
void fuprintfAtexit(void) {
	if (fuprintfBuf.buffer != NULL) {
		free(fuprintfBuf.buffer);
		fuprintfBuf.buffer = NULL;
		fuprintfBuf.capacity = 0;
		fuprintfBuf.length = 0;
	}
}

/**
 * Helper function for Unicode targets. Works like fprintf but converts the output to wchar_t using
 * fwprintf underneath. This function is not re-entrant safe.
 * 
 * @param[in] fd - file descriptor to write the output to
 * @param[in] fmt - format string
 * @param[in] ... - format string arguments
 * @return same as fwprintf
 * @remarks This function leaks the internally allocated buffer on exit.
 */
int fuprintf(FILE * fd, const char * fmt, ...) {
	if (fuprintfBuf.buffer == NULL) {		
		arrayFieldInit(&fuprintfBuf, buffer, 4096);
		atexit(fuprintfAtexit);
	}
	va_list ap;
	int result;
	va_start(ap, fmt);
	fuprintfBuf.length = 0;
	result = formatToBufferVar(&(fuprintfBuf.buffer), &(fuprintfBuf.capacity), &(fuprintfBuf.length), fmt, ap);
	va_end(ap);
	if (result <= 0) return result;
	result = fputUtf8N(fd, fuprintfBuf.buffer, fuprintfBuf.length);
	return result;
}
#endif /* UNICODE */


/**
 * Conditionally sets the given output token to the full qualified name of the input tokens.
 * 
 * @param[in] type - token type
 * @param[out] out - set full name to this token
 * @param[in] tokens - tokens passed (1 for contents and cdata; 2 for xml, instructions and tags, 3 for attributes)
 * @return 1 on success, else 0
 */
static int xmlToFullName(const tPSaxTokenType type, tPToken * out, const tPToken * tokens) {
	if (out == NULL || tokens == NULL) return 0;
	switch (type) {
	case PSTT_START_TAG:
	case PSTT_END_TAG:
	case PSTT_ATTRIBUTE:
		return p_xmlGetFullName(out, tokens);
		break;
	default:
		break;
	}
	return 1;
}


/**
 * Callback to parse the XML cache file format.
 * 
 * @param[in] type - token type
 * @param[in] tokens - tokens passed (1 for contents and cdata; 2 for xml, instructions and tags, 3 for attributes)
 * @param[in] level - token level (i.e. number of parents)
 * @param[in,out] param - user defined callback data (expects tPTrObjectCacheCtx)
 * @return 1 to continue, 0 on error
 * @see newTrObject()
 */
static int xmlCacheFileVisitor(const tPSaxTokenType type, const tPToken * tokens, const size_t level, void * param) {
	PCF_UNUSED(level);
	tPTrObjectCacheCtx * ctx = (tPTrObjectCacheCtx *)param;
	tPToken fullName;
	if (tokens == NULL || ctx == NULL) return 0;
	if (xmlToFullName(type, &fullName, tokens) != 1) return 0;
#define ENTER_NODE(item, root, newState) \
	if (p_cmpToken(&fullName, #item) != 0) return 0; /* invalid tag */ \
	if (ctx->root->length >= ctx->root->capacity) { \
		if (arrayFieldResize(ctx->root, item, PCF_MAX(INIT_ARRAY_SIZE, ctx->root->capacity * 2)) != 1) return 0; \
	} \
	ctx->item = ctx->root->item + ctx->root->length; \
	memset(ctx->item, 0, sizeof(*(ctx->item))); \
	ctx->root->length++; \
	ctx->state = PCS_WITHIN_##newState;
#define ADD_FIELD(item, field) \
	if (p_cmpToken(&fullName, #field) == 0) { \
		ctx->item->field = strndupInternal(tokens[2].start, tokens[2].length); \
		if (ctx->item->field == NULL) return 0; \
	}
#define LEAVE_NODE(item, newState) \
	if (p_cmpToken(&fullName, #item) != 0) return 0; /* invalid tag */ \
	ctx->state = PCS_WITHIN_##newState;
#define CHECK_FIELD(item, field) \
	if (ctx->item->field == NULL) return 0;
	
	switch (ctx->state) {
	case PCS_START:
		switch (type) {
		case PSTT_PARSE_XML:
		case PSTT_XML:
			break; /* ignore (valid at this point) */
		case PSTT_START_TAG:
			if (p_cmpToken(&fullName, "object") != 0) return 0; /* invalid tag */
			ctx->state = PCS_WITHIN_OBJECT;
			break;
		default:
			return 0; /* invalid token */
		}
		break;
	case PCS_WITHIN_OBJECT:
		switch (type) {
		case PSTT_START_TAG:
			ENTER_NODE(device, object, DEVICE)
			break;
		case PSTT_ATTRIBUTE:
			ADD_FIELD(object, name) else
			ADD_FIELD(object, url)
			break;
		case PSTT_END_TAG:
			if (p_cmpToken(&fullName, "object") != 0) return 0;
			ctx->state = PCS_END;
			CHECK_FIELD(object, name)
			CHECK_FIELD(object, url)
			break;
		default:
			return 0; /* invalid token */
		}
		break;
	case PCS_WITHIN_DEVICE:
		switch (type) {
		case PSTT_START_TAG:
			ENTER_NODE(service, device, SERVICE)
			break;
		case PSTT_ATTRIBUTE:
			ADD_FIELD(device, name)
			break;
		case PSTT_END_TAG:
			LEAVE_NODE(device, OBJECT)
			CHECK_FIELD(device, name)
			break;
		default:
			return 0; /* invalid token */
		}
		break;
	case PCS_WITHIN_SERVICE:
		switch (type) {
		case PSTT_START_TAG:
			ENTER_NODE(action, service, ACTION)
			break;
		case PSTT_ATTRIBUTE:
			ADD_FIELD(service, name) else
			ADD_FIELD(service, type) else
			ADD_FIELD(service, path)  else
			ADD_FIELD(service, control)
			break;
		case PSTT_END_TAG:
			LEAVE_NODE(service, DEVICE)
			CHECK_FIELD(service, name)
			CHECK_FIELD(service, type)
			CHECK_FIELD(service, path)
			CHECK_FIELD(service, control)
			break;
		default:
			return 0; /* invalid token */
		}
		break;
	case PCS_WITHIN_ACTION:
		switch (type) {
		case PSTT_START_TAG:
			ENTER_NODE(arg, action, ARG)
			break;
		case PSTT_ATTRIBUTE:
			ADD_FIELD(action, name)
			break;
		case PSTT_END_TAG:
			LEAVE_NODE(action, SERVICE)
			CHECK_FIELD(action, name)
			break;
		default:
			return 0; /* invalid token */
		}
		break;
	case PCS_WITHIN_ARG:
		switch (type) {
		case PSTT_ATTRIBUTE:
			ADD_FIELD(arg, name) else
			ADD_FIELD(arg, var) else
			ADD_FIELD(arg, type) else
			ADD_FIELD(arg, dir)
			break;
		case PSTT_END_TAG:
			LEAVE_NODE(arg, ACTION)
			CHECK_FIELD(arg, name)
			CHECK_FIELD(arg, var)
			CHECK_FIELD(arg, type)
			CHECK_FIELD(arg, dir)
			break;
		default:
			return 0; /* invalid token */
		}
		break;
	case PCS_END:
		return 0; /* no tokens allowed at this point */
	}
	
#undef ENTER_NODE
#undef ADD_FIELD
#undef LEAVE_NODE
#undef CHECK_FIELD
	
	return 1;
}


/**
 * Parses the type name from a deviceType node content.
 * 
 * @param[in] str - deviceType node content
 * @param[in] len - str length in bytes
 * @return parsed and allocated string or NULL on error
 */
static char * parseDeviceName(const char * str, const size_t len) {
	static const char devicePrefix[] = "urn:dslforum-org:device:";
	const char * ptr = devicePrefix;
	size_t i;
	for (i = 0; i < len && i < sizeof(devicePrefix) && *ptr == *str; i++, ptr++, str++);
	if ((i + 1) != sizeof(devicePrefix)) return NULL;
	return strndupInternal(str, len - i);
}


/**
 * Parses the type name from a serviceType node content.
 * 
 * @param[in] str - serviceType node content
 * @param[in] len - str length in bytes
 * @return parsed and allocated string or NULL on error
 */
static char * parseServiceName(const char * str, const size_t len) {
	static const char servicePrefix[] = "urn:dslforum-org:service:";
	const char * ptr = servicePrefix;
	size_t i;
	for (i = 0; i < len && i < sizeof(servicePrefix) && *ptr == *str; i++, ptr++, str++);
	if ((i + 1) != sizeof(servicePrefix)) return NULL;
	return strndupInternal(str, len - i);
}


/**
 * Compares the given token array with the path given.
 * 
 * @param[in] tokens - token array representing the path
 * @param[in] count - number of tokens given
 * @param[in] path - path as string array
 * @return 1 if equal, else 0
 */
static int cmpXmlPath(const tPToken * tokens, const size_t count, const char * const * path) {
	size_t i = 0;
	for (; *path != NULL && i < count && p_cmpToken(tokens, *path) == 0; i++, tokens++, path++);
	return (*path == NULL && i == count) ? 1 : 0;
}


/**
 * Compares the given full qualified XML token with the namespace token and and element name.
 * 
 * @param[in] full - full qualified XML token
 * @param[in] ns - namespace token (can be all zero for no namespace)
 * @param[in] str - element name
 * @return 1 if equal, else 0
 */
static int cmpXmlWithNs(const tPToken * full, const tPToken * ns, const char * str) {
	if (full == NULL || str == NULL) return 0;
	if (ns != NULL && ns->start != NULL) {
		if (full->length < (ns->length + 1)) return 0;
		const tPToken fullNs = {
			/* .start  = */ full->start,
			/* .length = */ ns->length
		};
		if (p_cmpTokens(&fullNs, ns) != 0) return 0;
		if (full->start[ns->length] != ':') return 0;
		const size_t prefixLength = ns->length + 1;
		const tPToken fullName = {
			/* .start  = */ full->start + prefixLength,
			/* .length = */ (size_t)(full->length - prefixLength)
		};
		return p_cmpToken(&fullName, str) ? 0 : 1;
	}
	return p_cmpToken(full, str) ? 0 : 1;
}


/**
 * Callback to parse the device description XML format.
 * 
 * @param[in] type - token type
 * @param[in] tokens - tokens passed (1 for xml, tags, instructions, contents and cdata; 2 for attributes)
 * @param[in] level - token level (i.e. number of parents)
 * @param[in,out] param - user defined callback data (expects tPTrObjectDeviceCtx)
 * @return 1 to continue, 0 on error
 * @see newTrObject()
 */
static int xmlDeviceDescVisitor(const tPSaxTokenType type, const tPToken * tokens, const size_t level, void * param) {
	typedef struct {
		const char * path[9];
		size_t depth;
	} tXmlPath;
	static const tXmlPath devicePaths[] = {
		{{"root", "device", NULL}, 2},
		{{"root", "device", "deviceList", "device", NULL}, 4},
		{{"root", "device", "deviceList", "device", "deviceList", "device", NULL}, 6}
	};
	static const tXmlPath servicePaths[] = {
		{{"root", "device", "serviceList", "service", NULL}, 4},
		{{"root", "device", "deviceList", "device", "serviceList", "service", NULL}, 6},
		{{"root", "device", "deviceList", "device", "deviceList", "device", "serviceList", "service", NULL}, 8}
	};
	tPTrObjectDeviceCtx * ctx = (tPTrObjectDeviceCtx *)param;
	tPToken fullName;
	if (tokens == NULL || ctx == NULL || level >= MAX_XML_DEPTH) return 0;
	if (xmlToFullName(type, &fullName, tokens) != 1) return 0;
#define ENTER_NODE(item, root) \
	if (ctx->root == NULL) return 0; \
	if (ctx->root->length >= ctx->root->capacity) { \
		if (arrayFieldResize(ctx->root, item, PCF_MAX(INIT_ARRAY_SIZE, ctx->root->capacity * 2)) != 1) { \
			ctx->lastError = MSGT_ERR_NO_MEM; \
			return 0; \
		} \
	} \
	ctx->item = ctx->root->item + ctx->root->length; \
	memset(ctx->item, 0, sizeof(*(ctx->item))); \
	ctx->root->length++;
	switch (type) {
	case PSTT_PARSE_XML:
	case PSTT_XML:
		if (level != 0) return 0; /* invalid token */
		break;
	case PSTT_START_TAG:
		ctx->xmlPath[level] = fullName;
		memset(&(ctx->content), 0, sizeof(ctx->content));
		if (p_cmpToken(&fullName, "device") == 0) {
			if (ctx->device != NULL) {
				if (ctx->device->name == NULL) return 0;
			}
			ENTER_NODE(device, object)
		} else if (p_cmpToken(&fullName, "service") == 0) {
			if (ctx->service != NULL) {
				if (ctx->service->name == NULL) return 0;
				if (ctx->service->type == NULL) return 0;
				if (ctx->service->control == NULL) return 0;
				if (ctx->service->path == NULL) return 0;
			}
			ENTER_NODE(service, device)
		}
		break;
	case PSTT_ATTRIBUTE:
		/* ignored */
		break;
	case PSTT_CONTENT:
		ctx->content = *tokens;
		break;
	case PSTT_END_TAG:
		if (p_cmpTokens(ctx->xmlPath + level, &fullName) != 0) {
			return 0; /* end tag mismatch */
		} else {
			if (p_cmpToken(&fullName, "service") == 0) {
				/* check service fields */
				if (ctx->service != NULL) {
					if (ctx->service->name == NULL) return 0;
					if (ctx->service->type == NULL) return 0;
					if (ctx->service->control == NULL) return 0;
					if (ctx->service->path == NULL) return 0;
				} else {
					return 0;
				}
			} else if (p_cmpToken(&fullName, "device") == 0) {
				/* check device fields */
				if (ctx->device != NULL) {
					if (ctx->device->name == NULL) return 0;
				} else {
					return 0;
				}
			} else if (ctx->content.start != NULL) {
				/* add field */
				char ** field = NULL;
				const tXmlPath * pathList = NULL;
				if (p_cmpToken(&fullName, "friendlyName") == 0 && ctx->object->name == NULL) {
					field = &(ctx->object->name);
					pathList = devicePaths;
				} else if (p_cmpToken(&fullName, "deviceType") == 0) {
					field = &(ctx->device->name);
					pathList = devicePaths;
				} else if (p_cmpToken(&fullName, "serviceType") == 0) {
					field = &(ctx->service->type);
					pathList = servicePaths;
				} else if (p_cmpToken(&fullName, "controlURL") == 0) {
					field = &(ctx->service->control);
					pathList = servicePaths;
				} else if (p_cmpToken(&fullName, "SCPDURL") == 0) {
					field = &(ctx->service->path);
					pathList = servicePaths;
				}
				if (field != NULL) {
					int pass = 0;
					for (size_t i = 0; i < 3; i++) {
						if (level == pathList[i].depth && cmpXmlPath(ctx->xmlPath, level, pathList[i].path) == 1) {
							pass = 1;
							break;
						}
					}
					if (pass != 0) {
						if (ctx->device != NULL && field == &(ctx->device->name)) {
							ctx->device->name = parseDeviceName(ctx->content.start, ctx->content.length);
							if (ctx->device->name == NULL) {
								ctx->lastError = MSGT_ERR_NO_MEM;
								return 0;
							}
						} else {
							*field = strndupInternal(ctx->content.start, ctx->content.length);
							if (ctx->service != NULL && field == &(ctx->service->type)) {
								ctx->service->name = parseServiceName(ctx->content.start, ctx->content.length);
								if (ctx->service->name == NULL) {
									ctx->lastError = MSGT_ERR_NO_MEM;
									return 0;
								}
							}
						}
						if (*field == NULL) {
							ctx->lastError = MSGT_ERR_NO_MEM;
							return 0;
						}
					} else {
						return 0; /* valid token in invalid path */
					}
				}
			}
			memset(&(ctx->xmlPath[level]), 0, sizeof(ctx->xmlPath[level]));
		}
		break;
	default:
		return 0; /* invalid token */
		break;
	}
	return 1;
}


/**
 * Callback to parse the service description XML format.
 * 
 * @param[in] type - token type
 * @param[in] tokens - tokens passed (1 for xml, tags, instructions, contents and cdata; 2 for attributes)
 * @param[in] level - token level (i.e. number of parents)
 * @param[in,out] param - user defined callback data (expects tPTrObjectServiceCtx)
 * @return 1 to continue, 0 on error
 * @see newTrObject()
 */
static int xmlServiceDescVisitor(const tPSaxTokenType type, const tPToken * tokens, const size_t level, void * param) {
	typedef struct {
		const char * path[9];
		size_t depth;
	} tXmlPath;
	static const tXmlPath actionPath = {{"scpd", "actionList", "action", NULL}, 3};
	static const tXmlPath argPath = {{"scpd", "actionList", "action", "argumentList", "argument", NULL}, 5};
	static const tXmlPath statePath = {{"scpd", "serviceStateTable", "stateVariable", NULL}, 3};
	tPTrObjectServiceCtx * ctx = (tPTrObjectServiceCtx *)param;
	tPToken fullName;
	if (tokens == NULL || ctx == NULL || level >= MAX_XML_DEPTH) return 0;
	if (xmlToFullName(type, &fullName, tokens) != 1) return 0;
	switch (type) {
	case PSTT_PARSE_XML:
	case PSTT_XML:
		if (level != 0) return 0; /* invalid token */
		break;
	case PSTT_START_TAG:
		ctx->xmlPath[level] = fullName;
		memset(&(ctx->content), 0, sizeof(ctx->content));
		if (p_cmpToken(&fullName, "action") == 0) {
			if (ctx->action != NULL) {
				if (ctx->action->name == NULL) return 0;
			}
			ENTER_NODE(action, service)
		} else if (p_cmpToken(&fullName, "argument") == 0) {
			if (ctx->arg != NULL) {
				if (ctx->arg->name == NULL) return 0;
				if (ctx->arg->var == NULL) return 0;
				if (ctx->arg->dir == NULL) return 0;
			}
			ENTER_NODE(arg, action)
		} else if (p_cmpToken(&fullName, "stateVariable") == 0) {
			memset(&(ctx->stateVarName), 0, sizeof(ctx->stateVarName));
		}
		break;
	case PSTT_ATTRIBUTE:
		/* ignored */
		break;
	case PSTT_CONTENT:
		ctx->content = *tokens;
		break;
	case PSTT_END_TAG:
		if (p_cmpTokens(ctx->xmlPath + level, &fullName) != 0) {
			return 0; /* end tag mismatch */
		} else {
			if (p_cmpToken(&fullName, "argument") == 0) {
				/* check argument fields */
				if (ctx->arg != NULL) {
					if (ctx->arg->name == NULL) return 0;
					if (ctx->arg->var == NULL) return 0;
					if (ctx->arg->dir == NULL) return 0;
				} else {
					return 0;
				}
			} else if (p_cmpToken(&fullName, "action") == 0) {
				/* check action fields */
				if (ctx->action != NULL) {
					if (ctx->action->name == NULL) return 0;
				} else {
					return 0;
				}
			} else if (ctx->content.start != NULL) {
				/* add field */
				char ** field = NULL;
				if (p_cmpToken(&fullName, "name") == 0) {
					if (level == actionPath.depth && cmpXmlPath(ctx->xmlPath, level, actionPath.path) == 1) {
						field = &(ctx->action->name);
					} else if (level == argPath.depth && cmpXmlPath(ctx->xmlPath, level, argPath.path) == 1) {
						field = &(ctx->arg->name);
					} else if (level == statePath.depth && cmpXmlPath(ctx->xmlPath, level, statePath.path) == 1) {
						ctx->stateVarName = ctx->content;
					}
				} else if (p_cmpToken(&fullName, "relatedStateVariable") == 0) {
					if (level == argPath.depth && cmpXmlPath(ctx->xmlPath, level, argPath.path) == 1) {
						field = &(ctx->arg->var);
					}
				} else if (p_cmpToken(&fullName, "direction") == 0) {
					if (level == argPath.depth && cmpXmlPath(ctx->xmlPath, level, argPath.path) == 1) {
						field = &(ctx->arg->dir);
					}
				} else if (ctx->service != NULL && ctx->stateVarName.start != NULL && p_cmpToken(&fullName, "dataType") == 0) {
					if (level == statePath.depth && cmpXmlPath(ctx->xmlPath, level, statePath.path) == 1) {
						/* set dataType for relatedStateVariable */
						for (size_t ac = 0; ac < ctx->service->length; ac++) {
							tTrAction * action = ctx->service->action + ac;
							for (size_t ar = 0; ar < action->length; ar++) {
								tTrArgument * arg = action->arg + ar;
								if (arg->var == NULL || arg->type != NULL) continue;
								if (p_cmpToken(&(ctx->stateVarName), arg->var) == 0) {
									arg->type = strndupInternal(ctx->content.start, ctx->content.length);
									if (arg->type == NULL) {
										ctx->lastError = MSGT_ERR_NO_MEM;
										return 0;
									}
								}
							}
						}
					}
				}
				if (field != NULL) {
					*field = strndupInternal(ctx->content.start, ctx->content.length);
					if (*field == NULL) {
						ctx->lastError = MSGT_ERR_NO_MEM;
						return 0;
					}
				}
			}
			memset(&(ctx->xmlPath[level]), 0, sizeof(ctx->xmlPath[level]));
		}
		break;
	default:
		return 0; /* invalid token */
		break;
	}
#undef ENTER_NODE
	return 1;
}


/**
 * Creates a new TR-064 object according to the given parameters. The caller needs to resolve the
 * host address in ctx.
 * 
 * @param[in,out] ctx - context to use
 * @param[in] opt - options to use
 * @return Object on success, else NULL.
 */
tTrObject * newTrObject(tTr64RequestCtx * ctx, const tOptions * opt) {
	if (ctx == NULL || opt == NULL) return NULL;
	static const char * request =
		"GET /%s HTTP/1.1\r\n"
		"Host: %s:%s\r\n"
		"\r\n"
	;
	char * xml = NULL;
	const char * xmlErrPos;
	char * escName = NULL;
	char * escUrl = NULL;
	size_t xmlLen = 0;
	int ok;
	tTrObject * obj = NULL;
	tTrObject * res = NULL;
	
	obj = (tTrObject *)calloc(1, sizeof(tTrObject));
	if (obj == NULL) return NULL;
	
	/* read from cache */
	if (isFile(opt->cache) == 1) {
		xml = readFileToString(opt->cache, &xmlLen);
		if (xml == NULL || xmlLen < 1) {
			if (xml != NULL) free(xml);
			if (ctx->verbose > 1) _ftprintf(ferr, MSGT(MSGT_WARN_CACHE_READ));
			goto onCacheFileError;
		}
		/* parse XML and fill in TR-064 object elements */
		tPTrObjectCacheCtx xmlCtx = {
			/* .object  = */ obj,
			/* .device  = */ NULL,
			/* .service = */ NULL,
			/* .action  = */ NULL,
			/* .arg     = */ NULL,
			/* .state   = */ PCS_START
		};
		xmlErrPos = NULL;
		if (p_sax(xml, xmlLen, &xmlErrPos, xmlCacheFileVisitor, &xmlCtx) != PSRT_SUCCESS) {
			if (ctx->verbose > 1) _ftprintf(ferr, MSGT(MSGT_WARN_CACHE_FMT));
			if (ctx->verbose > 3) {
				tParserPos pos;
				if (p_getPos(xml, xmlLen, xmlErrPos, 1, &pos) == 1) {
					_ftprintf(ferr, MSGT(MSGT_DBG_BAD_TOKEN), (unsigned)pos.line, (unsigned)pos.column);
				}
			}
			free(xml);
			/* re-initialize object to discard fragments from cache file */
			freeTrObject(obj);
			obj = (tTrObject *)calloc(1, sizeof(tTrObject));
			if (obj == NULL) goto onError;
			goto onCacheFileError;
		}
		free(xml);
		/* unescape object name */
		if (p_unescapeXmlVar(&(obj->name), NULL, 0) != 1) {
			if (ctx->verbose > 1) _ftprintf(ferr, MSGT(MSGT_WARN_CACHE_UNESC));
			goto onUnescapeOutOfMemoryError;
		}
		/* unescape object URL */
		if (p_unescapeXmlVar(&(obj->url), NULL, 0) != 1) {
			if (ctx->verbose > 1) _ftprintf(ferr, MSGT(MSGT_WARN_CACHE_UNESC));
			goto onUnescapeOutOfMemoryError;
		}
		/* check if cached URL matches requested one */
		if (strcmp(obj->url, opt->url) == 0) {
			return obj;
		}
onUnescapeOutOfMemoryError:
		/* re-initialize object to discard fragments from cache file */
		freeTrObject(obj);
		obj = (tTrObject *)calloc(1, sizeof(tTrObject));
		if (obj == NULL) goto onError;
	}
	
onCacheFileError:
	xml = NULL;
	
	/* read from device */
	/* read device description */
	ctx->length = 0;
	if (formatToCtxBuffer(ctx, request, ctx->path, ctx->host, ctx->port) != 1) {
		if (ctx->verbose > 1) _ftprintf(ferr, MSGT(MSGT_ERR_FMT_DEV_DESC));
		goto onError;
	}
	if (ctx->verbose > 3) {
		fuprintf(ferr, MSGU(MSGU_INFO_DEV_DESC_REQ), ctx->path);
	}
	if (ctx->request(ctx) != 1) {
		if (ctx->verbose > 1) _ftprintf(ferr, MSGT(MSGT_ERR_GET_DEV_DESC), (unsigned)(ctx->status));
		goto onError;
	}
	if (ctx->verbose > 3) _ftprintf(ferr, MSGT(MSGT_INFO_DEV_DESC_DUR), (unsigned)(ctx->duration));
	/* parse device descriptions in used callback (see xmlDeviceDescVisitor()) */
	{
		obj->url = strdup(opt->url);
		if (obj->url == NULL) {
			if (ctx->verbose > 1) _ftprintf(ferr, MSGT(MSGT_ERR_NO_MEM));
			goto onError;
		}
		tPTrObjectDeviceCtx devCtx = {
			/* .xmlPath    = */ {{0}},
			/* .object     = */ obj,
			/* .device     = */ NULL,
			/* .service    = */ NULL,
			/* .content    = */ {0},
			/* .lastError  = */ MSGT_SUCCESS
		};
		const size_t contentLength = (ctx->buffer + ctx->length) - ctx->content;
		xmlErrPos = NULL;
		if (p_sax(ctx->content, contentLength, &xmlErrPos, xmlDeviceDescVisitor, &devCtx) != PSRT_SUCCESS) {
			if (devCtx.lastError != MSGT_SUCCESS) {
				if (ctx->verbose > 0) _ftprintf(ferr, MSGT(devCtx.lastError));
			} else {
				if (ctx->verbose > 0) {
					fuprintf(ferr, MSGU(MSGU_ERR_DEV_DESC_FMT), ctx->path);
				}
				if (ctx->verbose > 3) {
					tParserPos pos;
					if (p_getPos(ctx->content, contentLength, xmlErrPos, 1, &pos) == 1) {
						_ftprintf(ferr, MSGT(MSGT_DBG_BAD_TOKEN), (unsigned)pos.line, (unsigned)pos.column);
					}
				}
			}
			goto onError;
		}
	}
	/* read and parse service descriptions */
	for (size_t d = 0; d < obj->length; d++) {
		tTrDevice * device = obj->device + d;
		if (device->service == NULL) continue;
		for (size_t s = 0; s < device->length; s++) {
			tTrService * service = device->service + s;
			/* read service description */
			/* skip leading slash (/) in service->path as it is already included in request */
			ctx->length = 0;
			if (formatToCtxBuffer(ctx, request, service->path + 1, ctx->host, ctx->port) != 1) {
				if (ctx->verbose > 1) _ftprintf(ferr, MSGT(MSGT_ERR_FMT_SRVC_DESC));
				goto onError;
			}
			if (ctx->verbose > 3) {
				fuprintf(ferr, MSGU(MSGU_INFO_SRVC_DESC_REQ), service->path);
			}
			if (ctx->request(ctx) != 1) {
				if (ctx->verbose > 1) _ftprintf(ferr, MSGT(MSGT_ERR_GET_SRVC_DESC), (unsigned)(ctx->status));
				goto onError;
			}
			if (ctx->verbose > 3) _ftprintf(ferr, MSGT(MSGT_INFO_SRVC_DESC_DUR), (unsigned)(ctx->duration));
			/* parse service description in used callback (see xmlServiceDescVisitor()) */
			{
				tPTrObjectServiceCtx serviceCtx = {
					/* .xmlPath      = */ {{0}},
					/* .service      = */ service,
					/* .action       = */ NULL,
					/* .arg          = */ NULL,
					/* .content      = */ {0},
					/* .stateVarName = */ {0},
					/* .lastError    = */ MSGT_SUCCESS
				};
				const size_t contentLength = (ctx->buffer + ctx->length) - ctx->content;
				xmlErrPos = NULL;
				if (p_sax(ctx->content, contentLength, &xmlErrPos, xmlServiceDescVisitor, &serviceCtx) != PSRT_SUCCESS) {
					if (serviceCtx.lastError != MSGT_SUCCESS) {
						if (ctx->verbose > 0) _ftprintf(ferr, MSGT(serviceCtx.lastError));
					} else {
						if (ctx->verbose > 0) {
							fuprintf(ferr, MSGU(MSGU_ERR_DEV_SRVC_FMT), service->path);
						}
						if (ctx->verbose > 3) {
							tParserPos pos;
							if (p_getPos(ctx->content, contentLength, xmlErrPos, 1, &pos) == 1) {
								_ftprintf(ferr, MSGT(MSGT_DBG_BAD_TOKEN), (unsigned)pos.line, (unsigned)pos.column);
							}
						}
					}
					goto onError;
				}
				/* check if we got a type for each argument variable */
				if (service->action != NULL) {
					for (size_t ac = 0; ac < service->length; ac++) {
						const tTrAction * action = service->action + ac;
						if (action->arg == NULL) continue;
						for (size_t ar = 0; ar < action->length; ar++) {
							const tTrArgument * arg = action->arg + ar;
							if (arg->type == NULL) {
								if (ctx->verbose > 0) fuprintf(ferr, MSGU(MSGU_ERR_NO_TYPE_FOR_ARG), arg->var);
								goto onError;
							}
						}
					}
				}
			}
		}
	}
	
	/* store new cache file (format output errors are ignored until the end) */
	ctx->length = 0;
	if (opt->cache == NULL) goto onSuccess;
	ok = formatToCtxBuffer(ctx, "%s", "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
	escName = p_escapeXml(obj->name, (size_t)-1);
	if (escName == NULL) goto onFormatOutOfMemoryError;
	escUrl = p_escapeXml(obj->url, (size_t)-1);
	if (escUrl == NULL) goto onFormatOutOfMemoryError;
	ok &= formatToCtxBuffer(ctx, "<object name=\"%s\" url=\"%s\">\n", escName, escUrl);
	if (obj->device != NULL) {
		for (size_t d = 0; d < obj->length; d++) {
			const tTrDevice * device = obj->device + d;
			ok &= formatToCtxBuffer(ctx, " <device name=\"%s\">\n", device->name);
			if (device->service != NULL) {
				for (size_t s = 0; s < device->length; s++) {
					const tTrService * service = device->service + s;
					ok &= formatToCtxBuffer(ctx, "  <service name=\"%s\" type=\"%s\" path=\"%s\" control=\"%s\">\n", service->name, service->type, service->path, service->control);
					if (service->action != NULL) {
						for (size_t ac = 0; ac < service->length; ac++) {
							const tTrAction * action = service->action + ac;
							ok &= formatToCtxBuffer(ctx, "   <action name=\"%s\">\n", action->name);
							if (action->arg != NULL) {
								for (size_t ar = 0; ar < action->length; ar++) {
									const tTrArgument * arg = action->arg + ar;
									ok &= formatToCtxBuffer(ctx, "    <arg name=\"%s\" var=\"%s\" type=\"%s\" dir=\"%s\"/>\n", arg->name, arg->var, arg->type, arg->dir);
								}
							}
							ok &= formatToCtxBuffer(ctx, "   </action>\n");
						}
					}
					ok &= formatToCtxBuffer(ctx, "  </service>\n");
				}
			}
			ok &= formatToCtxBuffer(ctx, " </device>\n");
		}
	}
	ok &= formatToCtxBuffer(ctx, "</object>\n");
	if (ok != 1) {
onFormatOutOfMemoryError:
		if (ctx->verbose > 1) _ftprintf(ferr, MSGT(MSGT_WARN_CACHE_NO_MEM));
	} else if (writeStringNToFile(opt->cache, ctx->buffer, ctx->length) != 1) {
		if (ctx->verbose > 1) _ftprintf(ferr, MSGT(MSGT_WARN_CACHE_WRITE));
	}
	if (escName != NULL && escName != obj->name) free(escName);
	if (escUrl != NULL && escUrl != obj->url) free(escUrl);
	
onSuccess:
	res = obj;
onError:
	if (res == NULL && obj != NULL) freeTrObject(obj);
	return res;
}


/**
 * Sets the value for the given argument to the passed string. A new string will be allocated from
 * it and assigned. Any previously assigned value will be freed beforehand. Use null for str to
 * clear the argument value.
 * 
 * @param[in] arg - argument to set value for
 * @param[in] str - string to set as value
 * @return 1 on success, else 0
 */
int setArgValue(tTrArgument * arg, const char * str) {
	if (arg == NULL) return 0;
	if (arg->value != NULL) free(arg->value);
	if (str != NULL) {
		arg->value = strdup(str);
		if (arg->value == NULL) return 0;
	} else {
		arg->value = NULL;
	}
	return 1;
}


/**
 * Sets the value for the given argument to the passed string. A new string will be allocated from
 * it and assigned. Any previously assigned value will be freed beforehand. Use null for str to
 * clear the argument value.
 * 
 * @param[in] arg - argument to set value for
 * @param[in] str - string to set as value
 * @param[in] len - string length in bytes
 * @return 1 on success, else 0
 */
int setArgValueN(tTrArgument * arg, const char * str, const size_t len) {
	if (arg == NULL) return 0;
	if (arg->value != NULL) free(arg->value);
	if (str != NULL) {
		arg->value = strndupInternal(str, len);
		if (arg->value == NULL) return 0;
	} else {
		arg->value = NULL;
	}
	return 1;
}


/**
 * Deletes the allocated TR-064 object.
 * 
 * @param[in] obj - object to delete
 */
void freeTrObject(tTrObject * obj) {
	if (obj == NULL) return;
	if (obj->name != NULL) free(obj->name);
	if (obj->url != NULL) free(obj->url);
	if (obj->device != NULL) {
		for (size_t d = 0; d < obj->length; d++) {
			tTrDevice * device = obj->device + d;
			if (device->name != NULL) free(device->name);
			if (device->service == NULL) continue;
			for (size_t s = 0; s < device->length; s++) {
				tTrService * service = device->service + s;
				if (service->name != NULL) free(service->name);
				if (service->type != NULL) free(service->type);
				if (service->path != NULL) free(service->path);
				if (service->control != NULL) free(service->control);
				if (service->action == NULL) continue;
				for (size_t ac = 0; ac < service->length; ac++) {
					tTrAction * action = service->action + ac;
					if (action->name != NULL) free(action->name);
					if (action->arg == NULL) continue;
					for (size_t ar = 0; ar < action->length; ar++) {
						tTrArgument * arg = action->arg + ar;
						if (arg->name != NULL) free(arg->name);
						if (arg->var != NULL) free(arg->var);
						if (arg->value != NULL) free(arg->value);
						if (arg->type != NULL) free(arg->type);
						if (arg->dir != NULL) free(arg->dir);
					}
					free(action->arg);
				}
				free(service->action);
			}
			free(device->service);
		}
		free(obj->device);
	}
	free(obj);
}


/**
 * Callback to parse the query response XML/SOAP format. The argument value is set unescaped.
 * 
 * @param[in] type - token type
 * @param[in] tokens - tokens passed (1 for xml, tags, instructions, contents and cdata; 2 for attributes)
 * @param[in] level - token level (i.e. number of parents)
 * @param[in,out] param - user defined callback data (expects tPTrQueryRespCtx)
 * @return 1 to continue, 0 on error
 * @see trQuery()
 */
static int xmlQueryRespVisitor(const tPSaxTokenType type, const tPToken * tokens, const size_t level, void * param) {
	static const char * soapNs = "http://schemas.xmlsoap.org/soap/envelope/";
	static const char userNs[] = "urn:dslforum-org:service:"; /* sizeof includes null-terminator */
	static const char resp[] = "Response"; /* sizeof includes null-terminator */
	tPTrQueryRespCtx * ctx = (tPTrQueryRespCtx *)param;
	tPToken fullName, respToken;
	int found;
	if (tokens == NULL || ctx == NULL || level >= MAX_XML_DEPTH) return 0;
	if (xmlToFullName(type, &fullName, tokens) != 1) return 0;
	switch (type) {
	case PSTT_PARSE_XML:
	case PSTT_XML:
		if (level != 0) return 0; /* invalid token */
		break;
	case PSTT_START_TAG:
		ctx->xmlPath[level] = fullName;
		memset(&(ctx->content), 0, sizeof(ctx->content));
		if (level == 0 && p_cmpToken(tokens + 1, "Envelope") == 0) {
			/* SOAP envelope */
			ctx->soapNs = *tokens;
			ctx->depth++;
		} else if (level == 1 && ctx->depth == 1 && cmpXmlWithNs(&fullName, &(ctx->soapNs), "Header") == 1) {
			/* SOAP header */
			/* ignored */
		} else if (level == 1 && ctx->depth == 1 && cmpXmlWithNs(&fullName, &(ctx->soapNs), "Body") == 1) {
			/* SOAP body */
			ctx->depth++;
		} else if (level < 2) {
			return 0; /* invalid token */
		} else if (level == 2 && ctx->depth == 2 && (ctx->userNs.start == NULL || p_cmpTokens(tokens, &(ctx->userNs)) == 0)) {
			/* TR-064 action response */
			if (tokens[1].length <= sizeof(resp)) {
				ctx->lastError = MSGT_ERR_QUERY_RESP_ACTION;
				return 0; /* response name mismatch */
			}
			respToken.start  = tokens[1].start;
			respToken.length = tokens[1].length - sizeof(resp) + 1;
			if (p_cmpToken(&respToken, ctx->action->name) != 0) {
				ctx->lastError = MSGT_ERR_QUERY_RESP_ACTION;
				return 0; /* response name mismatch */
			}
			respToken.start  = respToken.start + respToken.length;
			respToken.length = (size_t)(sizeof(resp) - 1);
			if (p_cmpToken(&respToken, resp) != 0) {
				ctx->lastError = MSGT_ERR_QUERY_RESP_ACTION;
				return 0; /* response name mismatch */
			}
			ctx->userNs = *tokens;
			ctx->depth++;
		} else if (level == 3 && ctx->depth == 3 && (ctx->userNs.start == NULL || tokens[0].start == NULL || p_cmpTokens(tokens, &(ctx->userNs)) == 0)) {
			/* TR-064 action response argument */
			ctx->depth++;
		}
		break;
	case PSTT_ATTRIBUTE:
		if (level == 1 && cmpXmlWithNs(ctx->xmlPath + level, &(ctx->soapNs), "Envelope") == 1 && p_cmpToken(tokens, "xmlns") == 0) {
			if (p_cmpTokens(tokens + 1, &(ctx->soapNs)) != 0 || p_cmpToken(tokens + 2, soapNs) != 0) {
				return 0; /* namespace mismatch */
			}
		} else if (level == 2 && ctx->userNs.start != NULL && p_cmpToken(tokens, "xmlns") == 0) {
			tPToken nsPart = tokens[2];
			nsPart.length = PCF_MIN(sizeof(userNs) - 1, nsPart.length);
			if (p_cmpTokens(tokens + 1, &(ctx->userNs)) != 0 || p_cmpToken(&nsPart, userNs) != 0) {
				return 0; /* namespace mismatch */
			}
		}
		break;
	case PSTT_CONTENT:
		ctx->content = *tokens;
		break;
	case PSTT_END_TAG:
		if (p_cmpTokens(ctx->xmlPath + level, &fullName) != 0) {
			return 0; /* end tag mismatch */
		} else if (level == 0 && ctx->depth == 1) {
			/* SOAP envelope */
			ctx->depth--;
			memset(&(ctx->soapNs), 0, sizeof(ctx->soapNs));
		} else if (level == 1 && ctx->depth == 1 && p_cmpToken(tokens + 1, "Header") == 0) {
			/* SOAP header */
			/* ignored */
		} else if (level == 1 && ctx->depth == 2 && p_cmpToken(tokens + 1, "Body") == 0) {
			/* SOAP body */
			ctx->depth--;
			memset(&(ctx->userNs), 0, sizeof(ctx->userNs));
		} else if (level < 2) {
			return 0; /* invalid token */
		} else if (level == 2 && ctx->depth == 3) {
			/* TR-064 action response */
			ctx->depth--;
		} else if (level == 3 && ctx->depth == 4) {
			/* TR-064 action response argument */
			ctx->depth--;
			/* find corresponding argument and set received value as newly allocated string */
			found = 0;
			for (size_t ar = 0; ar < ctx->action->length; ar++) {
				tTrArgument * arg = ctx->action->arg + ar;
				if (strcmp(arg->dir, "out") != 0) continue;
				if (p_cmpToken(tokens + 1, arg->name) != 0) continue;
				if (setArgValueN(arg, ctx->content.start, ctx->content.length) != 1) {
					ctx->lastError = MSGT_ERR_NO_MEM;
					return 0; /* allocation error */
				}
				errno = 0;
				if (arg->value != NULL && p_unescapeXmlVar(&(arg->value), NULL, 0) != 1) { ///////// bug
					if (errno == EINVAL) {
						ctx->lastError = MSGT_ERR_QUERY_RESP_ARG_BAD_ESC;
					} else {
						ctx->lastError = MSGT_ERR_NO_MEM;
					}
					return 0; /* allocation error */
				}
				found = 1;
				break;
			}
			if (found != 1) {
				ctx->lastError = MSGT_ERR_QUERY_RESP_ARG;
				return 0; /* unknown action argument */
			}
		}
		break;
	default:
		return 0; /* invalid token */
		break;
	}
	return 1;
}


/**
 * Escapes the given string to encode as CSV field.
 * 
 * @param[in] str - string to escape
 * @return escapes string or NULL on error
 * @remarks The returned string may be the same as the input if no escaping was needed.
 */
static char * escapeCsv(const char * str) {
	if (str == NULL) return NULL;
	/* calculate the result string size in bytes */
	size_t resSize = 1;
	size_t i = 0;
	for (const char * in = str; *in != 0; in++, i++, resSize++) {
		if (*in == '"') resSize++;
	}
	if ((i + 1) == resSize) return (char *)str;
	/* create the result string */
	char * res = (char *)malloc(sizeof(char) * resSize);
	if (res == NULL) return NULL;
	char * out = res;
	for (const char * in = str; *in != 0; in++) {
		if (*in == '"') *out++ = '"';
		*out++ = *in;
	}
	*out = 0;
	return res;
}


/**
 * Outputs the query result in CSV format.
 * 
 * @param[in,out] fd - output to this file descriptor
 * @param[in,out] qry - query handle
 * @param[in] action - action to output
 * @return 1 on success, else 0
 */
static int trQueryOutputCsv(FILE * fd, tTrQueryHandler * qry, const tTrAction * action) {
	if (fd == NULL || qry == NULL || action == NULL) return 0;
	int ok = 1, first = 1;
	char * escStr;
	
	qry->length = 0;
	/* build header */
	for (size_t ar = 0; ar < action->length; ar++) {
		tTrArgument * arg = action->arg + ar;
		if (strcmp(arg->dir, "out") != 0 || arg->value == NULL) continue;
		escStr = escapeCsv(arg->var);
		if (escStr == NULL) goto onOutOfMemory;
		ok &= formatToQryBuffer(qry, first ? "\"%s\"" : ",\"%s\"", escStr);
		if (escStr != arg->var) free(escStr);
		first = 0;
	}
	ok &= formatToQryBuffer(qry, "\n");
	if (ok != 1) goto onOutOfMemory;
	
	/* build record */
	first = 1;
	for (size_t ar = 0; ar < action->length; ar++) {
		tTrArgument * arg = action->arg + ar;
		if (strcmp(arg->dir, "out") != 0) continue;
		if (arg->value == NULL) {
			if (first != 0) ok &= formatToQryBuffer(qry, ",");
			continue;
		}
		escStr = escapeCsv(arg->value);
		if (escStr == NULL) goto onOutOfMemory;
		ok &= formatToQryBuffer(qry, first ? "\"%s\"" : ",\"%s\"", escStr);
		if (escStr != arg->value) free(escStr);
		first = 0;
	}
	ok &= formatToQryBuffer(qry, "\n");
	if (ok != 1) goto onOutOfMemory;
	
	if (fputUtf8N(fd, qry->buffer, qry->length) < 1) {
		if (qry->ctx->verbose > 1) _ftprintf(ferr, MSGT(MSGT_ERR_QUERY_PRINT));
		return 0;
	}
	
	return 1;
onOutOfMemory:
	if (qry->ctx->verbose > 1) _ftprintf(ferr, MSGT(MSGT_ERR_NO_MEM));
	return 0;
}


/**
 * Escapes the given string to encode as JSON string.
 * 
 * @param[in] str - string to escape
 * @return escapes string or NULL on error
 * @remarks The returned string may be the same as the input if no escaping was needed.
 * @see https://tools.ietf.org/html/rfc8259#section-7
 */
static char * escapeJson(const char * str) {
	static const char hex[] = "0123456789ABCDEF";
	if (str == NULL) return NULL;
	/* calculate the result string size in bytes */
	size_t resSize = 1;
	size_t i = 0;
	for (const char * in = str; *in != 0; in++, i++) {
		switch (*in) {
		case '"':
		case '\\':
		case '/':
		case '\b':
		case '\f':
		case '\n':
		case '\r':
		case '\t':
			resSize += 2;
			break;
		default:
			if (*in < 0x20) {
				resSize += 6;
			} else {
				resSize++;
			}
			break;
		}
	}
	if ((i + 1) == resSize) return (char *)str;
	/* create the result string */
	char * res = (char *)malloc(sizeof(char) * resSize);
	if (res == NULL) return NULL;
	char * out = res;
	for (const char * in = str; *in != 0; in++) {
		switch (*in) {
		case '"':  *out++ = '\\'; *out++ = '"';  break;
		case '\\': *out++ = '\\'; *out++ = '\\'; break;
		case '/':  *out++ = '\\'; *out++ = '/';  break;
		case '\b': *out++ = '\\'; *out++ = 'b';  break;
		case '\f': *out++ = '\\'; *out++ = 'f';  break;
		case '\n': *out++ = '\\'; *out++ = 'n';  break;
		case '\r': *out++ = '\\'; *out++ = 'r';  break;
		case '\t': *out++ = '\\'; *out++ = 't';  break;
		default:
			if (*in < 0x20) {
				*out++ = '\\';
				*out++ = 'u';
				*out++ = '0';
				*out++ = '0';
				*out++ = hex[(*in >> 4) & 0x0F];
				*out++ = hex[*in & 0x0F];
			} else {
				*out++ = *in;
			}
			break;
		}
	}
	*out = 0;
	return res;
}


/**
 * Maps the given TR-064 argument type to a JSON type.
 * 
 * @param[in] type - TR-064 argument type
 * @return mapped JSON type
 */
static tJsonType mapToJsonType(const char * type) {
	if (type == NULL) return JT_NULL;
	if (stricmp(type, "boolean") == 0) return JT_BOOLEAN;
	if (tolower(*type) == 'u') type++;
	if (tolower(*type) != 'i') return JT_STRING;
	char * endPtr = NULL;
	unsigned long bits = strtoul(type + 1, &endPtr, 10);
	if (endPtr != NULL && *endPtr == 0) {
		switch (bits) {
		case 1:
		case 2:
		case 4:
		case 8:
			return JT_NUMBER;
			break;
		default:
			break;
		}
	}
	return JT_STRING;
}


/**
 * Outputs the query result in JSON format.
 * 
 * @param[in,out] fd - output to this file descriptor
 * @param[in,out] qry - query handle
 * @param[in] action - action to output
 * @return 1 on success, else 0
 */
static int trQueryOutputJson(FILE * fd, tTrQueryHandler * qry, const tTrAction * action) {
	if (fd == NULL || qry == NULL || action == NULL) return 0;
	int ok, first = 1;
	char * escStr;
	
	qry->length = 0;
	escStr = escapeJson(action->name);
	if (escStr == NULL) goto onOutOfMemory;
	ok = formatToQryBuffer(qry, "{\"%s\":{\n", escStr);
	if (escStr != action->name) free(escStr);
	for (size_t ar = 0; ar < action->length; ar++) {
		tTrArgument * arg = action->arg + ar;
		if (strcmp(arg->dir, "out") != 0) continue;
		/* key */
		escStr = escapeJson(arg->var);
		if (escStr == NULL) goto onOutOfMemory;
		ok &= formatToQryBuffer(qry, first ? " \"%s\":" : ",\n \"%s\":", escStr);
		if (escStr != arg->var) free(escStr);
		/* value */
		if (arg->value == NULL) {
			ok &= formatToQryBuffer(qry, "null");
			first = 0;
			continue;
		}
		switch (mapToJsonType(arg->type)) {
		case JT_NULL:
			ok &= formatToQryBuffer(qry, "null");
			break;
		case JT_NUMBER:
			ok &= formatToQryBuffer(qry, "%s", arg->value);
			break;
		case JT_BOOLEAN:
			if (strcmp(arg->value, "0") == 0) {
				ok &= formatToQryBuffer(qry, "false");
				break;
			} else if (strcmp(arg->value, "1") == 0) {
				ok &= formatToQryBuffer(qry, "true");
				break;
			}
			/* fall-through */
		case JT_STRING:
			escStr = escapeJson(arg->value);
			if (escStr == NULL) goto onOutOfMemory;
			ok &= formatToQryBuffer(qry, "\"%s\"", escStr);
			if (escStr != arg->value) free(escStr);
			break;
		}
		first = 0;
	}
	ok &= formatToQryBuffer(qry, "\n}}\n");
	if (ok != 1) goto onOutOfMemory;
	
	if (fputUtf8N(fd, qry->buffer, qry->length) < 1) {
		if (qry->ctx->verbose > 1) _ftprintf(ferr, MSGT(MSGT_ERR_QUERY_PRINT));
		return 0;
	}
	
	return 1;
onOutOfMemory:
	if (qry->ctx->verbose > 1) _ftprintf(ferr, MSGT(MSGT_ERR_NO_MEM));
	return 0;
}


/**
 * Outputs the query result in XML format.
 * 
 * @param[in,out] fd - output to this file descriptor
 * @param[in,out] qry - query handle
 * @param[in] action - action to output
 * @return 1 on success, else 0
 */
static int trQueryOutputXml(FILE * fd, tTrQueryHandler * qry, const tTrAction * action) {
	if (fd == NULL || qry == NULL || action == NULL) return 0;
	int ok;
	char * escStr;
	
	qry->length = 0;
	ok = formatToQryBuffer(qry, "<%s>\n", action->name);
	for (size_t ar = 0; ar < action->length; ar++) {
		tTrArgument * arg = action->arg + ar;
		if (strcmp(arg->dir, "out") != 0) continue;
		/* start tag */
		ok &= formatToQryBuffer(qry, " <%s>", arg->var);
		/* value */
		if (arg->value != NULL) {
			escStr = p_escapeXml(arg->value, (size_t)-1);
			if (escStr == NULL) goto onOutOfMemory;
			ok &= formatToQryBuffer(qry, "%s", escStr);
			if (escStr != arg->value) free(escStr);
		}
		/* end tag */
		ok &= formatToQryBuffer(qry, "</%s>\n", arg->var);
	}
	ok &= formatToQryBuffer(qry, "</%s>\n", action->name);
	if (ok != 1) goto onOutOfMemory;
	
	if (fputUtf8N(fd, qry->buffer, qry->length) < 1) {
		if (qry->ctx->verbose > 1) _ftprintf(ferr, MSGT(MSGT_ERR_QUERY_PRINT));
		return 0;
	}
		
	return 1;
onOutOfMemory:
	if (qry->ctx->verbose > 1) _ftprintf(ferr, MSGT(MSGT_ERR_NO_MEM));
	return 0;
}


/**
 * Queries a TR-064 SOAP request according to opt and prints the result to fout.
 * 
 * @param[in,out] qry - query handle
 * @param[in] opt - query options
 * @param[in] argIndex - first valid argument index
 * @return 1 on success, else 0
 */
static int trQuery(tTrQueryHandler * qry, const tOptions * opt, int argIndex) {
	static const char * req =
		"POST %s HTTP/1.1\r\n"
		"Host: %s:%s\r\n"
		"Connection: keep-alive\r\n"
		"Accept: */*\r\n"
		"User-Agent: tr64c %s\r\n"
		"%s" /* authorization field goes in here */
		"SOAPAction: %s#%s\r\n"
		"Content-Type: text/xml; charset=utf-8\r\n"
		"Content-Length: %u\r\n"
		"\r\n"
		"%.*s"
	;
	static const char * head =
		"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
		"<s:Envelope s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\" xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\">\n"
		"<s:Body>\n"
	;
	static const char * tail =
		"</s:Body>\n"
		"</s:Envelope>"
	;
	if (qry == NULL || opt == NULL) return 0;
	tTr64RequestCtx * ctx = qry->ctx;
	tTrObject * obj = qry->obj;
	const tTrDevice * device = NULL;
	const tTrService * service = NULL;
	tTrAction * action = NULL;
	const size_t deviceLen  = (opt->device != NULL)  ? strlen(opt->device)  : 0;
	const size_t serviceLen = (opt->service != NULL) ? strlen(opt->service) : 0;
	const size_t actionLen  = (opt->action != NULL)  ? strlen(opt->action)  : 0;
	int res = 0;
	int fmt;
	
	/* select the matching action description */
	if (obj->device == NULL) {
		if (opt->verbose > 1) _ftprintf(ferr, MSGT(MSGT_ERR_NO_DEV_IN_DESC));
		goto onError;
	}
	for (size_t d = 0; d < obj->length; d++) {
		const tTrDevice * dev = obj->device + d;
		if (dev->service == NULL || (opt->device != NULL && strncmp(dev->name, opt->device, deviceLen) != 0)) continue;
		for (size_t s = 0; s < dev->length; s++) {
			const tTrService * srvc = dev->service + s;
			if (srvc->action == NULL || (opt->service != NULL && strncmp(srvc->name, opt->service, serviceLen) != 0)) continue;
			for (size_t ac = 0; ac < srvc->length; ac++) {
				tTrAction * act = srvc->action + ac;
				if (act == NULL || (opt->action != NULL && strncmp(act->name, opt->action, actionLen) != 0)) continue;
				if (action == NULL) {
					device = dev;
					service = srvc;
					action = act;
				} else {
					if (opt->verbose > 0) _ftprintf(ferr, MSGT(MSGT_ERR_OPT_ACTION_AMB));
					goto onError;
				}
			}
		}
	}
	if (service == NULL || action == NULL) {
		if (opt->verbose > 1) _ftprintf(ferr, MSGT(MSGT_ERR_OPT_BAD_ACTION));
		goto onError;
	}
	if (opt->verbose > 3) {
		fuprintf(ferr, MSGU(MSGU_DBG_SELECTED_QUERY), device->name, service->name, action->name);
	}
	
	/* check input parameters and build request SOAP action */
	qry->length = 0;
	fmt = formatToQryBuffer(qry, "%s<u:%s xmlns:u=\"%s\">\n", head, action->name, service->type);
	for (size_t ar = 0; ar < action->length; ar++) {
		tTrArgument * arg = action->arg + ar;
		if (strcmp(arg->dir, "in") != 0) continue;
		int ok = 0;
		for (int i = argIndex; i < opt->argCount; i++) {
			char * sep = strchr(opt->args[i], '=');
			if (sep == NULL) continue;
			*sep = 0;
			/* strncmp does not work to match two strings completely */
			if (strcmp(arg->var, opt->args[i]) == 0) {
				if (ok == 1) {
					if (ctx->verbose > 1) fuprintf(ferr, MSGU(MSGU_ERR_OPT_AMB_IN_ARG), arg->var);
					goto onError;
				}
				ok = 1;
				/* replace value in argument */
				if (arg->value != NULL) free(arg->value);
				arg->value = strdup(sep + 1);
				if (arg->value == NULL) {
					if (ctx->verbose > 1) _ftprintf(ferr, MSGT(MSGT_ERR_NO_MEM));
					goto onError;
				}
				/* XML escape value */
				if (p_escapeXmlVar(&(arg->value)) != 1) {
					if (ctx->verbose > 1) _ftprintf(ferr, MSGT(MSGT_ERR_NO_MEM));
					goto onError;
				}
				/* format argument */
				fmt &= formatToQryBuffer(qry, "<%s>%s</%s>\n", arg->name, arg->value, arg->name);
			}
			*sep = '=';
		}
		if (ok != 1) {
			if (ctx->verbose > 1) fuprintf(ferr, MSGU(MSGU_ERR_OPT_NO_IN_ARG), arg->var);
			goto onError;
		}
	}
	fmt &= formatToQryBuffer(qry, "</u:%s>\n%s", action->name, tail);
	
	if (fmt != 1) {
		if (ctx->verbose > 1) _ftprintf(ferr, MSGT(MSGT_ERR_FMT_QUERY));
		goto onError;
	}
	
onAuthentication:
	/* build HTTP request */
	ctx->length = 0;
	fmt = formatToCtxBuffer(
		ctx,
		req,
		service->control,
		ctx->host,
		ctx->port,
		PROGRAM_VERSION_STR,
		(ctx->auth != NULL) ? ctx->auth : "",
		service->type,
		action->name,
		(unsigned)(qry->length),
		(unsigned)(qry->length),
		qry->buffer
	);
	if (fmt != 1) {
		if (ctx->verbose > 1) _ftprintf(ferr, MSGT(MSGT_ERR_FMT_QUERY));
		goto onError;
	}
	
	/* set method */
	if (ctx->method != NULL) free(ctx->method);
	ctx->method = strdup("POST");
	if (ctx->method == NULL) {
		if (ctx->verbose > 1) _ftprintf(ferr, MSGT(MSGT_ERR_NO_MEM));
		goto onError;
	}
	
	/* set path */
	if (ctx->path != NULL) free(ctx->path);
	ctx->path = strdup(service->control);
	if (ctx->path == NULL) {
		if (ctx->verbose > 1) _ftprintf(ferr, MSGT(MSGT_ERR_NO_MEM));
		goto onError;
	}
	
	/* send HTTP request to server and receive response */
	if (ctx->request(ctx) != 1) {
		if (ctx->status == 401 && ctx->auth != NULL) {
			/* retry with proper authentication */
			goto onAuthentication;
		} else {
			if (ctx->verbose > 1) _ftprintf(ferr, MSGT(MSGT_ERR_GET_QUERY_RESP), (unsigned)(ctx->status));
			goto onError;
		}
	}
	
	/* parse response */
	{
		if (ctx->verbose > 3) _ftprintf(ferr, MSGT(MSGT_DBG_PARSE_QUERY_RESP));
		tPTrQueryRespCtx respCtx = {
			/* .xmlPath      = */ {{0}},
			/* .soapNs       = */ {0},
			/* .userNs       = */ {0},
			/* .service      = */ service,
			/* .action       = */ action,
			/* .content      = */ {0},
			/* .depth        = */ 0,
			/* .lastError    = */ MSGT_SUCCESS
		};
		const size_t contentLength = (ctx->buffer + ctx->length) - ctx->content;
		const char * xmlErrPos = NULL;
		if (p_sax(ctx->content, contentLength, &xmlErrPos, xmlQueryRespVisitor, &respCtx) != PSRT_SUCCESS) {
			if (respCtx.lastError != MSGT_SUCCESS) {
				if (ctx->verbose > 0) _ftprintf(ferr, MSGT(respCtx.lastError));
			} else {
				if (ctx->verbose > 0) _ftprintf(ferr, MSGT(MSGT_ERR_QUERY_RESP_FMT));
				if (ctx->verbose > 3) {
					tParserPos pos;
					if (p_getPos(ctx->content, contentLength, xmlErrPos, 1, &pos) == 1) {
						_ftprintf(ferr, MSGT(MSGT_DBG_BAD_TOKEN), (unsigned)pos.line, (unsigned)pos.column);
					}
				}
			}
			goto onError;
		}
	}
	
	/* output result (possible errors are printed by the called function) */
	if (ctx->verbose > 3) _ftprintf(ferr, MSGT(MSGT_DBG_OUT_QUERY_RESP));
	if (qry->output(fout, qry, action) != 1) goto onError;
	
	res = 1;
onError:
	return res;
}


/**
 * Creates a new TR-064 query handler.
 * Use ctx->resolve() before calling qry->query().
 * 
 * @param[in,out] ctx - use this request context
 * @param[in,out] obj - use this device description
 * @param[in] opt - other options (e.g. output format)
 * @return new handler or NULL on error
 */
tTrQueryHandler * newTrQueryHandler(tTr64RequestCtx * ctx, tTrObject * obj, const tOptions * opt) {
	static int (* writer[])(FILE *, tTrQueryHandler *, const tTrAction *) = {
		trQueryOutputCsv,
		trQueryOutputJson,
		trQueryOutputXml
	};
	if (ctx == NULL || obj == NULL || opt == NULL) return NULL;
	tTrQueryHandler * qry = NULL;
	tTrQueryHandler * res = NULL;
	
	qry = (tTrQueryHandler *)malloc(sizeof(tTrQueryHandler));
	if (qry == NULL) return NULL;
	
	qry->ctx = ctx;
	qry->obj = obj;
	qry->query = trQuery;
	qry->output = writer[opt->format];
	if (arrayFieldInit(qry, buffer, BUFFER_SIZE) != 1) {
		if (ctx->verbose > 0) _ftprintf(ferr, MSGT(MSGT_ERR_NO_MEM));
		goto onError;
	}
	
	res = qry;
onError:
	if (res == NULL && qry != NULL) freeTrQueryHandler(qry);
	return res;
}


/**
 * Frees the given query handler. The handler is invalid after this call.
 * 
 * @param[in,out] qry - query handler
 */
void freeTrQueryHandler(tTrQueryHandler * qry) {
	if (qry == NULL) return;
	if (qry->buffer != NULL) free(qry->buffer);
	free(qry);
}


/**
 * Helper function for printDiscoveredDevices() to parse received TR-064 device response.
 * 
 * @param[in] type - token type
 * @param[in] tokens - tokens passed (1 for body; 2 parameter)
 * @param[in,out] param - user defined callback data (expects tPToken[3])
 * @return 1 to continue
 */
static int parseDiscoveryDevice(const tPHttpTokenType type, const tPToken * tokens, void * param) {
	tPToken * outTokens = (tPToken *)param;
	if (tokens == NULL || outTokens == NULL) return 0;
	if (type == PHTT_REQUEST) return 0; /* invalid */
	if (type == PHTT_STATUS && p_cmpToken(tokens + 1, "200") != 0) return 0; /* invalid */
	if (type != PHTT_PARAMETER) return 1;
	if (p_cmpTokenI(tokens, "ST") == 0) {
		if (tokens[1].length > 0) outTokens[0] = tokens[1];
	} else if (p_cmpTokenI(tokens, "SERVER") == 0) {
		if (tokens[1].length > 0) outTokens[1] = tokens[1];
	} else if (p_cmpTokenI(tokens, "LOCATION") == 0) {
		if (tokens[1].length > 0) outTokens[2] = tokens[1];
	}
	return 1;
}


/**
 * Helper function for handleScan() to print out the discovered TR-064 devices.
 * 
 * @param[in] buffer - response message
 * @param[in] length - length of buffer
 * @param[in,out] param - user defined callback data (expects tTr64RequestCtx)
 * @return 1 to continue
 */
static int printDiscoveredDevices(const char * buffer, const size_t length, void * param) {
	static const char * st = "urn:dslforum-org:device:InternetGatewayDevice:1";
	tTr64RequestCtx * ctx = (tTr64RequestCtx *)param;
	tPToken tokens[3] = {0}; /* ST, SERVER, LOCATION */
	if (buffer == NULL || ctx == NULL) return 0;
	switch (p_http(buffer, length, NULL, parseDiscoveryDevice, tokens)) {
	case PHRT_SUCCESS:
		if (tokens[0].start != NULL && tokens[1].start != NULL && tokens[2].start != NULL && p_cmpToken(tokens, st) == 0) {
			/* print valid response */
			fuprintf(ferr, "Device: %.*s\nURL:    %.*s\n", (unsigned)(tokens[1].length), tokens[1].start, (unsigned)(tokens[2].length), tokens[2].start);
		}
		break;
	case PHRT_UNEXPECTED_END:
		/* incomplete response -> ignore */
		break;
	default:
		/* wrong response -> ignore */
		break;
	}
	return 1;
}


/**
 * Parses the given interactive command-line and sets the arguments in the passed option structure.
 * Any previous option argument list will be replaced. Fields are split by blanks. Special
 * characters can be escaped. The following escape sequences are valid and should be used to escape
 * those characters within the fields:
 * - \\   escapes \
 * - \n   escapes line-feed
 * - \r   escapes carrier-return
 * - \t   escapes tabulator
 * - \"   escapes quotes
 * - \'   escapes apostrophe
 * - \    escapes space
 * - \xXX escapes any byte using the given 2 character hex value
 * 
 * @param[in,out] line - UTF-8 input line to parse (gets modified by the parser)
 * @param[in,out] opt - replaces the args and argCount fields
 * @return 1 on success, else 0
 * @remarks The unescaped and split string is definitely shorter or at least equal to the input line
 * which is why this function can operate on it to avoid unnecessary allocations.
 */
static int iParseCmdLineToOpts(char * line, tOptions * opt) {
	if (line == NULL || opt == NULL) return 0;
	char * start = NULL; /* start of field (excluding quotes) */
	char * out = line;
	char * ptr = line;
	char quote = 0;
	int res = 0;
	
	/* free previous argument list */
	if (opt->args != NULL) {
		for (int i = 0; i < opt->argCount; i++) {
			if (opt->args[i] != NULL) free(opt->args[i]);
		}
		free(opt->args);
		opt->args = NULL;
		opt->argCount = 0;
	}
	
	/* parse input line, unescape and split by fields */
	while (*ptr != 0 && *ptr != '\n' && *ptr != '\r') {
		if (start != NULL) {
			/* within field */
			if (*ptr == '\\') {
				/* escape sequence */
				switch (ptr[1]) {
				case '\\': *out++ = '\\'; ptr++; break;
				case 'n':  *out++ = '\n'; ptr++; break;
				case 'r':  *out++ = '\r'; ptr++; break;
				case 't':  *out++ = '\t'; ptr++; break;
				case '"':  *out++ = '"';  ptr++; break;
				case '\'': *out++ = '\''; ptr++; break;
				case ' ':  *out++ = ' ';  ptr++; break;
				case 'x':
					if (isxdigit(ptr[2]) != 0 && isxdigit(ptr[3]) != 0) {
						const int b1 = toupper(ptr[2]) - '0';
						const int b2 = toupper(ptr[3]) - '0';
						const int b = (((b1 > 16) ? (b1 - 7) : b1) << 4) | ((b2 > 16) ? (b2 - 7) : b2);
						if (b > 0) {
							*out++ = (char)b;
							ptr += 3;
							break;
						}
					}
					/* fall-through */
				default:
					/* invalid escape sequence */
					if (opt->verbose > 1) _ftprintf(ferr, MSGT(MSGT_WARN_CMD_BAD_ESC), (unsigned)(ptr - line + 1));
					goto onError;
				}
			} else if (quote == 0 && (*ptr == '"' || *ptr == '\'')) {
				/* start of quoted part within the field => quotes are ignored */
				quote = *ptr;
			} else if (*ptr == quote) {
				/* end of quoted part */
				quote = 0;
			} else if (quote == 0 && isblank(*ptr) != 0) {
				/* field separator outside quoted part */
				start = NULL;
			} else {
				/* normal field character */
				*out++ = *ptr;
			}
		} else {
			/* between fields */
			if (isblank(*ptr) == 0) {
				/* start of field */
				start = ptr;
				if (out != line && *out != 0) {
					opt->argCount++;
					*out++ = 0; /* null-terminate previous field */
				}
				continue; /* re-parse */
			}
		}
		ptr++;
	}
	if (out != line && *out != 0) {
		/* end of last field */
		opt->argCount++;
		*out++ = 0; /* null-terminate previous field */
	}
	
	/* create new argument list from normalized input line */
	opt->args = (char **)calloc(1, sizeof(*opt->args) * opt->argCount);
	if (opt->args == NULL) {
		if (opt->verbose > 0) _ftprintf(ferr, MSGT(MSGT_ERR_NO_MEM));
		goto onError;
	}
	ptr = line;
	for (int i = 0; i < opt->argCount; i++) {
		opt->args[i] = strdup(ptr);
		if (opt->args[i] == NULL) {
			if (opt->verbose > 0) _ftprintf(ferr, MSGT(MSGT_ERR_NO_MEM));
			goto onError;
		}
		ptr += strlen(opt->args[i]) + 1;
	}
	
	res = 1;
onError:
	return res;
}


/**
 * Outputs the help for the interactive mode.
 */
void iPrintHelp(void) {
	_tprintf(
	_T("exit\n")
	_T("      Terminates the interactive mode.\n")
	_T("help\n")
	_T("      Print short usage instruction.\n")
	_T("list\n")
	_T("      List services and actions available on the device.\n")
	_T("query [device/]service/action [<variable=value> ...]\n")
	_T("      Query the given action and output its response.\n")
	);
}


/**
 * Process a single TR-064 query.
 * 
 * @param[in] opt - given options
 * @return 1 on success, else 0
 */
int handleQuery(tOptions * opt) {
	if (opt->mode != M_QUERY) return 0;
	tTr64RequestCtx * ctx = NULL;
	tTrObject * obj = NULL;
	tTrQueryHandler * qry = NULL;
	int res = 0;
	
	if (opt->service == NULL) {
		if (opt->verbose > 1) _ftprintf(ferr, MSGT(MSGT_ERR_OPT_NO_SERVICE));
		goto onError;
	}
	if (opt->action == NULL) {
		if (opt->verbose > 1) _ftprintf(ferr, MSGT(MSGT_ERR_OPT_NO_ACTION));
		goto onError;
	}
	
	ctx = newTr64Request(opt->url, opt->user, opt->pass, opt->timeout, opt->verbose);
	if (ctx == NULL) goto onError;
	if (ctx->resolve(ctx) != 1) goto onError;
	obj = newTrObject(ctx, opt);
	if (obj == NULL) goto onError;
	qry = newTrQueryHandler(ctx, obj, opt);
	if (qry == NULL) goto onError;
	if (qry->query(qry, opt, 1) != 1) goto onError;
	
	res = 1;
onError:
	if (qry != NULL) freeTrQueryHandler(qry);
	if (obj != NULL) freeTrObject(obj);
	if (ctx != NULL) freeTr64Request(ctx);
	return res;
}


/**
 * Scan for available TR-064 compliant devices by performing a simple service discovery.
 * 
 * @param[in] opt - given options
 * @return 1 on success, else 0
 * @see https://en.wikipedia.org/wiki/Simple_Service_Discovery_Protocol
 */
int handleScan(tOptions * opt) {
	static const char * request =
		"M-SEARCH * HTTP/1.1\r\n"
		"HOST: %s:%s\r\n"
		"MAN: \"ssdp:discover\"\r\n"
		"MX: %i\r\n"
		"ST: urn:dslforum-org:device:InternetGatewayDevice:1\r\n"
		"\r\n"
	;
	int res = 0;
	if (opt->mode != M_SCAN) return res;
	if (opt->url == NULL) {
		if (opt->verbose > 0) _ftprintf(ferr, MSGT(MSGT_ERR_OPT_NO_SSDP_ADDR));
		return res;
	}
	if (opt->timeout < 1000 && opt->verbose > 1) {
		_ftprintf(ferr, MSGT(MSGT_WARN_OPT_LOW_TIMEOUT));
	}
	tTr64RequestCtx * ctx = newTr64Request("239.255.255.250:1900", NULL, NULL, opt->timeout, opt->verbose);
	if (ctx == NULL) goto onError;
	if (formatToCtxBuffer(ctx, request, ctx->host, ctx->port, (int)PCF_MAX(1, PCF_MIN(5, (ctx->timeout / 1000) - 1))) != 1) {
		if (opt->verbose > 0) _ftprintf(ferr, MSGT(MSGT_ERR_FMT_SSDP));
		goto onError;
	}
	
	if (ctx->discover(ctx, opt->url, printDiscoveredDevices, ctx) != 1) goto onError;
	
	res = 1;
onError:
	if (ctx != NULL) freeTr64Request(ctx);
	return res;
}


/**
 * Outputs the possible actions from the given parameters.
 * 
 * @param[in,out] ctx - use this request context to build the output
 * @param[in] obj - use this device description
 * @return 1 on success, else 0
 */
static int lOutputList(tTr64RequestCtx * ctx, const tTrObject * obj) {
	if (ctx == NULL || obj == NULL || obj->name == NULL || obj->device == NULL) return 0;
	int ok = 1;
	
	ok &= formatToCtxBuffer(ctx, "Object: %s\n", obj->name);
	for (size_t d = 0; d < obj->length; d++) {
		const tTrDevice * device = obj->device + d;
		ok &= formatToCtxBuffer(ctx, "  Device: %s\n", device->name);
		if (device->service == NULL) continue;
		for (size_t s = 0; s < device->length; s++) {
			const tTrService * service = device->service + s;
			ok &= formatToCtxBuffer(ctx, "    Service: %s\n", service->name);
			if (service->action == NULL) continue;
			for (size_t ac = 0; ac < service->length; ac++) {
				const tTrAction * action = service->action + ac;
				ok &= formatToCtxBuffer(ctx, "      Action: %s\n", action->name);
				if (action->arg == NULL) continue;
				for (size_t ar = 0; ar < action->length; ar++) {
					const tTrArgument * arg = action->arg + ar;
					ok &= formatToCtxBuffer(ctx, "        [%s] %s : %s\n", arg->dir, arg->var, arg->type);
				}
			}
		}
	}
	if (ok == 1) {
		fputUtf8N(fout, ctx->buffer, ctx->length);
		return 1;
	}
	
	if (ctx->verbose > 1) _ftprintf(ferr, MSGT(MSGT_WARN_LIST_NO_MEM));
	return 0;
}


/**
 * List available actions.
 * 
 * @param[in] opt - given options
 * @return 1 on success, else 0
 */
int handleList(tOptions * opt) {
	if (opt->mode != M_LIST) return 0;
	tTr64RequestCtx * ctx = NULL;
	tTrObject * obj = NULL;
	int res = 0;
	ctx = newTr64Request(opt->url, opt->user, opt->pass, opt->timeout, opt->verbose);
	if (ctx == NULL) goto onError;
	if (ctx->resolve(ctx) != 1) goto onError;
	obj = newTrObject(ctx, opt);
	if (obj == NULL) goto onError;
	
	ctx->length = 0;
	if (obj->device == NULL) {
		if (opt->verbose > 1) _ftprintf(ferr, MSGT(MSGT_ERR_NO_DEV_IN_DESC));
		goto onError;
	}
	
	res = lOutputList(ctx, obj);
onError:
	if (ctx != NULL) freeTr64Request(ctx);
	if (obj != NULL) freeTrObject(obj);
	return res;
}


/**
 * Enter interactive query mode.
 * 
 * @param[in,out] opt - given options
 * @return 1 on success, else 0
 */
int handleInteractive(tOptions * opt) {
	if (opt->mode != M_INTERACTIVE) return 0;
	tTr64RequestCtx * ctx = NULL;
	tTrObject * obj = NULL;
	tTrQueryHandler * qry = NULL;
	tReadLineBuf line[1] = {0};
	int len, res = 0;
	
	ctx = newTr64Request(opt->url, opt->user, opt->pass, opt->timeout, opt->verbose);
	if (ctx == NULL) goto onError;
	if (ctx->resolve(ctx) != 1) goto onError;
	obj = newTrObject(ctx, opt);
	if (obj == NULL) goto onError;
	qry = newTrQueryHandler(ctx, obj, opt);
	if (qry == NULL) goto onError;
	
	while (signalReceived == 0) {
#ifdef UNICODE
		len = getLineUtf8(line, fin, opt->narrow);
#else /* UNICODE */
		len = getLineUtf8(line, fin, 1);
#endif /* UNICODE */
		if (len < 0) goto onError;
		if (len == 0 || line->str[0] == '\n') continue;
		if (iParseCmdLineToOpts(line->str, opt) != 1) continue; /* errors are output by the called function */
		if (opt->argCount <= 0) continue;
		for (char * ch = opt->args[0]; *ch != 0; ch++) *ch = toupper(*ch);
		if (strcmp(opt->args[0], "?") == 0 || strncmp(opt->args[0], "HELP", strlen(opt->args[0])) == 0) {
			iPrintHelp();
		} else if (feof(fin) || strncmp(opt->args[0], "EXIT", strlen(opt->args[0])) == 0) {
			break;
		} else if (strncmp(opt->args[0], "LIST", strlen(opt->args[0])) == 0) {
			lOutputList(ctx, obj);
		} else if (strncmp(opt->args[0], "QUERY", strlen(opt->args[0])) == 0) {
			if (opt->argCount < 2) {
				if (opt->verbose > 1) _ftprintf(ferr, MSGT(MSGT_WARN_BAD_CMD));
				continue;
			}
			/* perform query */
			if (parseActionPath(opt, 1) != 1) {
				if (opt->verbose > 0) _ftprintf(ferr, MSGT(MSGT_ERR_OPT_NO_ACTION));
				continue;
			}
			qry->query(qry, opt, 2);
		} else {
			if (opt->verbose > 1) _ftprintf(ferr, MSGT(MSGT_WARN_BAD_CMD));
		}
		fflush(fout);
	}
	
	res = 1;
onError:
	freeGetLine(line);
	if (qry != NULL) freeTrQueryHandler(qry);
	if (obj != NULL) freeTrObject(obj);
	if (ctx != NULL) freeTr64Request(ctx);
	return res;
}
