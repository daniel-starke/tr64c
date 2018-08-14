/**
 * @file hmd5.c
 * @author Daniel Starke
 * @see hmd5.h
 * @date 2010-02-12
 * @version 2016-05-01
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
#include <stdlib.h>
#include "target.h"
#include "hmd5.h"


#define MD5_FF(b, c, d) (d ^ (b & (c ^ d)))
#define MD5_FG(b, c, d) MD5_FF(d, b, c)
#define MD5_FH(b, c, d) (b ^ c ^ d)
#define MD5_FI(b, c, d) (c ^ (b | ~d))
#define MD5_ROL(w, s) (w = (w << s) | (w >> (32 - s)))


/**
 * @internal
 */
static const uint8_t hMd5FillBuffer[64] = { 0x80, 0 /* , 0, 0, ...  */  };


/**
 * The function initializes the passed context. The can also be
 * used to reset an old context.
 * 
 * @param[out] handle - memory to store internal states for the hashing
 */
void h_initMd5(void * handle) {
	tHMd5Ctx * ctx = (tHMd5Ctx *)handle;
	ctx->state[0] = UINT32_C(0x67452301);
	ctx->state[1] = UINT32_C(0xEFCDAB89);
	ctx->state[2] = UINT32_C(0x98BADCFE);
	ctx->state[3] = UINT32_C(0x10325476);
	ctx->total[0] = 0;
	ctx->total[1] = 0;
	ctx->buflen = 0;
}


/**
 * The function initializes a context and returns it. If the
 * function fails the return value is NULL.
 * 
 * @return handle for the associated hashing functions
 */
void * h_initNewMd5() {
	tHMd5Ctx * ctx;
	ctx = (tHMd5Ctx *)malloc(sizeof(tHMd5Ctx));
	if (ctx == NULL) return NULL;
	h_initMd5((void *)ctx);
	return (void *)ctx;
}


/**
 * The function is for internal usage only. It is needed
 * to process the rounds for the MD5 hash.
 *
 * @param[in,out] ctx - the hash context
 * @param[in] buffer - a pointer to a memory block to process
 * @param[in] len - the length of the memory block to process
 * @internal
 */
static void h_processMd5(tHMd5Ctx * ctx, const uint8_t * buffer, const size_t len) {
	uint32_t correct_words[16];
	const uint32_t * words = (uint32_t *)buffer;
	const uint32_t * endp = words + (len / sizeof(uint32_t));
	register uint32_t A = ctx->state[0];
	register uint32_t B = ctx->state[1];
	register uint32_t C = ctx->state[2];
	register uint32_t D = ctx->state[3];

	ctx->total[0] += (uint32_t)len;
	if (ctx->total[0] < (uint32_t)len) {
		++ctx->total[1];
	}

	while (words < endp) {
		uint32_t *cwp = correct_words;
		uint32_t A_save = A;
		uint32_t B_save = B;
		uint32_t C_save = C;
		uint32_t D_save = D;

		#define MD5_OP(a, b, c, d, s, T) \
			do { \
				a += MD5_FF(b, c, d) + (*cwp++ = (*words)) + T; \
				++words; \
				MD5_ROL(a, s); \
				a += b; \
			}  while (0)

		MD5_OP(A, B, C, D,  7, UINT32_C(0xD76AA478));
		MD5_OP(D, A, B, C, 12, UINT32_C(0xE8C7B756));
		MD5_OP(C, D, A, B, 17, UINT32_C(0x242070DB));
		MD5_OP(B, C, D, A, 22, UINT32_C(0xC1BDCEEE));
		MD5_OP(A, B, C, D,  7, UINT32_C(0xF57C0FAF));
		MD5_OP(D, A, B, C, 12, UINT32_C(0x4787C62A));
		MD5_OP(C, D, A, B, 17, UINT32_C(0xA8304613));
		MD5_OP(B, C, D, A, 22, UINT32_C(0xFD469501));
		MD5_OP(A, B, C, D,  7, UINT32_C(0x698098D8));
		MD5_OP(D, A, B, C, 12, UINT32_C(0x8B44F7AF));
		MD5_OP(C, D, A, B, 17, UINT32_C(0xFFFF5BB1));
		MD5_OP(B, C, D, A, 22, UINT32_C(0x895CD7BE));
		MD5_OP(A, B, C, D,  7, UINT32_C(0x6B901122));
		MD5_OP(D, A, B, C, 12, UINT32_C(0xFD987193));
		MD5_OP(C, D, A, B, 17, UINT32_C(0xA679438E));
		MD5_OP(B, C, D, A, 22, UINT32_C(0x49B40821));

		#undef MD5_OP
		#define MD5_OP(f, a, b, c, d, k, s, T) \
			do { \
				a += f(b, c, d) + correct_words[k] + T; \
				MD5_ROL(a, s); \
				a += b; \
			} while (0)

		MD5_OP(MD5_FG, A, B, C, D,  1,  5, UINT32_C(0xF61E2562));
		MD5_OP(MD5_FG, D, A, B, C,  6,  9, UINT32_C(0xC040B340));
		MD5_OP(MD5_FG, C, D, A, B, 11, 14, UINT32_C(0x265E5A51));
		MD5_OP(MD5_FG, B, C, D, A,  0, 20, UINT32_C(0xE9B6C7AA));
		MD5_OP(MD5_FG, A, B, C, D,  5,  5, UINT32_C(0xD62F105D));
		MD5_OP(MD5_FG, D, A, B, C, 10,  9, UINT32_C(0x02441453));
		MD5_OP(MD5_FG, C, D, A, B, 15, 14, UINT32_C(0xD8A1E681));
		MD5_OP(MD5_FG, B, C, D, A,  4, 20, UINT32_C(0xE7D3FBC8));
		MD5_OP(MD5_FG, A, B, C, D,  9,  5, UINT32_C(0x21E1CDE6));
		MD5_OP(MD5_FG, D, A, B, C, 14,  9, UINT32_C(0xC33707D6));
		MD5_OP(MD5_FG, C, D, A, B,  3, 14, UINT32_C(0xF4D50D87));
		MD5_OP(MD5_FG, B, C, D, A,  8, 20, UINT32_C(0x455A14ED));
		MD5_OP(MD5_FG, A, B, C, D, 13,  5, UINT32_C(0xA9E3E905));
		MD5_OP(MD5_FG, D, A, B, C,  2,  9, UINT32_C(0xFCEFA3F8));
		MD5_OP(MD5_FG, C, D, A, B,  7, 14, UINT32_C(0x676F02D9));
		MD5_OP(MD5_FG, B, C, D, A, 12, 20, UINT32_C(0x8D2A4C8A));
		MD5_OP(MD5_FH, A, B, C, D,  5,  4, UINT32_C(0xFFFA3942));
		MD5_OP(MD5_FH, D, A, B, C,  8, 11, UINT32_C(0x8771F681));
		MD5_OP(MD5_FH, C, D, A, B, 11, 16, UINT32_C(0x6D9D6122));
		MD5_OP(MD5_FH, B, C, D, A, 14, 23, UINT32_C(0xFDE5380C));
		MD5_OP(MD5_FH, A, B, C, D,  1,  4, UINT32_C(0xA4BEEA44));
		MD5_OP(MD5_FH, D, A, B, C,  4, 11, UINT32_C(0x4BDECFA9));
		MD5_OP(MD5_FH, C, D, A, B,  7, 16, UINT32_C(0xF6BB4B60));
		MD5_OP(MD5_FH, B, C, D, A, 10, 23, UINT32_C(0xBEBFBC70));
		MD5_OP(MD5_FH, A, B, C, D, 13,  4, UINT32_C(0x289B7EC6));
		MD5_OP(MD5_FH, D, A, B, C,  0, 11, UINT32_C(0xEAA127FA));
		MD5_OP(MD5_FH, C, D, A, B,  3, 16, UINT32_C(0xD4EF3085));
		MD5_OP(MD5_FH, B, C, D, A,  6, 23, UINT32_C(0x04881D05));
		MD5_OP(MD5_FH, A, B, C, D,  9,  4, UINT32_C(0xD9D4D039));
		MD5_OP(MD5_FH, D, A, B, C, 12, 11, UINT32_C(0xE6DB99E5));
		MD5_OP(MD5_FH, C, D, A, B, 15, 16, UINT32_C(0x1FA27CF8));
		MD5_OP(MD5_FH, B, C, D, A,  2, 23, UINT32_C(0xC4AC5665));
		MD5_OP(MD5_FI, A, B, C, D,  0,  6, UINT32_C(0xF4292244));
		MD5_OP(MD5_FI, D, A, B, C,  7, 10, UINT32_C(0x432AFF97));
		MD5_OP(MD5_FI, C, D, A, B, 14, 15, UINT32_C(0xAB9423A7));
		MD5_OP(MD5_FI, B, C, D, A,  5, 21, UINT32_C(0xFC93A039));
		MD5_OP(MD5_FI, A, B, C, D, 12,  6, UINT32_C(0x655B59C3));
		MD5_OP(MD5_FI, D, A, B, C,  3, 10, UINT32_C(0x8F0CCC92));
		MD5_OP(MD5_FI, C, D, A, B, 10, 15, UINT32_C(0xFFEFF47D));
		MD5_OP(MD5_FI, B, C, D, A,  1, 21, UINT32_C(0x85845DD1));
		MD5_OP(MD5_FI, A, B, C, D,  8,  6, UINT32_C(0x6FA87E4F));
		MD5_OP(MD5_FI, D, A, B, C, 15, 10, UINT32_C(0xFE2CE6E0));
		MD5_OP(MD5_FI, C, D, A, B,  6, 15, UINT32_C(0xA3014314));
		MD5_OP(MD5_FI, B, C, D, A, 13, 21, UINT32_C(0x4E0811A1));
		MD5_OP(MD5_FI, A, B, C, D,  4,  6, UINT32_C(0xF7537E82));
		MD5_OP(MD5_FI, D, A, B, C, 11, 10, UINT32_C(0xBD3AF235));
		MD5_OP(MD5_FI, C, D, A, B,  2, 15, UINT32_C(0x2AD7D2BB));
		MD5_OP(MD5_FI, B, C, D, A,  9, 21, UINT32_C(0xEB86D391));

		A += A_save;
		B += B_save;
		C += C_save;
		D += D_save;
	}

	ctx->state[0] = A;
	ctx->state[1] = B;
	ctx->state[2] = C;
	ctx->state[3] = D;
}


/**
 * The function processes a memory block of the specific size
 * to calculate the hash.
 *
 * @param[in,out] handle - the hash context
 * @param[in] buffer - a pointer to a memory block to process
 * @param[in] len - the length of the memory block to process
 * @remarks The passed pointers are not checked against NULL.
 */
void h_updateMd5(void * handle, const uint8_t * buffer, const size_t len) {
	tHMd5Ctx * ctx = (tHMd5Ctx *)handle;
	size_t bufLen = len;
	if (ctx->buflen != 0) {
		size_t left_over = ctx->buflen;
		size_t add = 128 > bufLen + left_over ? bufLen : (size_t)(128 - left_over);

		memcpy(&((char *)ctx->buffer)[left_over], (char *)buffer, add);
		ctx->buflen += (uint32_t)add;

		if (ctx->buflen > 64) {
			h_processMd5(ctx, (uint8_t *)ctx->buffer, ctx->buflen & (uint32_t)(~63));

			ctx->buflen &= 63;
			memcpy((char *)ctx->buffer, &((char *)ctx->buffer)[(left_over + add) & (size_t)(~63)], ctx->buflen);
		}

		buffer = buffer + add;
		bufLen = (size_t)(bufLen - add);
	}

	if (bufLen >= 64) {
#if !_STRING_ARCH_unaligned
		if (PCF_UNALIGNED_P32(buffer)) {
			while (bufLen > 64) {
				h_processMd5(ctx, (uint8_t *)memcpy((char *)ctx->buffer, (char *)buffer, 64), 64);
				buffer = buffer + 64;
				bufLen = (size_t)(bufLen - 64);
			}
		} else
#endif /* _STRING_ARCH_unaligned */
		{
			h_processMd5(ctx, buffer, bufLen & (size_t)(~63));
			buffer = buffer + (bufLen & (size_t)(~63));
			bufLen &= 63;
		}
	}

	if (bufLen > 0) {
		size_t left_over = ctx->buflen;

		memcpy(&((char *)ctx->buffer)[left_over], (char *)buffer, bufLen);
		left_over += bufLen;
		if (left_over >= 64) {
			h_processMd5(ctx, (uint8_t *)ctx->buffer, 64);
			left_over = (size_t)(left_over - 64);
			memcpy((char *)ctx->buffer, (char *)&ctx->buffer[16], left_over);
		}
		ctx->buflen = (uint32_t)left_over;
	}
}


/**
 * The function stores the final result of the hash in the
 * memory block pointed to by result.
 * The passed result memory must be able to hold a 128 bit
 * value. This function can be called only once for each hash.
 *
 * @param[in] handle - the hash context
 * @param[out] result - pointer to the memory block to store the result
 * @remarks The passed pointers are not checked against NULL.
 */
void h_finalMd5(void * handle, uint8_t * result) {
	tHMd5Ctx * ctx = (tHMd5Ctx *)handle;
	uint32_t bytes = ctx->buflen;
	size_t size = (bytes < 56) ? 64 / 4 : 64 * 2 / 4;

	ctx->total[0] += bytes;
	if (ctx->total[0] < bytes) {
		++ctx->total[1];
	}

	ctx->buffer[size - 2] = ctx->total[0] << 3;
	ctx->buffer[size - 1] = (ctx->total[1] << 3) | (ctx->total[0] >> 29);

	memcpy(&((char *)ctx->buffer)[bytes], (char *)hMd5FillBuffer, (size - 2) * 4 - bytes);

	h_processMd5(ctx, (uint8_t *)ctx->buffer, size * 4);

	memcpy((char *)result, (char *)&ctx->state, 16);
}


/**
 * The function stores the final result of the hash in the
 * memory block pointed to by result and frees the handle.
 * The passed result memory must be able to hold a 128 bit
 * value.
 *
 * @param[in] handle - the hash context
 * @param[out] result - pointer to the memory block to store the result
 * @remarks The passed pointers are not checked against NULL.
 */
void h_finalFreeMd5(void * handle, uint8_t * result) {
	h_finalMd5(handle, result);
	free(handle);
}
