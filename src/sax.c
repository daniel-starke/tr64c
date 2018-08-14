/**
 * @file sax.c
 * @author Daniel Starke
 * @see parser.h
 * @date 2018-06-23
 * @version 2018-07-25
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


#ifdef PCF_P_SAX_DEBUG
#include <stdio.h>
#include <stdlib.h>
#endif /* PCF_P_SAX_DEBUG */


/**
 * UTF-8 based Simple API for XML parser. The input XML is tokenized and each token is passed to the
 * given callback function. The string tokens passed to the callback point into the XML string passed
 * to this function. Leading and trailing spaces are stripped from the tag contents. Attribute
 * values are passed without quoting. The function does not check if the XML document is well-formed
 * but it expects it to be so. Only XML documents are supported (see XML technical report chapter 2).
 * Escaping is preserved.
 * 
 * @param[in] xml - input XML
 * @param[in] length - length of XML in bytes
 * @param[out] errorPos - error position (optional)
 * @param[in] visitor - user defined callback function
 * @param[in] param - user defined parameter (passed to callback function)
 * @return see tPSaxReturnType
 * @see https://www.w3.org/TR/xml/
 */
tPSaxReturnType p_sax(const char * xml, const size_t length, const char ** errorPos, const PSaxTokenVisitor visitor, void * param) {
	typedef enum {
		SAX_START                  = 0x0000, /* start of the XML document; all other flags are unset */
		SAX_WITHIN_XML             = 0x0001, /* XML specific processing instruction */
		SAX_WITHIN_INSTRUCTION     = 0x0002, /* processing instruction */
		SAX_WITHIN_COMMENT         = 0x0004, /* within a comment */
		SAX_WITHIN_START_TAG       = 0x0008, /* within an start tag */
		SAX_WITHIN_END_TAG         = 0x0010, /* within an end tag */
		SAX_WITHIN_TAG             = SAX_WITHIN_START_TAG | SAX_WITHIN_END_TAG, /* within a tag */
		SAX_WITHIN_TAG_NAME        = 0x0020, /* within the name of a tag */
		SAX_WITHIN_ATTRIBUTE_LIST  = 0x0040, /* within a list of attributes */
		SAX_WITHIN_ATTRIBUTE_NAME  = 0x0080, /* within the name of an attribute */
		SAX_WITHIN_ATTRIBUTE_EQUAL = 0x0100, /* within the name of an attribute */
		SAX_WITHIN_ATTRIBUTE_VALUE = 0x0200, /* within the value of an attribute */
		SAX_WITHIN_CONTENT         = 0x0400, /* within the content of a tag */
		SAX_WITHIN_CDATA           = 0x0800, /* within a CDATA part */
	} tSaxFlags;
	if (xml == NULL || visitor == NULL) return PSRT_INVALID_ARGUMENT;
	const char * ptr = xml;
	const char * lastNonSpace = NULL;
	const char * nsSep = NULL;
	char attrValueSep = 0;
	tPToken tokens[3] = { 0 };
	tPToken startTag[2] = { 0 };
	tSaxFlags flags = SAX_START;
	size_t level = 0;
	tPSaxReturnType error = PSRT_UNEXPECTED_CHARACTER;
#define WITHIN(x)  ((flags & SAX_WITHIN_##x) != 0)
#define VISIT(x) \
	do { \
		if (visitor(PSTT_##x, tokens, level, param) == 0) { \
			if (errorPos != NULL) *errorPos = (tokens->start != NULL) ? tokens->start : ptr; \
			return PSRT_ABORT; \
		} \
	} while (0)
#define ONERROR(x) do {error = PSRT_##x; goto onError;} while ( 0 )
	for (size_t n = 0; n < length && *ptr != 0; ) {
#ifdef PCF_P_SAX_DEBUG
		const char * flagStr[] = {"XML", "INSTRUCTION", "COMMENT", "START_TAG", "END_TAG", "TAG_NAME", "ATTRIBUTE_LIST", "ATTRIBUTE_NAME", "ATTRIBUTE_EQUAL", "ATTRIBUTE_VALUE", "CONTENT", "CDATA"};
		fprintf(stderr, "char =");
		if (isprint(*ptr) != 0) {
			fprintf(stderr, " '%c'", *ptr);
		} else {
			fprintf(stderr, "    ");
		}
		fprintf(stderr, " 0x%02X", *ptr);
		if (flags == SAX_START) {
			fprintf(stderr, ", flags = START\n");
		} else {
			int first = 1;
			for (size_t i = 0; i < 12; i++) {
				if (((size_t)(1 << i) & (size_t)flags) != 0) {
					fprintf(stderr, "%s%s", (first == 1) ? ", flags = " : " | ", flagStr[i]);
					first = 0;
				}
			}
			if (nsSep != NULL) fprintf(stderr, ", has namespace");
			fprintf(stderr, "\n");
		}
		if (*ptr != xml[n]) {
			fprintf(stderr, "Error: n position does not match ptr\n");
			abort();
		}
		fflush(stderr);
#endif /* PCF_P_SAX_DEBUG */
		/* the order of checks is important here */
		if (flags == SAX_START || WITHIN(CONTENT)) {
			if (*ptr == '<') {
				if (tokens[0].start != NULL && (lastNonSpace - tokens[0].start) >= 0) {
					tokens[0].length = (size_t)((lastNonSpace + 1) - tokens[0].start);
					VISIT(CONTENT);
				}
				if ((n + 1) < length && ptr[1] == '?') {
					flags = SAX_WITHIN_INSTRUCTION;
					lastNonSpace = NULL;
					tokens[0].start = NULL;
					tokens[0].length = 0;
					tokens[1].start = NULL;
					tokens[1].length = 0;
					n++;
					ptr++;
				} else if ((n + 8) < length && ptr[1] == '!' && ptr[2] == '[' && ptr[3] == 'C' && ptr[4] == 'D' && ptr[5] == 'A' && ptr[6] == 'T' && ptr[7] == 'A' && ptr[8] == '[') {
					flags = SAX_WITHIN_CDATA;
					n += 8;
					ptr += 8;
					tokens[0].start = ptr + 1;
					tokens[0].length = 0;
				} else if ((n + 3) < length && ptr[1] == '!' && ptr[2] == '-' && ptr[3] == '-') {
					flags = SAX_WITHIN_COMMENT;
					n += 3;
					ptr += 3;
				} else {
					tokens[0].start = NULL;
					tokens[1].start = NULL;
					if ((n + 1) < length && ptr[1] != '/') {
						flags = SAX_WITHIN_START_TAG;
					} else {
						flags = SAX_WITHIN_END_TAG;
						n++;
						ptr++;
					}
				}
			} else if (WITHIN(CONTENT) && *ptr != '>') {	
				if (p_isXmlWhiteSpace(*ptr) == 0) {
					lastNonSpace = ptr;
					if (tokens[0].start == NULL) {
						tokens[0].start = ptr;
					}
				}
			} else {
				ONERROR(UNEXPECTED_CHARACTER);
			}
		} else if ( WITHIN(ATTRIBUTE_NAME) ) {
			if (p_isXmlNameChar(*ptr) != 0) {
				if (*ptr == ':') {
					if (nsSep != NULL) ONERROR(UNEXPECTED_CHARACTER);
					nsSep = ptr;
				}
				tokens[1].length++;
			} else if (p_isXmlWhiteSpace(*ptr) != 0) {
				flags = (tSaxFlags)((flags & ((tSaxFlags)~SAX_WITHIN_ATTRIBUTE_NAME)) | SAX_WITHIN_ATTRIBUTE_EQUAL);
			} else if (*ptr == '=') {
				flags = (tSaxFlags)((flags & ((tSaxFlags)~SAX_WITHIN_ATTRIBUTE_NAME)) | SAX_WITHIN_ATTRIBUTE_EQUAL);
				attrValueSep = '=';
			} else {
				ONERROR(EXPECTED_ATTR_EQUAL);
			}
		} else if ( WITHIN(ATTRIBUTE_EQUAL) ) {
			if (*ptr == '=' && attrValueSep != '=') {
				attrValueSep = '=';
			} else if (attrValueSep == '=' && (*ptr == '\'' || *ptr == '"')) {
				flags = (tSaxFlags)((flags & ((tSaxFlags)~SAX_WITHIN_ATTRIBUTE_EQUAL)) | SAX_WITHIN_ATTRIBUTE_VALUE);
				attrValueSep = *ptr;
				if ((n + 1) < length) {
					tokens[2].start = ptr + 1;
					tokens[2].length = 0;
				}
			} else if (p_isXmlWhiteSpace(*ptr) == 0) {
				ONERROR(EXPECTED_ATTR_VALUE);
			}
		} else if ( WITHIN(ATTRIBUTE_VALUE) ) {
			if (*ptr == attrValueSep) {
				flags = (tSaxFlags)(flags & ((tSaxFlags)~SAX_WITHIN_ATTRIBUTE_VALUE));
				if (nsSep != NULL) {
					tokens[0].start = tokens[1].start;
					tokens[0].length = (size_t)(nsSep - tokens[0].start);
					tokens[1].start += (tokens[0].length + 1);
					tokens[1].length -= (tokens[0].length + 1);
					nsSep = NULL;
				}
				VISIT(ATTRIBUTE);
			} else {
				tokens[2].length++;
			}
		} else if ( WITHIN(ATTRIBUTE_LIST) ) {
			if (WITHIN(INSTRUCTION) && (n + 1) < length && *ptr == '?' && ptr[1] == '>') {
				flags = SAX_WITHIN_CONTENT;
				lastNonSpace = NULL;
				tokens[0].start = NULL;
				tokens[1].start = NULL;
				n += 2;
				ptr += 2;
			} else if (*ptr == '>' && !WITHIN(INSTRUCTION)) {
				if ( WITHIN(END_TAG) ) {
					if ( WITHIN(START_TAG) ) {
						tokens[0] = startTag[0];
						tokens[1] = startTag[1];
						VISIT(END_TAG);
					} else {
						/* unreachable, because ATTRIBUTE_LIST is never set in this case */
						ONERROR(EXPECTED_TAG_END);
					}
				} else if ( WITHIN(START_TAG) ) {
					level++;
				}
				flags = SAX_WITHIN_CONTENT;
				lastNonSpace = NULL;
				tokens[0].start = NULL;
				tokens[1].start = NULL;
			} else if (p_isXmlNameStartChar(*ptr) != 0 && !WITHIN(END_TAG)) {
				flags = (tSaxFlags)(flags | SAX_WITHIN_ATTRIBUTE_NAME);
				tokens[0].start = NULL;
				tokens[0].length = 0;
				tokens[1].start = ptr;
				tokens[1].length = 1;
				tokens[2].start = NULL;
				tokens[2].length = 0;
				attrValueSep = 0;
				nsSep = (*ptr == ':') ? ptr : NULL;
			} else if (*ptr == '/' && !WITHIN(END_TAG)) {
				flags = (tSaxFlags)(flags | SAX_WITHIN_END_TAG);
			} else if (p_isXmlWhiteSpace(*ptr) == 0) {
				if ( WITHIN(INSTRUCTION) ) {
					ONERROR(EXPECTED_PI_END);
				} else {
					ONERROR(EXPECTED_ATTR_NAME);
				}
			}
		} else if ( WITHIN(INSTRUCTION) ) {
			if ((n + 1) < length && *ptr == '?' && ptr[1] == '>') {
				if (lastNonSpace != NULL) {
					tokens[1].length = (size_t)((lastNonSpace + 1) - tokens[1].start);
					if (nsSep != NULL) {
						tokens[0].start = tokens[1].start;
						tokens[0].length = (size_t)(nsSep - tokens[0].start);
						tokens[1].start += (tokens[0].length + 1);
						tokens[1].length -= (tokens[0].length + 1);
						nsSep = NULL;
					}
					if (tokens[1].length >= 3 && tolower(tokens[1].start[0]) == 'x' && tolower(tokens[1].start[1]) == 'm' && tolower(tokens[1].start[2]) == 'l') {
						VISIT(XML);
					} else {
						VISIT(INSTRUCTION);
					}
				}
				flags = SAX_WITHIN_CONTENT;
				lastNonSpace = NULL;
				tokens[0].start = NULL;
				tokens[1].start = NULL;
				n += 2;
				ptr += 2;
			} else if ( WITHIN(TAG_NAME) ) {
				if (p_isXmlNameChar(*ptr) == 0) {
					if (tokens[1].length >= 3 && tolower(tokens[1].start[0]) == 'x' && tolower(tokens[1].start[1]) == 'm' && tolower(tokens[1].start[2]) == 'l') {
						flags = (tSaxFlags)(flags | SAX_WITHIN_XML);
					}
					if (nsSep != NULL) {
						tokens[0].start = tokens[1].start;
						tokens[0].length = (size_t)(nsSep - tokens[0].start);
						tokens[1].start += (tokens[0].length + 1);
						tokens[1].length -= (tokens[0].length + 1);
						nsSep = NULL;
					}
					const int res = visitor(WITHIN(XML) ? PSTT_PARSE_XML : PSTT_PARSE_INSTRUCTION, tokens, level, param);
					if (res == 2) {
						if ( WITHIN(XML) ) {
							VISIT(XML);
						} else {
							VISIT(INSTRUCTION);
						}
						flags = (tSaxFlags)(flags | SAX_WITHIN_ATTRIBUTE_LIST);
						tokens[0].start = NULL;
						tokens[0].length = 0;
						tokens[1].start = NULL;
						tokens[1].length = 0;
						if (p_isXmlWhiteSpace(*ptr) == 0) continue; /* re-evaluate current character */
					} else if (res == 1) {
						flags = (tSaxFlags)(flags & ((tSaxFlags)~SAX_WITHIN_TAG_NAME));
					} else {
						if (errorPos != NULL) *errorPos = (tokens[1].start != NULL) ? tokens[1].start : ptr;
						return PSRT_ABORT;
					}
				} else if (p_isXmlNameChar(*ptr) != 0) {
					if (*ptr == ':') {
						if (nsSep != NULL) {
							/* treat as raw processing instruction */
							flags = (tSaxFlags)(flags & ((tSaxFlags)~SAX_WITHIN_TAG_NAME));
						} else {
							nsSep = ptr;
						}
					}
					tokens[1].length++;
					lastNonSpace = ptr;
				} else {
					/* treat as raw processing instruction */
					flags = (tSaxFlags)(flags & ((tSaxFlags)~SAX_WITHIN_TAG_NAME));
				}
			} else if (p_isXmlNameChar(*ptr) != 0 && lastNonSpace == NULL) {
				flags = (tSaxFlags)(flags | SAX_WITHIN_TAG_NAME);
				lastNonSpace = ptr;
				tokens[1].start = ptr;
				tokens[1].length = 1;
				nsSep = (*ptr == ':') ? ptr : NULL;
			} else if (p_isXmlWhiteSpace(*ptr) == 0) {
				lastNonSpace = ptr;
				if (tokens[1].start == NULL) {
					tokens[1].start = ptr;
					tokens[1].length = 1;
				}
			} /* other cases are ignored */
		} else if ( WITHIN(TAG_NAME) ) {
			if (*ptr == '>') {
				if (nsSep != NULL) {
					tokens[0].start = tokens[1].start;
					tokens[0].length = (size_t)(nsSep - tokens[0].start);
					tokens[1].start += (tokens[0].length + 1);
					tokens[1].length -= (tokens[0].length + 1);
					nsSep = NULL;
				}
				if ( WITHIN(END_TAG) ) {
					level--;
					VISIT(END_TAG);
				} else if ( WITHIN(START_TAG) ) {
					VISIT(START_TAG);
					level++;
				} else {
					ONERROR(UNEXPECTED_CHARACTER);
				}
				flags = SAX_WITHIN_CONTENT;
				lastNonSpace = NULL;
				tokens[0].start = NULL;
				tokens[1].start = NULL;
			} else if ((n + 1) < length && *ptr == '?' && ptr[1] == '>') {
				flags = SAX_WITHIN_CONTENT;
				lastNonSpace = NULL;
				tokens[0].start = NULL;
				tokens[1].start = NULL;
				n++;
				ptr++;
			} else if (*ptr == '/' && !WITHIN(END_TAG)) {
				if (nsSep != NULL) {
					tokens[0].start = tokens[1].start;
					tokens[0].length = (size_t)(nsSep - tokens[0].start);
					tokens[1].start += (tokens[0].length + 1);
					tokens[1].length -= (tokens[0].length + 1);
					nsSep = NULL;
				}
				VISIT(START_TAG);
				VISIT(END_TAG);
				flags = (tSaxFlags)((flags & ((tSaxFlags)~SAX_WITHIN_TAG_NAME)) | SAX_WITHIN_END_TAG);
			} else if (p_isXmlWhiteSpace(*ptr) != 0) {
				if (nsSep != NULL) {
					tokens[0].start = tokens[1].start;
					tokens[0].length = (size_t)(nsSep - tokens[0].start);
					tokens[1].start += (tokens[0].length + 1);
					tokens[1].length -= (tokens[0].length + 1);
					nsSep = NULL;
				}
				if ( WITHIN(END_TAG) ) {
					level--;
					VISIT(END_TAG);
					flags = (tSaxFlags)(flags & ((tSaxFlags)~SAX_WITHIN_TAG_NAME));
				} else {
					VISIT(START_TAG);
					flags = (tSaxFlags)((flags & ((tSaxFlags)~SAX_WITHIN_TAG_NAME)) | SAX_WITHIN_ATTRIBUTE_LIST);
					startTag[0] = tokens[0];
					startTag[1] = tokens[1];
				}
			} else if (p_isXmlNameChar(*ptr) != 0) {
				if (*ptr == ':') {
					if (nsSep != NULL) ONERROR(UNEXPECTED_CHARACTER);
					nsSep = ptr;
				}
				tokens[1].length++;
			} else if ( WITHIN(END_TAG) ) {
				ONERROR(EXPECTED_TAG_END);
			} else {
				ONERROR(EXPECTED_ANY_TAG_END);
			}
		} else if ( WITHIN(TAG) ) {
			if (p_isXmlNameStartChar(*ptr) != 0 && (flags & SAX_WITHIN_TAG) != SAX_WITHIN_TAG && tokens[1].start == NULL) {
				flags = (tSaxFlags)(flags | SAX_WITHIN_TAG_NAME);
				tokens[0].start = NULL;
				tokens[0].length = 0;
				tokens[1].start = ptr;
				tokens[1].length = 1;
				nsSep = (*ptr == ':') ? ptr : NULL;
			} else if (*ptr == '>') {
				flags = SAX_WITHIN_CONTENT;
				lastNonSpace = NULL;
				tokens[0].start = NULL;
				tokens[1].start = NULL;
			} else if (p_isXmlWhiteSpace(*ptr) == 0) {
				ONERROR(EXPECTED_TAG_NAME);
			}
		} else if ( WITHIN(COMMENT) ) {
			if ((n + 2) < length && *ptr == '-' && ptr[1] == '-' && ptr[2] == '>') {
				flags = SAX_WITHIN_CONTENT;
				lastNonSpace = NULL;
				tokens[0].start = NULL;
				tokens[1].start = NULL;
				n += 2;
				ptr += 2;
			}
		} else if ( WITHIN(CDATA) ) {
			if ((n + 2) < length && *ptr == ']' && ptr[1] == ']' && ptr[2] == '>') {
				if (tokens[0].length > 0) {
					VISIT(CDATA);
				}
				flags = SAX_WITHIN_CONTENT;
				lastNonSpace = NULL;
				tokens[0].start = NULL;
				tokens[1].start = NULL;
				n += 2;
				ptr += 2;
			} else {
				tokens[0].length++;
			}
		} else {
			ONERROR(UNEXPECTED_CHARACTER);
		}
		n++;
		ptr++;
	}
	if (flags != SAX_START && !WITHIN(CONTENT)) {
		ONERROR(UNEXPECTED_END);
	}
#undef WITHIN
#undef VISIT
#undef ONERROR
	return PSRT_SUCCESS;
onError:
	if (errorPos != NULL) *errorPos = ptr;
	return error;
}
