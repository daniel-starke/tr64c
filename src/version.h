/**
 * @file version.h
 * @author Daniel Starke
 * @date 2018-07-13
 * @version 2018-08-17
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

#define PROGRAM_VERSION 1,1,0,0

#if defined(BACKEND_WINSOCKS)
#define BACKEND_STR "WinSocks"
#define PROGRAM_VERSION_STR "1.1.0 2018-08-17 WinSocks"
#elif defined(BACKEND_POSIX)
#define BACKEND_STR "POSIX"
#define PROGRAM_VERSION_STR "1.1.0 2018-08-17 POSIX"
#else
#error "Unsupported backend."
#endif

