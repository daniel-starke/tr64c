/**
 * @file http.c
 * @author Daniel Starke
 * @see parser.h
 * @date 2018-07-05
 * @version 2018-08-09
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
#include <ctype.h>
#include <stddef.h>
#include <string.h>
#include "parser.h"


#ifdef PCF_P_HTTP_DEBUG
#include <stdio.h>
#include <stdlib.h>
#endif /* PCF_P_HTTP_DEBUG */


#if defined(_MSC_VER) && _MSC_VER <= 1600
#define isblank(x) ((x) == '\t' || (x) == ' ')
#endif


/**
 * UTF-8 based HTTP parser. The input HTTP header and body is tokenized and each token is passed to
 * the given callback function. The string tokens passed to the callback point into the HTTP string
 * passed to this function. The arguments of HTTP are not checked against validity.
 * 
 * @param[in] http - input HTTP
 * @param[in] length - length of HTTP in bytes
 * @param[out] errorPos - error position (optional)
 * @param[in] visitor - user defined callback function
 * @param[in] param - user defined parameter (passed to callback function)
 * @return see tPHttpReturnType
 * @see https://tools.ietf.org/html/rfc7230
 * @see http://www.iana.org/assignments/message-headers/message-headers.xhtml
 * @remarks The input HTTP is not a string but a char buffer with a defined size.
 */
tPHttpReturnType p_http(const char * http, const size_t length, const char ** errorPos, const PHttpTokenVisitor visitor, void * param) {
	if (http == NULL || visitor == NULL) return PHRT_INVALID_ARGUMENT;
	typedef enum {
		HTTP_START,                 /* start of the HTTP message; all other flags are unset */
		HTTP_WITHIN_METHOD,         /* within the method field of a request line */
		HTTP_WITHIN_TARGET,         /* within the target field of a request line */
		HTTP_WITHIN_VERSION,        /* within the version field of a request or status line */
		HTTP_WITHIN_STATUS,         /* within the status field of a status line */
		HTTP_WITHIN_REASON,         /* within the reason field of a status line */
		HTTP_WITHIN_FIELD,          /* within a parameter field name */
		HTTP_WITHIN_VALUE,          /* within a parameter value */
		HTTP_WITHIN_CONTENT_LENGTH  /* within the Content-Length parameter value */
	} tHttpStates;
	const char * ptr = http;
	const char * lastNonSpace = NULL;
	tPToken tokens[3] = { 0 };
	tHttpStates state = HTTP_START;
	long contentLength = -1;
	tPHttpReturnType error = PHRT_UNEXPECTED_END;
#define VISIT(x) \
	do { \
		if (visitor(PHTT_##x, tokens, param) == 0) { \
			if (errorPos != NULL) *errorPos = tokens->start; \
			return PHRT_ABORT; \
		} \
	} while (0)
#define ONERROR(x) do {error = PHRT_##x; goto onError;} while ( 0 )
	for (size_t n = 0; n < length && *ptr != 0; n++, ptr++) {
#ifdef PCF_P_HTTP_DEBUG
		const char * stateStr[] = {"START", "METHOD", "TARGET", "VERSION", "STATUS", "REASON", "FIELD", "VALUE", "CONTENT_LENGTH"};
		fprintf(stderr, "char =");
		if (isprint(*ptr) != 0) {
			fprintf(stderr, " '%c'", *ptr);
		} else {
			fprintf(stderr, "    ");
		}
		fprintf(stderr, " 0x%02X, n = %3u, state = %s\n", *ptr, (unsigned)n, stateStr[state]);
		if (*ptr != http[n]) {
			fprintf(stderr, "Error: n position does not match ptr\n");
			abort();
		}
		fflush(stderr);
#endif /* PCF_P_HTTP_DEBUG */
		if (isspace(*ptr) == 0 && *ptr < 0x20) ONERROR(UNEXPECTED_CHARACTER); /* this excludes also negative values */
		switch (state) {
		case HTTP_START:
			if (p_isHttpTChar(*ptr) != 0) {
				state = HTTP_WITHIN_METHOD;
				lastNonSpace = NULL;
				tokens[0].start = ptr;
				tokens[0].length = 1;
				tokens[1].start = NULL;
				tokens[1].length = 0;
				tokens[2].start = NULL;
				tokens[2].length = 0;
			} else {
				ONERROR(UNEXPECTED_CHARACTER);
			}
			break;
		case HTTP_WITHIN_METHOD:
			if (p_isHttpTChar(*ptr) != 0) {
				tokens[0].length++;
			} else if (*ptr == ' ') {
				if (tokens[0].length == 0) ONERROR(UNEXPECTED_CHARACTER);
				state = HTTP_WITHIN_TARGET;
				tokens[1].start = ptr + 1;
				tokens[1].length = 0;
			} else if (*ptr == '/' && tokens[0].length == 4
				&& toupper(tokens[0].start[0]) == 'H'
				&& toupper(tokens[0].start[1]) == 'T'
				&& toupper(tokens[0].start[2]) == 'T'
				&& toupper(tokens[0].start[3]) == 'P') {
				state = HTTP_WITHIN_VERSION;
				tokens[0].length++;
			} else {
				ONERROR(UNEXPECTED_CHARACTER);
			}
			break;
		case HTTP_WITHIN_TARGET:
			if (*ptr == ' ') {
				if (tokens[1].length == 0) ONERROR(UNEXPECTED_CHARACTER);
				state = HTTP_WITHIN_VERSION;
				tokens[2].start = ptr + 1;
				tokens[2].length = 0;
			} else if (isspace(*ptr) == 0) {
				tokens[1].length++;
			} else {
				ONERROR(UNEXPECTED_CHARACTER);
			}
			break;
		case HTTP_WITHIN_VERSION:
			if (*ptr == ' ') {
				if (tokens[1].start == NULL) {
					/* status-line */
					if (tokens[0].length == 0) ONERROR(UNEXPECTED_CHARACTER);
					state = HTTP_WITHIN_STATUS;
					tokens[1].start = ptr + 1;
					tokens[1].length = 0;
				} else {
					/* request-line */
					ONERROR(UNEXPECTED_CHARACTER);
				}
			} else if (*ptr == '\r' && (n + 1) < length && ptr[1] == '\n') {
				if (tokens[2].start == NULL) ONERROR(UNEXPECTED_CHARACTER); /* status-line */
				/* request-line complete */
				if (tokens[2].length == 0) ONERROR(UNEXPECTED_CHARACTER);
				state = HTTP_WITHIN_FIELD;
				VISIT(REQUEST);
				tokens[0].start = ptr + 2;
				tokens[0].length = 0;
				tokens[1].start = NULL;
				tokens[1].length = 0;
				ptr++;
				n++;
			} else if (p_isHttpTChar(*ptr) != 0 || *ptr == '/') {
				if (tokens[2].start == NULL) {
					/* status-line */
					tokens[0].length++;
				} else {
					/* request-line */
					tokens[2].length++;
				}
			} else {
				ONERROR(UNEXPECTED_CHARACTER);
			}
			break;
		case HTTP_WITHIN_STATUS:
			if (isdigit(*ptr) != 0) {
				tokens[1].length++;
			} else if (*ptr == ' ') {
				if (tokens[1].length == 0) ONERROR(UNEXPECTED_CHARACTER);
				state = HTTP_WITHIN_REASON;
				tokens[2].start = ptr + 1;
				tokens[2].length = 0;
			} else {
				ONERROR(UNEXPECTED_CHARACTER);
			}
			break;
		case HTTP_WITHIN_REASON:
			if (*ptr == '\r' && (n + 1) < length && ptr[1] == '\n') {
				/* status-line complete */
				state = HTTP_WITHIN_FIELD;
				VISIT(STATUS);
				tokens[0].start = ptr + 2;
				tokens[0].length = 0;
				tokens[1].start = NULL;
				tokens[1].length = 0;
				ptr++;
				n++;
			} else {
				tokens[2].length++;
			}
			break;
		case HTTP_WITHIN_FIELD:
			if (p_isHttpTChar(*ptr) != 0) {
				tokens[0].length++;
			} else if (*ptr == ':') {
				if (p_cmpTokenI(tokens, "Content-Length") == 0) {
					state = HTTP_WITHIN_CONTENT_LENGTH;
					if (contentLength != -1) {
						ptr = tokens[0].start;
						ONERROR(UNEXPECTED_CHARACTER);
					}
				} else {
					state = HTTP_WITHIN_VALUE;
				}
				tokens[1].start = NULL;
				tokens[1].length = 0;
			} else if (tokens[0].length == 0 && *ptr == '\r' && (n + 1) < length && ptr[1] == '\n') {
				/* start of body */
				if (contentLength >= 0) {
					tokens[0].start = http;
					tokens[0].length = (size_t)contentLength + n + 2;
					VISIT(EXPECTED);
				}
				tokens[0].start = ptr + 2;
				tokens[0].length = (size_t)(length - (n + 2));
				if (contentLength > 0) {
					if (tokens[0].length < (size_t)contentLength) {
						if (errorPos != NULL) *errorPos = tokens[0].start + tokens[0].length;
						return PHRT_UNEXPECTED_END;
					}
					tokens[0].length = (size_t)contentLength;
					/* body complete */
					VISIT(BODY);
				} else if (tokens[0].length > 0) {
					/* body complete */
					VISIT(BODY);
				}
				return PHRT_SUCCESS;
			} else {
				ONERROR(UNEXPECTED_CHARACTER);
			}
			break;
		case HTTP_WITHIN_VALUE:
			if (*ptr == '\r' && (n + 1) < length && ptr[1] == '\n') {
onParameterSetComplete:
				if (tokens[1].start == NULL) {
					tokens[1].start = ptr;
					lastNonSpace = tokens[1].start;
				} else {
					lastNonSpace++;
				}
				/* parameter set complete */
				state = HTTP_WITHIN_FIELD;
				tokens[1].length = (size_t)(lastNonSpace - tokens[1].start);
				VISIT(PARAMETER);
				tokens[0].start = ptr + 2;
				tokens[0].length = 0;
				tokens[1].start = NULL;
				tokens[1].length = 0;
				ptr++;
				n++;
			} else if (tokens[1].start == NULL && isblank(*ptr) == 0) {
				tokens[1].start = ptr;
				lastNonSpace = ptr;
			} else if (tokens[1].start != NULL) {
				if (*ptr != '\r' && *ptr != '\n') {
					if (isblank(*ptr) == 0) lastNonSpace = ptr;
				} else if (isblank(*ptr) == 0) {
					ONERROR(UNEXPECTED_CHARACTER);
				}
			}
			break;
		case HTTP_WITHIN_CONTENT_LENGTH:
			if (tokens[1].start == NULL) {
				if (isdigit(*ptr) != 0) {
					tokens[1].start = ptr;
					contentLength = *ptr - '0';
					lastNonSpace = ptr;
				} else if (isblank(*ptr) == 0) {
					ONERROR(INVALID_CONTENT_LENGTH);
				}
			} else if (tokens[1].start != NULL) {
				if (isdigit(*ptr) != 0) {
					if ((lastNonSpace + 1) != ptr) {
						ptr--;
						ONERROR(INVALID_CONTENT_LENGTH);
					}
					const long oldContentLength = contentLength;
					contentLength = (contentLength * 10) + (*ptr - '0');
					if (contentLength < oldContentLength) {
						/* number overflow */
						ONERROR(INVALID_CONTENT_LENGTH);
					}
					lastNonSpace = ptr;
				} else if (*ptr == '\r' && (n + 1) < length && ptr[1] == '\n') {
					goto onParameterSetComplete;
				} else if (isblank(*ptr) == 0) {
					ONERROR(INVALID_CONTENT_LENGTH);
				}
			}
			break;
		}
	}
#undef VISIT
#undef ONERROR
onError:
	if (errorPos != NULL) *errorPos = ptr;
	return error;
}
