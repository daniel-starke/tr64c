/**
 * @file url.c
 * @author Daniel Starke
 * @see parser.h
 * @date 2018-07-01
 * @version 2018-07-08
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
#include <stddef.h>
#include <string.h>
#include "parser.h"


#ifdef PCF_P_URL_DEBUG
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#endif /* PCF_P_URL_DEBUG */


/**
 * Helper callback function used by p_urlTokens().
 * 
 * @param[in] type - token type
 * @param[in] tokens - tokens passed (1 for body; 2 parameter)
 * @param[in] param - user defined callback data
 * @return 1 to continue
 */
static int p_urlTokensVisitor(const tPUrlTokenType type, const tPToken * token, void * param) {
	tPToken * output = (tPToken *)param;
	output[type] = *token;
	return 1;
}


/**
 * UTF-8 based URL parser. The input URL is tokenized and each token is passed to the given callback
 * function. The string tokens passed to the callback point into the URL string passed to this
 * function. The parts of the URL are not checked against validity. This needs to be done after
 * resolving the escape sequences. Therefore, escaping is preserved.
 * 
 * @param[in] url - input URL
 * @param[in] length - length of URL in bytes
 * @param[in] visitor - user defined callback function
 * @param[in] param - user defined parameter (passed to callback function)
 * @return see tPUrlReturnType
 * @see https://tools.ietf.org/html/rfc3986
 */
tPUrlReturnType p_url(const char * url, const size_t length, const PUrlTokenVisitor visitor, void * param) {
	if (url == NULL || visitor == NULL) return PURT_INVALID_ARGUMENT;
	const char * ptr = url;
	const char * protocolSep = NULL; /* first occurrence of '://' */
	const char * userSep = NULL;     /* first occurrence of ':' before passSep */
	const char * passSep = NULL;     /* last occurrence of '@' before fileSep */
	const char * portSep = NULL;     /* last occurrence of ':' before fileSep */
	const char * pathSep = NULL;     /* first occurrence of fileSep, searchSep or hashSep */
	const char * fileSep = NULL;     /* first occurrence of '/' which is not part of '://' */
	const char * searchSep = NULL;   /* first occurrence of '?' after fileSep or port */
	const char * hashSep = NULL;     /* first occurrence of '#' after fileSep or port */
	tPToken token = {ptr, 0};
#define VISIT(x) if (visitor(PUTT_##x, &token, param) == 0) return PURT_ABORT;
	for (size_t n = 0; n < length && *ptr != 0; ) {
#ifdef PCF_P_URL_DEBUG
		fprintf(stderr, "char =");
		if (isprint(*ptr) != 0) {
			fprintf(stderr, " '%c'", *ptr);
		} else {
			fprintf(stderr, "    ");
		}
		fprintf(stderr, " 0x%02X", *ptr);
		fprintf(stderr, ", seps = ://%i:%i@%i:%i/%i?%i#%i, pathSep = %i\n",
			protocolSep != NULL ? 1 : 0,
			userSep     != NULL ? 1 : 0,
			passSep     != NULL ? 1 : 0,
			portSep     != NULL ? 1 : 0,
			fileSep     != NULL ? 1 : 0,
			searchSep   != NULL ? 1 : 0,
			hashSep   != NULL ? 1 : 0,
			pathSep     != NULL ? 1 : 0
		);
		if (*ptr != url[n]) {
			fprintf(stderr, "Error: n position does not match ptr\n");
			abort();
		}
		fflush(stderr);
#endif
		switch (*ptr) {
		case ':':
			if (pathSep == NULL) {
				if (protocolSep == NULL && (n + 2) < length && ptr[1] == '/' && ptr[2] == '/') {
					token.length = (size_t)(ptr - token.start);
					VISIT(PROTOCOL);
					protocolSep = ptr;
					ptr += 2;
					n += 2;
					token.start = ptr + 1;
				} else if (userSep == NULL && passSep == NULL && fileSep == NULL) {
					userSep = ptr;
				} else if (portSep == NULL && fileSep == NULL) {
					if (userSep != NULL) {
						if (passSep != NULL) portSep = ptr;
					} else {
						portSep = ptr;
					}
				}
			}
			break;
		case '@':
			if (pathSep == NULL) {
				portSep = NULL;
				passSep = ptr;
			}
			break;
		case '/':
			if (fileSep == NULL && searchSep == NULL && hashSep == NULL) {
				fileSep = ptr;
				pathSep = fileSep;
			}
			break;
		case '?':
			if (searchSep == NULL && hashSep == NULL) {
				searchSep = ptr;
				if (pathSep == NULL) pathSep = searchSep;
			}
			break;
		case '#':
			if (hashSep == NULL) {
				hashSep = ptr;
				if (pathSep == NULL) pathSep = hashSep;
			}
			break;
		default:
			break;
		}
		n++;
		ptr++;
	}
	if (passSep == NULL && userSep != NULL) {
		portSep = userSep;
		userSep = NULL;
	}
	if (pathSep == NULL) pathSep = ptr;
	/* user:pass@ */
	if (passSep != NULL) {
		if (userSep != NULL) {
			token.length = (size_t)(userSep - token.start);
			VISIT(USER);
			token.start = userSep + 1;
			token.length = (size_t)(passSep - token.start);
			VISIT(PASS);
		} else {
			token.length = (size_t)(passSep - token.start);
			VISIT(USER);
		}
		token.start = passSep + 1;
	}
	/* host:port */
	if (portSep != NULL) {
		token.length = (size_t)(portSep - token.start);
		VISIT(HOST);
		token.start = portSep + 1;
		token.length = (size_t)(pathSep - token.start);
		VISIT(PORT);
	} else {
		token.length = (size_t)(pathSep - token.start);
		if (token.length > 0 || protocolSep != NULL || userSep != NULL || passSep != NULL || portSep != NULL) {
			/* parse if not only a path was given */
			VISIT(HOST);
		}
	}
	/* /path?search#hash */
	if (fileSep != NULL) {
		const char * end = (searchSep != NULL) ? searchSep : (hashSep != NULL) ? hashSep : ptr;
		token.start = fileSep + 1;
		token.length = (size_t)(end - token.start);
		VISIT(PATH);
		token.start = end + 1;
	}
	/* ?search#hash */
	if (searchSep != NULL) {
		const char * end = (hashSep != NULL) ? hashSep : ptr;
		token.start = searchSep + 1;
		token.length = (size_t)(end - token.start);
		VISIT(SEARCH);
		token.start = end + 1;
	}
	/* #hash */
	if (hashSep != NULL) {
		token.start = hashSep + 1;
		token.length = (size_t)(ptr - token.start);
		VISIT(HASH);
	}
#undef VISIT
	return PURT_SUCCESS;
}


/**
 * Invokes p_url() and fills the results in the given output variable which needs to be able to hold
 * at least as many elements as defined by tPUrlTokenType. Each element of output can be addressed
 * by the enumeration value defined in tPUrlTokenType.
 * 
 * @param[in] url - input URL
 * @param[in] length - length of URL in bytes
 * @param[out] output - write resulting token to this array
 * @see p_url()
 */
tPUrlReturnType p_urlTokens(const char * str, const size_t length, tPToken * output) {
	memset(output, 0, sizeof(tPToken) * 8);
	return p_url(str, length, p_urlTokensVisitor, output);
}
