/**
 * @file utf8.h
 * @author Daniel Starke
 * @see utf8.c
 * @date 2018-06-28
 * @version 2018-06-29
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
#ifndef __LIBPCF_UTF8_H__
#define __LIBPCF_UTF8_H__

#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif


/**
 * Defines a single Unicode code point value or character.
 */
typedef uint_least32_t tUChar;


/**
 * Possible UTF-8 processing modes.
 */
typedef enum {
	UTF8M_IGNORE, /**< ignore invalid characters */
	UTF8M_REPLACE /**< replace invalid characters with the replacement character */
} tUtf8Mode;


size_t utf8_length(const char * in, const size_t size, const tUtf8Mode mode);
size_t utf8_codePointeSize(const tUChar cp, const tUtf8Mode mode);
size_t utf8_fromCodePoint(char * out, const size_t size, const tUChar cp, const tUtf8Mode mode);
tUChar utf8_toCodePoint(const char * in, const size_t size, size_t * len, const tUtf8Mode mode);


#ifdef __cplusplus
}
#endif


#endif /* __LIBPCF_UTF8_H__ */
