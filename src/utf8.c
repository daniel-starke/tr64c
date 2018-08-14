/**
 * @file utf8.c
 * @author Daniel Starke
 * @see utf8.h
 * @date 2018-06-28
 * @version 2018-06-29
 * @see http://www.ietf.org/rfc/rfc3629.txt
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
#include <errno.h>
#include <stddef.h>
#include "utf8.h"

#ifdef PCF_IS_WIN
#include <windows.h>
#endif


/**
 * Returns the number of characters in the given UTF-8 string.
 * The function may set one of the following errno values:
 * - EILSEQ - tried to decode an invalid or incomplete UTF-8 sequence
 * 
 * @param[in] in - input string
 * @param[in] size - limit input string to this many bytes
 * @param[in] mode - processing mode
 * @return number of characters (not bytes)
 */
size_t utf8_length(const char * in, const size_t size, const tUtf8Mode mode) {
	if (in == NULL) return 0;
	size_t n = 0, bytes = 0;
	size_t res = 0;
	for (const char * ptr = in; *ptr != 0 && n < size; ptr += bytes, n += bytes) {
		const tUChar cp = utf8_toCodePoint(ptr, size - n, &bytes, mode);
		if (cp == 0 && bytes == 0) break;
		if (cp > 0) res++;
	}
	return res;
}


/**
 * Returns the number of bytes the given Unicode code point takes up if encoded with UTF-8.
 * The function may set one of the following errno values:
 * - EINVAL - invalid code point given
 * 
 * @param[in] cp - Unicode code point
 * @param[in] mode - processing mode
 * @return number of bytes in UTF-8
 */
size_t utf8_codePointeSize(const tUChar cp, const tUtf8Mode mode) {
	if (cp < 0x80) {
		return 1;
	} else if (cp < 0x800) {
		return 2;
	} else if (cp < 0xD800) {
		return 3;
	} else if (cp < 0xE000) {
		/* surrogate pairs -> invalid in UTF-8 */
		errno = EINVAL;
		if (mode == UTF8M_REPLACE) return 3;
		return 0;
	} else if (cp < 0x10000) {
		return 3;
	} else if (cp < 0x110000) {
		return 4;
	}
	errno = EINVAL;
	if (mode == UTF8M_REPLACE) return 3;
	return 0;
}


/**
 * Encodes the given Unicode code point as UTF-8 sequence.
 * The function may set one of the following errno values:
 * - EINVAL - invalid code point given
 * 
 * @param[out] out - write result to this buffer
 * @param[in] size - maximum size of out in bytes
 * @param[in] cp - Unicode code point
 * @param[in] mode - processing mode
 * @return number of written bytes
 */
size_t utf8_fromCodePoint(char * out, const size_t size, const tUChar cp, const tUtf8Mode mode) {
	if (out == NULL || size <= 0) return 0;
	if (cp < 0x80) {
		out[0] = (char)cp;
		return 1;
	} else if (cp < 0x800) {
		if (size < 2) goto onError;
		out[0] = (char)(0xC0 | (cp >> 6));
		out[1] = (char)(0x80 | (cp & 0x3F));
		return 2;
	} else if (cp < 0xD800) {
		if (size < 3) goto onError;
		out[0] = (char)(0xE0 | (cp >> 12));
		out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
		out[2] = (char)(0x80 | (cp & 0x3F));
		return 3;
	} else if (cp < 0xE000) {
		/* surrogate pairs -> invalid in UTF-8 */
	} else if (cp < 0x10000) {
		if (size < 3) goto onError;
		out[0] = (char)(0xE0 | (cp >> 12));
		out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
		out[2] = (char)(0x80 | (cp & 0x3F));
		return 3;
	} else if (cp < 0x110000) {
		if (size < 4) goto onError;
		out[0] = (char)(0xF0 | (cp >> 18));
		out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
		out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
		out[3] = (char)(0x80 | (cp & 0x3F));
		return 4;
	}
onError:
	errno = EINVAL;
	if (size >= 3 && mode == UTF8M_REPLACE) {
		out[0] = (char)0xEF;
		out[1] = (char)0xBF;
		out[2] = (char)0xBD;
		return 3;
	}
	return 0;
}


/**
 * Decodes the given UTF-8 sequence to a single Unicode code point.
 * The function may set one of the following errno values:
 * - EILSEQ - tried to decode an invalid or incomplete UTF-8 sequence
 * 
 * @param[in] in - input UTF-8 sequence
 * @param[in] size - read at most this many bytes
 * @param[out] len - optionally set this pointer to retrieve the number of bytes processed
 * @param[in] mode - processing mode
 * @return returns the decoded Unicode code point or 0 on error 
 */
tUChar utf8_toCodePoint(const char * in, const size_t size, size_t * len, const tUtf8Mode mode) {
#define DBG fprintf(stderr, "%s:%i\n", __func__, __LINE__); fflush(stderr);
	size_t n = 0;
	if (in == NULL || size <= 0 || *in == 0) goto onIncomplete;
	const unsigned char * ptr = (const unsigned char *)in;
onOutOfSync:
	if (*ptr < 0x80) {
		/* one byte sequence */
		if (len != NULL) *len = n + 1;
		return *ptr;
	} else if (*ptr < 0xC0) {
		/* out of sync */
		ptr++;
		n++;
		if (n < size) goto onOutOfSync;
	} else if (*ptr < 0xC2) {
		/* invalid start byte */
		n++;
	} else if (*ptr < 0xE0) {
		/* two bytes sequence */
		if ((n + 1) >= size) goto onIncomplete;
		n += 2;
		if ((ptr[1] & 0xC0) != 0x80) goto onInvalid;
		const tUChar cp = (tUChar)(((ptr[0] & 0x1F) << 6) | (ptr[1] & 0x3F));
		if (cp < 0x80) goto onInvalid;
		if (len != NULL) *len = n;
		return cp;
	} else if (*ptr < 0xF0) {
		/* three bytes sequence */
		if ((n + 2) >= size) goto onIncomplete;
		n += 3;
		if ((ptr[1] & 0xC0) != 0x80 || (ptr[2] & 0xC0) != 0x80) goto onInvalid;
		const tUChar cp = (tUChar)(((ptr[0] & 0x0F) << 12) | ((ptr[1] & 0x3F) << 6) | (ptr[2] & 0x3F));
		if (cp < 0x800 || (cp >= 0xD800 && cp < 0xE000)) goto onInvalid;
		if (len != NULL) *len = n;
		return cp;
	} else if (*ptr < 0xF5) {
		/* four bytes sequence */
		if ((n + 3) >= size) goto onIncomplete;
		n += 4;
		if ((ptr[1] & 0xC0) != 0x80 || (ptr[2] & 0xC0) != 0x80 || (ptr[3] & 0xC0) != 0x80) goto onInvalid;
		const tUChar cp = (tUChar)(((ptr[0] & 0x07) << 18) | ((ptr[1] & 0x3F) << 12) | ((ptr[2] & 0x3F) << 6) | (ptr[3] & 0x3F));
		if (cp < 0x10000 || cp >= 0x110000) goto onInvalid;
		if (len != NULL) *len = n;
		return cp;
	}
onInvalid:
	/* invalid sequence */
	errno = EILSEQ;
	if (len != NULL) *len = n;
	if (mode == UTF8M_REPLACE) return 0xFFFD;
	return 0;
onIncomplete:
	/* incomplete sequence */
	errno = EILSEQ;
	if (len != NULL) *len = n;
	return 0;
}
