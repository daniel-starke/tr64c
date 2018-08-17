/**
 * @file cvutf8.c
 * @author Daniel Starke
 * @see cvutf8.h
 * @date 2014-05-03
 * @version 2018-08-15
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
#include <string.h>
#include <wchar.h>
#include "target.h"
#include "cvutf8.h"

#ifdef PCF_IS_WIN
#include <windows.h>
#endif


/**
 * The function creates a new wchar_t string based on the passed UTF-8 char string.
 *
 * @param[in] utf8 - UTF-8 encoded input string
 * @return UTF-16 encoded output string or NULL on error
 */
wchar_t * cvutf8_toUtf16(const char * utf8) {
	return cvutf8_toUtf16N(utf8, strlen(utf8));
}


/**
 * The function creates a new wchar_t string based on the passed UTF-8 char string.
 *
 * @param[in] utf8 - UTF-8 encoded input string
 * @param[in] len - UTF-8 string length in number of bytes
 * @return UTF-16 encoded output string or NULL on error
 */
wchar_t * cvutf8_toUtf16N(const char * utf8, const size_t len) {
	wchar_t * utf16;
	size_t utf16Size;
	if (utf8 == (const char *)NULL) {
		return (wchar_t *)NULL;
	}
	if (len == 0) {
		return (wchar_t *)calloc(1, sizeof(wchar_t));
	}
#ifdef PCF_IS_WIN
	utf16Size = (size_t)MultiByteToWideChar(CP_UTF8, 0, utf8, (int)len, 0, 0);
	if (utf16Size == 0xFFFD || utf16Size == 0) {
		return (wchar_t *)NULL;
	}
#else /* ! PCF_IS_WIN */
	utf16Size = mbstowcs(NULL, utf8, len);
	if (utf16Size == ((size_t)-1)) {
		return (wchar_t *)NULL;
	}
#endif /* PCF_IS_WIN */
	utf16 = (wchar_t *)malloc(sizeof(wchar_t) * (utf16Size + 1));
	if (utf16 == (wchar_t *)NULL) {
		return (wchar_t *)NULL;
	}
#ifdef PCF_IS_WIN
	utf16Size = (size_t)MultiByteToWideChar(CP_UTF8, 0, utf8, (int)len, utf16, (int)utf16Size);
#else /* ! PCF_IS_WIN */
	utf16Size = mbstowcs(utf16, utf8, len);
#endif /* PCF_IS_WIN */
	utf16[utf16Size] = 0;
	return utf16;
}


/**
 * The function creates a new char string based on the passed UTF-16 wchar_t string.
 *
 * @param[in] utf16 - UTF-16 encoded input string
 * @return UTF-8 encoded output string or NULL on error
 */
char * cvutf8_fromUtf16(const wchar_t * utf16) {
	return cvutf8_fromUtf16N(utf16, wcslen(utf16));
}


/**
 * The function creates a new char string based on the passed UTF-16 wchar_t string.
 *
 * @param[in] utf16 - UTF-16 encoded input string
 * @param[in] len - UTF-8 string length in number of wchar_t elements
 * @return UTF-8 encoded output string or NULL on error
 */
char * cvutf8_fromUtf16N(const wchar_t * utf16, const size_t len) {
	char * utf8;
	size_t utf8Size;
	if (utf16 == (const wchar_t *)NULL) {
		return (char *)NULL;
	}
	if (len == 0) {
		return (char *)calloc(1, sizeof(char));
	}
#ifdef PCF_IS_WIN
	utf8Size = (size_t)WideCharToMultiByte(CP_UTF8, 0, utf16, (int)len, 0, 0, NULL, NULL);
	if (utf8Size == 0xFFFD || utf8Size == 0) {
		return (char *)NULL;
	}
#else /* ! PCF_IS_WIN */
	utf8Size = wcstombs(NULL, utf16, len);
	if (utf8Size == ((size_t)-1)) {
		return (char *)NULL;
	}
#endif /* PCF_IS_WIN */
	utf8 = (char *)malloc(sizeof(char) * (utf8Size + 1));
	if (utf8 == (char *)NULL) {
		return (char *)NULL;
	}
#ifdef PCF_IS_WIN
	utf8Size = (size_t)WideCharToMultiByte(CP_UTF8, 0, utf16, (int)len, utf8, (int)utf8Size, NULL, NULL);
#else /* ! PCF_IS_WIN */
	utf8Size = wcstombs(utf8, utf16, len);
#endif /* PCF_IS_WIN */
	utf8[utf8Size] = 0;
	return utf8;
}
