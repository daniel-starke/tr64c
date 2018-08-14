/**
 * @file argpus.c
 * @author Daniel Starke
 * @see argpus.h
 * @date 2017-05-22
 * @version 2017-05-22
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
#include <stdint.h>
#include <string.h>
#include <wchar.h>
#include "argpus.h"


#define ARGP_FUNC argpus_parse
#define ARGP_CTX tArgPUS
#define ARGP_LOPT tArgPEUS
#define ARGP_UNICODE


/* include template function */
#include "argp.i"
