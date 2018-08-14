/**
 * @file cvutf8.h
 * @author Daniel Starke
 * @see cvutf8.c
 * @date 2014-05-03
 * @version 2018-07-14
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
#ifndef __LIBPCF_CVUTF8_H__
#define __LIBPCF_CVUTF8_H__

#include <wchar.h>


#ifdef __cplusplus
extern "C" {
#endif


wchar_t * cvutf8_toUtf16(const char * utf8);
wchar_t * cvutf8_toUtf16N(const char * utf8, const size_t len);
char * cvutf8_fromUtf16(const wchar_t * utf16);
char * cvutf8_fromUtf16N(const wchar_t * utf16, const size_t len);


#ifdef __cplusplus
}
#endif


#endif /* __LIBPCF_CVUTF8_H__ */
