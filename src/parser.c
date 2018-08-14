/**
 * @file parser.c
 * @author Daniel Starke
 * @see parser.h
 * @date 2018-06-23
 * @version 2018-08-11
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
#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "bsearch.h"
#include "parser.h"
#include "target.h"
#include "utf8.h"


/**
 * Internal helper enumeration to check various character types.
 * 
 * @see p_charType
 */
typedef enum {
	PCT_NONE                = 0x00, /**< does not match any character type */
	PCT_XML_NAME_START_CHAR = 0x01, /**< valid characters for the first character of a XML name */
	PCT_XML_NAME_CHAR       = 0x02, /**< valid characters within a XML name */
	PCT_XML_WHITE_SPACE     = 0x04, /**< valid XML white space characters */
	PCT_XML_NEED_ESCAPE     = 0x08, /**< character which needs to be escaped in XML */
	PCT_URL_NEED_ESCAPE     = 0x10, /**< character which needs to be escaped in a URL */
	PCT_HTTP_TCHAR          = 0x20, /**< HTTP tchar as defined by https://tools.ietf.org/html/rfc7230 */
	PCT_HTTP_DELIMITER      = 0x40  /**< HTTP delimiter as defined by https://tools.ietf.org/html/rfc7230 */
} tPCharType;


/**
 * Internal array to check various character types.
 */
static const char p_charType[] = {
	/*     0x00 */ PCT_NONE,
	/*     0x01 */ PCT_NONE,
	/*     0x02 */ PCT_NONE,
	/*     0x03 */ PCT_NONE,
	/*     0x04 */ PCT_NONE,
	/*     0x05 */ PCT_NONE,
	/*     0x06 */ PCT_NONE,
	/*     0x07 */ PCT_NONE,
	/*     0x08 */ PCT_NONE,
	/*     0x09 */ PCT_XML_WHITE_SPACE,
	/*     0x0A */ PCT_XML_WHITE_SPACE,
	/*     0x0B */ PCT_NONE,
	/*     0x0C */ PCT_NONE,
	/*     0x0D */ PCT_XML_WHITE_SPACE,
	/*     0x0E */ PCT_NONE,
	/*     0x0F */ PCT_NONE,
	/*     0x10 */ PCT_NONE,
	/*     0x11 */ PCT_NONE,
	/*     0x12 */ PCT_NONE,
	/*     0x13 */ PCT_NONE,
	/*     0x14 */ PCT_NONE,
	/*     0x15 */ PCT_NONE,
	/*     0x16 */ PCT_NONE,
	/*     0x17 */ PCT_NONE,
	/*     0x18 */ PCT_NONE,
	/*     0x19 */ PCT_NONE,
	/*     0x1A */ PCT_NONE,
	/*     0x1B */ PCT_NONE,
	/*     0x1C */ PCT_NONE,
	/*     0x1D */ PCT_NONE,
	/*     0x1E */ PCT_NONE,
	/*     0x1F */ PCT_NONE,
	/* ' ' 0x20 */ PCT_XML_WHITE_SPACE | PCT_URL_NEED_ESCAPE,
	/* '!' 0x21 */ PCT_NONE | PCT_URL_NEED_ESCAPE | PCT_HTTP_TCHAR,
	/* '"' 0x22 */ PCT_XML_NEED_ESCAPE | PCT_HTTP_DELIMITER,
	/* '#' 0x23 */ PCT_NONE | PCT_URL_NEED_ESCAPE | PCT_HTTP_TCHAR,
	/* '$' 0x24 */ PCT_NONE | PCT_URL_NEED_ESCAPE | PCT_HTTP_TCHAR,
	/* '%' 0x25 */ PCT_NONE | PCT_URL_NEED_ESCAPE | PCT_HTTP_TCHAR,
	/* '&' 0x26 */ PCT_XML_NEED_ESCAPE | PCT_URL_NEED_ESCAPE | PCT_HTTP_TCHAR,
	/* ''' 0x27 */ PCT_XML_NEED_ESCAPE | PCT_URL_NEED_ESCAPE | PCT_HTTP_TCHAR,
	/* '(' 0x28 */ PCT_NONE | PCT_URL_NEED_ESCAPE | PCT_HTTP_DELIMITER,
	/* ')' 0x29 */ PCT_NONE | PCT_URL_NEED_ESCAPE | PCT_HTTP_DELIMITER,
	/* '*' 0x2A */ PCT_NONE | PCT_URL_NEED_ESCAPE | PCT_HTTP_TCHAR,
	/* '+' 0x2B */ PCT_NONE | PCT_URL_NEED_ESCAPE | PCT_HTTP_TCHAR,
	/* ',' 0x2C */ PCT_NONE | PCT_URL_NEED_ESCAPE | PCT_HTTP_DELIMITER,
	/* '-' 0x2D */ PCT_XML_NAME_CHAR | PCT_HTTP_TCHAR,
	/* '.' 0x2E */ PCT_XML_NAME_CHAR | PCT_HTTP_TCHAR,
	/* '/' 0x2F */ PCT_NONE | PCT_URL_NEED_ESCAPE | PCT_HTTP_DELIMITER,
	/* '0' 0x30 */ PCT_XML_NAME_CHAR | PCT_HTTP_TCHAR,
	/* '1' 0x31 */ PCT_XML_NAME_CHAR | PCT_HTTP_TCHAR,
	/* '2' 0x32 */ PCT_XML_NAME_CHAR | PCT_HTTP_TCHAR,
	/* '3' 0x33 */ PCT_XML_NAME_CHAR | PCT_HTTP_TCHAR,
	/* '4' 0x34 */ PCT_XML_NAME_CHAR | PCT_HTTP_TCHAR,
	/* '5' 0x35 */ PCT_XML_NAME_CHAR | PCT_HTTP_TCHAR,
	/* '6' 0x36 */ PCT_XML_NAME_CHAR | PCT_HTTP_TCHAR,
	/* '7' 0x37 */ PCT_XML_NAME_CHAR | PCT_HTTP_TCHAR,
	/* '8' 0x38 */ PCT_XML_NAME_CHAR | PCT_HTTP_TCHAR,
	/* '9' 0x39 */ PCT_XML_NAME_CHAR | PCT_HTTP_TCHAR,
	/* ':' 0x3A */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE | PCT_HTTP_DELIMITER,
	/* ';' 0x3B */ PCT_NONE | PCT_URL_NEED_ESCAPE | PCT_HTTP_DELIMITER,
	/* '<' 0x3C */ PCT_XML_NEED_ESCAPE | PCT_HTTP_DELIMITER,
	/* '=' 0x3D */ PCT_NONE | PCT_URL_NEED_ESCAPE | PCT_HTTP_DELIMITER,
	/* '>' 0x3E */ PCT_XML_NEED_ESCAPE | PCT_HTTP_DELIMITER,
	/* '?' 0x3F */ PCT_NONE | PCT_URL_NEED_ESCAPE | PCT_HTTP_DELIMITER,
	/* '@' 0x40 */ PCT_NONE | PCT_URL_NEED_ESCAPE | PCT_HTTP_DELIMITER,
	/* 'A' 0x41 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_HTTP_TCHAR,
	/* 'B' 0x42 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_HTTP_TCHAR,
	/* 'C' 0x43 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_HTTP_TCHAR,
	/* 'D' 0x44 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_HTTP_TCHAR,
	/* 'E' 0x45 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_HTTP_TCHAR,
	/* 'F' 0x46 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_HTTP_TCHAR,
	/* 'G' 0x47 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_HTTP_TCHAR,
	/* 'H' 0x48 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_HTTP_TCHAR,
	/* 'I' 0x49 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_HTTP_TCHAR,
	/* 'J' 0x4A */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_HTTP_TCHAR,
	/* 'K' 0x4B */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_HTTP_TCHAR,
	/* 'L' 0x4C */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_HTTP_TCHAR,
	/* 'M' 0x4D */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_HTTP_TCHAR,
	/* 'N' 0x4E */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_HTTP_TCHAR,
	/* 'O' 0x4F */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_HTTP_TCHAR,
	/* 'P' 0x50 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_HTTP_TCHAR,
	/* 'Q' 0x51 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_HTTP_TCHAR,
	/* 'R' 0x52 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_HTTP_TCHAR,
	/* 'S' 0x53 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_HTTP_TCHAR,
	/* 'T' 0x54 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_HTTP_TCHAR,
	/* 'U' 0x55 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_HTTP_TCHAR,
	/* 'V' 0x56 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_HTTP_TCHAR,
	/* 'W' 0x57 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_HTTP_TCHAR,
	/* 'X' 0x58 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_HTTP_TCHAR,
	/* 'Y' 0x59 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_HTTP_TCHAR,
	/* 'Z' 0x5A */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_HTTP_TCHAR,
	/* '[' 0x5B */ PCT_NONE | PCT_URL_NEED_ESCAPE | PCT_HTTP_DELIMITER,
	/* '\' 0x5C */ PCT_NONE | PCT_HTTP_DELIMITER,
	/* ']' 0x5D */ PCT_NONE | PCT_URL_NEED_ESCAPE | PCT_HTTP_DELIMITER,
	/* '^' 0x5E */ PCT_HTTP_TCHAR,
	/* '_' 0x5F */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_HTTP_TCHAR,
	/* '`' 0x60 */ PCT_HTTP_TCHAR,
	/* 'a' 0x61 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_HTTP_TCHAR,
	/* 'b' 0x62 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_HTTP_TCHAR,
	/* 'c' 0x63 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_HTTP_TCHAR,
	/* 'd' 0x64 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_HTTP_TCHAR,
	/* 'e' 0x65 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_HTTP_TCHAR,
	/* 'f' 0x66 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_HTTP_TCHAR,
	/* 'g' 0x67 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_HTTP_TCHAR,
	/* 'h' 0x68 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_HTTP_TCHAR,
	/* 'i' 0x69 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_HTTP_TCHAR,
	/* 'j' 0x6A */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_HTTP_TCHAR,
	/* 'k' 0x6B */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_HTTP_TCHAR,
	/* 'l' 0x6C */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_HTTP_TCHAR,
	/* 'm' 0x6D */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_HTTP_TCHAR,
	/* 'n' 0x6E */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_HTTP_TCHAR,
	/* 'o' 0x6F */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_HTTP_TCHAR,
	/* 'p' 0x70 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_HTTP_TCHAR,
	/* 'q' 0x71 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_HTTP_TCHAR,
	/* 'r' 0x72 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_HTTP_TCHAR,
	/* 's' 0x73 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_HTTP_TCHAR,
	/* 't' 0x74 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_HTTP_TCHAR,
	/* 'u' 0x75 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_HTTP_TCHAR,
	/* 'v' 0x76 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_HTTP_TCHAR,
	/* 'w' 0x77 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_HTTP_TCHAR,
	/* 'x' 0x78 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_HTTP_TCHAR,
	/* 'y' 0x79 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_HTTP_TCHAR,
	/* 'z' 0x7A */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_HTTP_TCHAR,
	/* '{' 0x7B */ PCT_HTTP_DELIMITER,
	/* '|' 0x7C */ PCT_HTTP_TCHAR,
	/* '}' 0x7D */ PCT_HTTP_DELIMITER,
	/* '~' 0x7E */ PCT_HTTP_TCHAR,
	/*     0x7F */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR,
	/*     0x80 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0x81 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0x82 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0x83 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0x84 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0x85 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0x86 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0x87 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0x88 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0x89 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0x8A */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0x8B */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0x8C */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0x8D */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0x8E */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0x8F */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0x90 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0x91 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0x92 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0x93 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0x94 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0x95 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0x96 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0x97 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0x98 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0x99 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0x9A */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0x9B */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0x9C */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0x9D */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0x9E */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0x9F */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xA0 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xA1 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xA2 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xA3 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xA4 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xA5 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xA6 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xA7 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xA8 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xA9 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xAA */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xAB */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xAC */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xAD */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xAE */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xAF */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xB0 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xB1 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xB2 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xB3 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xB4 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xB5 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xB6 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xB7 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xB8 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xB9 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xBA */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xBB */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xBC */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xBD */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xBE */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xBF */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xC0 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xC1 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xC2 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xC3 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xC4 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xC5 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xC6 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xC7 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xC8 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xC9 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xCA */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xCB */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xCC */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xCD */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xCE */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xCF */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xD0 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xD1 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xD2 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xD3 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xD4 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xD5 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xD6 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xD7 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xD8 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xD9 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xDA */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xDB */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xDC */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xDD */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xDE */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xDF */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xE0 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xE1 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xE2 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xE3 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xE4 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xE5 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xE6 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xE7 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xE8 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xE9 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xEA */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xEB */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xEC */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xED */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xEE */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xEF */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xF0 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xF1 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xF2 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xF3 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xF4 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xF5 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xF6 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xF7 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xF8 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xF9 */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xFA */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xFB */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xFC */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xFD */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xFE */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
	/*     0xFF */ PCT_XML_NAME_START_CHAR | PCT_XML_NAME_CHAR | PCT_URL_NEED_ESCAPE,
};


/**
 * Compares the given token with a passed string. Both are compared case sensitive. The token needs
 * to match the passed string exactly and completely to return 0.
 * 
 * @param[in] token - token to compare
 * @param[in] str - compare with this string
 * @return same as strcmp
 */
int p_cmpToken(const tPToken * token, const char * str) {
	if (token == NULL || token->start == NULL || str == NULL) {
		errno = EFAULT;
		return INT_MAX;
	}
	const unsigned char * left = (const unsigned char *)(token->start);
	const unsigned char * right = (const unsigned char *)str;
	size_t length = token->length;
	for (;length > 0 && *right != 0 && *left == *right; length--, left++, right++);
	if (length > 0 && *right == 0) {
		return *left;
	} else if (length == 0 && *right != 0) {
		return -(*right);
	} else if (length == 0 && *right == 0) {
		return 0;
	}
	return *left - *right;
}


/**
 * Compares the given token with a passed string. Both are compared case insensitive. The token
 * needs to match the passed string exactly and completely to return 0. toupper is being used to
 * normalize cases.
 * 
 * @param[in] token - token to compare
 * @param[in] str - compare with this string
 * @return same as stricmp
 * @warning Do not use this function if the character value range of str exceeds 7 bits (ANSI).
 */
int p_cmpTokenI(const tPToken * token, const char * str) {
	if (token == NULL || token->start == NULL || str == NULL) {
		errno = EFAULT;
		return INT_MAX;
	}
	const unsigned char * left = (const unsigned char *)(token->start);
	const unsigned char * right = (const unsigned char *)str;
	size_t length = token->length;
	for (;length > 0 && *right != 0 && toupper(*left) == toupper(*right); length--, left++, right++);
	if (length > 0 && *right == 0) {
		return toupper(*left);
	} else if (length == 0 && *right != 0) {
		return -toupper(*right);
	} else if (length == 0 && *right == 0) {
		return 0;
	}
	return toupper(*left) - toupper(*right);
}


/**
 * Compares the given tokens with each other. Both are compared case sensitive. The tokens needs
 * to match exactly and completely to return 0.
 * 
 * @param[in] lhs - left-hand statement of comparison
 * @param[in] rhs - right-hand statement of comparison
 * @return same as strcmp
 */
int p_cmpTokens(const tPToken * lhs, const tPToken * rhs) {
	if (lhs == NULL || lhs->start == NULL || rhs == NULL || rhs->start == NULL) {
		errno = EFAULT;
		return INT_MAX;
	}
	size_t l = lhs->length;
	size_t r = rhs->length;
	size_t m = PCF_MIN(l, r);
	const unsigned char * left  = (const unsigned char *)(lhs->start);
	const unsigned char * right = (const unsigned char *)(rhs->start);
	for (;m > 0 && *left == *right; m--, left++, right++);
	if (m == 0) {
		if (l > r) {
			return *((const unsigned char *)(lhs->start + r));
		} else if (l < r) {
			return -(*((const unsigned char *)(rhs->start + l)));
		} else {
			/* same length and same content */
			return 0;
		}
	}
	return *left - *right;
}


/**
 * Compares the given tokens with each other. Both are compared case insensitive. The tokens needs
 * to match exactly and completely to return 0. toupper is being used to normalize cases.
 * 
 * @param[in] lhs - left-hand statement of comparison
 * @param[in] rhs - right-hand statement of comparison
 * @return same as stricmp
 * @warning Do not use this function if the character value range of rhs exceeds 7 bits (ANSI).
 */
int p_cmpTokensI(const tPToken * lhs, const tPToken * rhs) {
	if (lhs == NULL || lhs->start == NULL || rhs == NULL || rhs->start == NULL) {
		errno = EFAULT;
		return INT_MAX;
	}
	size_t l = lhs->length;
	size_t r = rhs->length;
	size_t m = PCF_MIN(l, r);
	const unsigned char * left  = (const unsigned char *)(lhs->start);
	const unsigned char * right = (const unsigned char *)(rhs->start);
	for (;m > 0 && toupper(*left) == toupper(*right); m--, left++, right++);
	if (m == 0) {
		if (l > r) {
			return toupper(*((const unsigned char *)(lhs->start + r)));
		} else if (l < r) {
			return -toupper(*((const unsigned char *)(rhs->start + l)));
		} else {
			/* same length and same content */
			return 0;
		}
	}
	return toupper(*left) - toupper(*right);
}


/**
 * Creates a null-terminated string copy of the given parser token.
 * 
 * @param[in] token - token to copy
 * @return Copy on success, NULL on error
 */
char * p_copyToken(const tPToken * token) {
	if (token->start == NULL) return NULL;
	char * result = (char *)malloc(token->length + 1);
	if (result == NULL) return result;
	if (token->length > 0) memcpy(result, token->start, token->length);
	result[token->length] = 0;
	return result;
}


/**
 * Returns the position from the given pointer starting at line 0 and column 0.
 * Carrier-return characters are ignored. Lines are split by line-feeds.
 * 
 * @param[in] str - input string
 * @param[in] length - maximum length of the input string in bytes
 * @param[in] pos - pointer to a position within the string for with the position shall be returned
 * @param[in] tabLen - assume that a tabulator takes this many columns
 * @param[out] output - set position in the pointed variable
 * @return 1 on success, 0 on error
 */
int p_getPos(const char * str, const size_t length, const char * pos, const size_t tabLen, tParserPos * output) {
	if (str == NULL || pos < str || output == NULL) return 0;
	const char * ptr = str;
	size_t l = 0;
	size_t c = 0;
	const char * f = ptr;
	for (size_t i = 0; i < length && *ptr != 0 && ptr <= pos; i++, ptr++) {
		switch (*ptr) {
		case '\r':
			/* carrier-returns are ignored */
			break;
		case '\t':
			c += tabLen;
			break;
		case '\n':
			l++;
			c = 0;
			if (ptr < pos && (i + 1) < length) f = ptr + 1;
			break;
		default:
			/* we only count the first byte of a UTF-8 character */
			if ((*ptr & 0xC0) != 0x80) c++;
			break;
		}
	}
	output->line   = l;
	output->column = c;
	output->front  = f;
	return 1;
}


/**
 * Helper function to compare the str field of two tPXmlUnEscMapEntity items.
 * 
 * @param[in] lhs - left-hand side
 * @param[in] rhs - right-hand side
 * @return same as strncmp()
 */
int p_cmpXmlUnEscMapEntities(const void * lhs, const void * rhs) {
	const tPXmlUnEscMapEntity * a = (const tPXmlUnEscMapEntity *)lhs;
	const tPXmlUnEscMapEntity * b = (const tPXmlUnEscMapEntity *)rhs;
	const tPToken aToken = {
		/* .start  = */ a->str,
		/* .length = */ a->strSize
	};
	const tPToken bToken = {
		/* .start  = */ b->str,
		/* .length = */ b->strSize
	};
	return p_cmpTokens(&aToken, &bToken);
}


/**
 * Returns whether the given value is a valid XML name start character or not.
 * 
 * @param[in] value - test this value
 * @return 1 if true, else 0
 */
int p_isXmlNameStartChar(const int value) {
	return ((p_charType[value & 0xFF] & PCT_XML_NAME_START_CHAR) != 0) ? 1 : 0;
}


/**
 * Returns whether the given value is a valid XML name character or not.
 * 
 * @param[in] value - test this value
 * @return 1 if true, else 0
 */
int p_isXmlNameChar(const int value) {
	return ((p_charType[value & 0xFF] & PCT_XML_NAME_CHAR) != 0) ? 1 : 0;
}


/**
 * Returns whether the given value is a XML white space character or not.
 * 
 * @param[in] value - test this value
 * @return 1 if true, else 0
 */
int p_isXmlWhiteSpace(const int value) {
	return ((p_charType[value & 0xFF] & PCT_XML_WHITE_SPACE) != 0) ? 1 : 0;
}


/**
 * Returns whether the given value needs to be escaped in XML or not.
 * 
 * @param[in] value - test this value
 * @return 1 if true, else 0
 */
int p_isXmlNeedEscape(const int value) {
	return ((p_charType[value & 0xFF] & PCT_XML_NEED_ESCAPE) != 0) ? 1 : 0;
}


/**
 * Returns whether the given value needs to be escaped in a URL or not.
 * 
 * @param[in] value - test this value
 * @return 1 if true, else 0
 */
int p_isUrlNeedEscape(const int value) {
	return ((p_charType[value & 0xFF] & PCT_URL_NEED_ESCAPE) != 0) ? 1 : 0;
}


/**
 * Returns whether the given value is a TCHAR as defined in https://tools.ietf.org/html/rfc7230.
 * 
 * @param[in] value - test this value
 * @return 1 if true, else 0
 */
int p_isHttpTChar(const int value) {
	return ((p_charType[value & 0xFF] & PCT_HTTP_TCHAR) != 0) ? 1 : 0;
}


/**
 * Returns whether the given value is a delimiter as defined in https://tools.ietf.org/html/rfc7230.
 * 
 * @param[in] value - test this value
 * @return 1 if true, else 0
 */
int p_isHttpDelimiter(const int value) {
	return ((p_charType[value & 0xFF] & PCT_HTTP_DELIMITER) != 0) ? 1 : 0;
}


/**
 * The function escapes all needed XML control characters with their escape code.
 * 
 * @param[in] str - string to escape
 * @param[in] length - maximum length of the input string in bytes
 * @return escaped string or NULL on error
 * @see p_isXmlNeedEscape()
 * @remarks The function may return str if no escaping is needed. Note that the constness changes.
 */
char * p_escapeXml(const char * str, const size_t length) {
	if (str == NULL) return NULL;
	/* calculate the result string size in bytes */
	size_t resSize = 1;
	size_t i = 0;
	for (const char * in = str; *in != 0 && i < length; in++, i++) {
		switch (*in) {
		case '"':  resSize += 6; break;
		case '\'': resSize += 6; break;
		case '<':  resSize += 4; break;
		case '>':  resSize += 4; break;
		case '&':  resSize += 5; break;
		default:   resSize++; break;
		}
	}
	if ((i + 1) == resSize) return (char *)str;
	/* create the result string */
	char * res = (char *)malloc(sizeof(char) * resSize);
	if (res == NULL) return NULL;
#define CASE_APPEND(a, b) case a: memcpy(out, b, sizeof(b) - 1); out += (size_t)(sizeof(b) - 1); break;
	i = 0;
	char * out = res;
	for (const char * in = str; *in != 0 && i < length; in++, i++) {
		switch (*in) {
		CASE_APPEND('"',  "&quot;")
		CASE_APPEND('\'', "&apos;")
		CASE_APPEND('<',  "&lt;"  )
		CASE_APPEND('>',  "&gt;"  )
		CASE_APPEND('&',  "&amp;" )
		default: *out = *in; out++; break;
		}
	}
	*out = 0;
#undef CASE_APPEND
	return res;
}


/**
 * The function escapes all needed XML control characters with their escape code. Frees the pointed
 * variable memory and replaces it with the escaped string.
 * 
 * @param[in,out] var - variable to escape
 * @return 1 on success, else 0
 */
int p_escapeXmlVar(char ** var) {
	if (var == NULL || *var == NULL) return 0;
	char * str = p_escapeXml(*var, (size_t)-1);
	if (str == NULL) return 0;
	if (str != *var) {
		free(*var);
		*var = str;
	}
	return 1;
}


/**
 * The function unescapes all predefined entity characters XML defines with their escape code.
 * An entity map can be passed optionally for escaping. The standard requires that this map contains
 * at least the predefined entity characters. The function sets errno to EINVAL upon wrong escape
 * sequence.
 * 
 * @param[in] str - string to unescape
 * @param[in] length - maximum length of the input string in bytes
 * @param[in] map - optional map for replacement entities (needs to be sorted by p_cmpXmlUnEscMapEntities())
 * @param[in] mapSize - number of entries in map
 * @return unescaped string or NULL on error
 * @see p_isXmlNeedEscape()
 * @remarks The function may return str if no unescaping is needed. Note that the constness changes.
 */
char * p_unescapeXml(const char * str, const size_t length, const tPXmlUnEscMapEntity * map, const size_t mapSize) {
	/* this map needs to be sorted in ascending order by the first field */
	static const tPXmlUnEscMapEntity defMap[] = {
		{"amp",  3, "&" , 1},
		{"apos", 4, "'" , 1},
		{"gt",   2, ">" , 1},
		{"lt",   2, "<" , 1},
		{"quot", 4, "\"", 1}
	};
	static const size_t defMapSize = 5;
	const tPXmlUnEscMapEntity * usedMap = (map != NULL) ? map : defMap;
	const size_t usedMapSize = (map != NULL) ? mapSize : defMapSize;
	if (str == NULL) return NULL;
	/* calculate the result string size in bytes (this also validates the input string) */
	size_t resSize = 1;
	size_t i = 0;
	int oldErrno = errno;
	tPXmlUnEscMapEntity refItem = {0};
	for (const char * in = str; *in != 0 && i < length; in++, i++) {
		switch (*in) {
		case '&':
			refItem.str = in + 1;
			break;
		case ';':
			if (refItem.str != NULL) {
				refItem.strSize = (size_t)(in - refItem.str);
				if (refItem.str[0] == '#') {
					char * endPtr = NULL;
					unsigned long cp = 0;
					errno = 0;
					if (refItem.strSize > 1 && refItem.str[1] == 'x') {
						/* Unicode code point in hex */
						cp = strtoul(refItem.str + 2, &endPtr, 16);
					} else {
						/* Unicode code point in decimal */
						cp = strtoul(refItem.str + 1, &endPtr, 10);
					}
					if (errno == ERANGE || *endPtr != ';' || cp == 0) {
						errno = EINVAL;
						return NULL; /* number overflow or not a number */
					}
					resSize += utf8_codePointeSize((tUChar)cp, UTF8M_REPLACE); /* invalid Unicode code points are replaced */
				} else {
					/* named sequence */
					const tPXmlUnEscMapEntity * item = (const tPXmlUnEscMapEntity *)bs_array(
						&refItem, usedMap, sizeof(tPXmlUnEscMapEntity), usedMapSize, p_cmpXmlUnEscMapEntities
					);
					if (item == NULL) {
						errno = EINVAL;
						return NULL; /* unknown escape sequence */
					}
					resSize += item->replSize;
				}
			} else {
				resSize++;
			}
			refItem.str = NULL;
			break;
		default:
			resSize++;
			break;
		}
	}
	if (refItem.str != NULL) {
		errno = EINVAL;
		return NULL; /* unknown escape sequence */
	}
	if ((i + 1) == resSize) return (char *)str;
	/* create the result string */
	char * res = (char *)malloc(sizeof(char) * resSize);
	if (res == NULL) return NULL;
	i = 0;
	char * out = res;
	for (const char * in = str; *in != 0 && i < length; in++, i++) {
		switch (*in) {
		case '&':
			refItem.str = in + 1;
			break;
		case ';':
			if (refItem.str != NULL) {
				refItem.strSize = (size_t)(in - refItem.str);
				if (refItem.str[0] == '#') {
					char * endPtr = NULL;
					unsigned long cp = 0;
					errno = 0;
					if (refItem.strSize > 1 && refItem.str[1] == 'x') {
						/* Unicode code point in hex */
						cp = strtoul(refItem.str + 2, &endPtr, 16);
					} else {
						/* Unicode code point in decimal */
						cp = strtoul(refItem.str + 1, &endPtr, 10);
					}
					/* invalid Unicode code points are replaced */
					out += utf8_fromCodePoint(out, (size_t)-1, (tUChar)cp, UTF8M_REPLACE);
				} else {
					/* named sequence */
					const tPXmlUnEscMapEntity * item = (const tPXmlUnEscMapEntity *)bs_array(
						&refItem, usedMap, sizeof(tPXmlUnEscMapEntity), usedMapSize, p_cmpXmlUnEscMapEntities
					);
					memcpy(out, item->repl, item->replSize);
					out += item->replSize;
				}
			} else {
				*out = *in;
				out++;
			}
			refItem.str = NULL;
			break;
		default:
			if (refItem.str == NULL) {
				*out = *in;
				out++;
			}
			break;
		}
	}
	*out = 0;
	errno = oldErrno;
	return res;
}


/**
 * The function unescapes all predefined entity characters XML defines with their escape code.
 * An entity map can be passed optionally for escaping. The standard requires that this map contains
 * at least the predefined entity characters. Frees the pointed variable memory and replaces it with
 * the unescaped string. The function sets errno to EINVAL upon wrong escape sequence.
 * 
 * @param[in,out] var - variable to unescape
 * @param[in] map - optional map for replacement entities (needs to be sorted by p_cmpXmlUnEscMapEntities())
 * @param[in] mapSize - number of entries in map
 * @return 1 on success, else 0
 */
int p_unescapeXmlVar(char ** var, const tPXmlUnEscMapEntity * map, const size_t mapSize) {
	if (var == NULL || *var == NULL) return 0;
	char * str = p_unescapeXml(*var, (size_t)-1, map, mapSize);
	if (str == NULL) return 0;
	if (str != *var) {
		free(*var);
		*var = str;
	}
	return 1;
}


/**
 * Sets the full qualified name from the passed namespace and element name to the given variable.
 * 
 * @param[out] out - result output
 * @param[in] parts - input parts (namespace, element name)
 * @return 1 on success, else 0
 */
int p_xmlGetFullName(tPToken * out, const tPToken * parts) {
	if (out == NULL || parts == NULL) return 0;
	*out = parts[1];
	if (parts[0].start != NULL) {
		out->start = parts[0].start;
		out->length += parts[0].length + 1;
	}
	return 1;
}


/**
 * The function escapes all UTF-8 sequence bytes and reserved characters. The errno value EINVAL
 * may be set if the passed string contains an ASCII control character.
 * 
 * @param[in] str - string to escape
 * @param[in] length - maximum length of the input string in bytes
 * @see https://tools.ietf.org/html/rfc3986
 * @see p_isUrlNeedEscape()
 */
char * p_escapeUrl(const char * str, const size_t length) {
	static const char * hexStr = "0123456789ABCDEF";
	if (str == NULL) return NULL;
	/* calculate the result string size in bytes */
	size_t resSize = 1;
	size_t i = 0;
	for (const unsigned char * in = (const unsigned char *)str; *in != 0 && i < length; in++, i++) {
		if (*in < 0x20) {
			errno = EINVAL;
			return NULL;
		} else if (p_isUrlNeedEscape(*in) != 0) {
			resSize += 3;
		} else {
			resSize++;
		}
	}
	if ((i + 1) == resSize) return (char *)str;
	/* create the result string */
	char * res = (char *)malloc(sizeof(char) * resSize);
	if (res == NULL) return NULL;
	i = 0;
	char * out = res;
	for (const unsigned char * in = (const unsigned char *)str; *in != 0 && i < length; in++, i++) {
		if (p_isUrlNeedEscape(*in) != 0) {
			out[0] = '%';
			out[1] = hexStr[(*in >> 4) & 0x0F];
			out[2] = hexStr[*in & 0x0F];
			out += 3;
		} else {
			*out = (char)(*in);
			out++;
		}
	}
	*out = 0;
	return res;
}


/**
 * The function escapes all UTF-8 sequence bytes and reserved characters. The errno value EINVAL
 * may be set if the passed string contains an ASCII control character. Frees the pointed variable
 * memory and replaces it with the escaped string.
 * 
 * @param[in,out] var - variable to escape
 * @return 1 on success, else 0
 */
int p_escapeUrlVar(char ** var) {
	if (var == NULL || *var == NULL) return 0;
	char * str = p_escapeUrl(*var, (size_t)-1);
	if (str == NULL) return 0;
	if (str != *var) {
		free(*var);
		*var = str;
	}
	return 1;
}


/**
 * The function unescapes all percent-encoded URL values. The errno value EINVAL may be set if the
 * passed string or any decoded value contains an ASCII control character.
 * 
 * @param[in] str - string to unescape
 * @param[in] length - maximum length of the input string in bytes
 * @return unescaped string or NULL on error
 * @see p_isUrlNeedEscape()
 * @remarks The function may return str if no unescaping is needed. Note that the constness changes.
 */
char * p_unescapeUrl(const char * str, const size_t length) {
	if (str == NULL) return NULL;
	/* calculate the result string size in bytes */
	size_t resSize = 1;
	size_t i = 0;
	for (const unsigned char * in = (const unsigned char *)str; *in != 0 && i < length; in++, i++) {
		if (*in < 0x20) {
			errno = EINVAL;
			return NULL;
		} else if (*in == '%' && (i + 2) < length && isxdigit(in[1]) != 0 && isxdigit(in[2]) != 0) {
			in += 2;
			i += 2;
		}
		resSize++;
	}
	if ((i + 1) == resSize) return (char *)str;
	/* create the result string */
	char * res = (char *)malloc(sizeof(char) * resSize);
	if (res == NULL) return NULL;
	i = 0;
	char * out = res;
	for (const unsigned char * in = (const unsigned char *)str; *in != 0 && i < length; in++, i++) {
		if (*in == '%' && (i + 2) < length && isxdigit(in[1]) != 0 && isxdigit(in[2]) != 0) {
			const int b1 = toupper(in[1]) - '0';
			const int b2 = toupper(in[2]) - '0';
			const int b = (((b1 > 16) ? (b1 - 7) : b1) << 4) | ((b2 > 16) ? (b2 - 7) : b2);
			if (b < 0x20) {
				free(res);
				errno = EINVAL;
				return NULL;
			}
			*out = (char)b;
			out++;
			in += 2;
			i += 2;
		} else {
			*out = (char)(*in);
			out++;
		}
	}
	*out = 0;
	return res;
}


/**
 * The function unescapes all percent-encoded URL values. The errno value EINVAL may be set if the
 * passed string or any decoded value contains an ASCII control character. Frees the pointed
 * variable memory and replaces it with the escaped string.
 * 
 * @param[in,out] var - variable to unescape
 * @return 1 on success, else 0
 */
int p_unescapeUrlVar(char ** var) {
	if (var == NULL || *var == NULL) return 0;
	char * str = p_unescapeUrl(*var, (size_t)-1);
	if (str == NULL) return 0;
	if (str != *var) {
		free(*var);
		*var = str;
	}
	return 1;
}
