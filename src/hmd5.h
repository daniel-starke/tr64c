/**
 * @file hmd5.h
 * @author Daniel Starke
 * @see hmd5.c
 * @date 2010-02-12
 * @version 2018-08-14
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
#ifndef __LIBPCF_HMD5_H__
#define __LIBPCF_HMD5_H__

#include <string.h>


#ifdef __cplusplus
extern "C" {
#endif


/**
 * The defined structure is for internal usage only.
 * It defines context information for the hash function.
 */
typedef struct tHMd5Ctx {
	uint32_t state[4]; /**< intermediate digest state */
	uint32_t total[2]; /**< size of processed data */
	uint32_t buflen; /**< amount of data in buffer */
	uint32_t buffer[32]; /**< data block being processed */
} tHMd5Ctx;


void   h_initMd5(void * handle);
void * h_initNewMd5();
void   h_updateMd5(void * handle, const uint8_t * buffer, const size_t len);
void   h_finalMd5(void * handle, uint8_t * result);
void   h_finalFreeMd5(void * handle, uint8_t * result);


#ifdef __cplusplus
}
#endif


#endif /* __LIBPCF_HMD5_H__ */
