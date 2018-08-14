/**
 * @file argp.h
 * @author Daniel Starke
 * @see argps.h
 * @see argpous.h
 * @date 2017-05-18
 * @version 2017-05-18
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
#ifndef __LIBPCF_ARGP_H__
#define __LIBPCF_ARGP_H__

#include <stdio.h>
#include "target.h"


#ifdef __cplusplus
extern "C" {
#endif


#ifndef no_argument
#define no_argument 0
#endif
#ifndef required_argument
#define required_argument 1
#endif
#ifndef optional_argument
#define optional_argument 2
#endif


/**
 * Argument parser processing flags.
 */
typedef enum tArgPFlag {
	ARGP_SHORT           = 0x01, /**< enables short options */
	ARGP_LONG            = 0x02, /**< enables long options */
	ARGP_POSIXLY_CORRECT = 0x04, /**< enables POSIX correctness regarding reordering */
	ARGP_FORWARD_ERRORS  = 0x08, /**< do not print parsing errors to standard error */
	ARGP_ARG_ONE         = 0x10, /**< handle each non-option argument as for option 1 */
	ARGP_GNU_SHORT       = 0x20, /**< enable GNU extension for short option arguments */
	ARGP_GNU_W           = 0x40, /**< enable GNU extension for long names with -W */
} tArgPFlag;


/**
 * Argument parser processing states.
 */
typedef enum tArgPState {
	APST_START,             /**< initial state */
	APST_NEXT,              /**< process next option */
	APST_SHORT,             /**< process short option */
	APST_LONG,              /**< process long option */
	APST_ARG,               /**< process argument */
	APST_END,               /**< terminate argument parsing */
	APST_ERROR_NON_OPT,     /**< error: non-option argument */
	APST_ERROR_MISSING_ARG, /**< error: missing required argument */
	APST_ERROR_INVALID_OPT, /**< error: invalid option */
	APST_ERROR_AMBIGUOUS,   /**< error: ambiguous long option */
	APST_ERROR_NULL_PTR,    /**< error: null pointer */
} tArgPState;


#ifdef __cplusplus
}
#endif


#endif /* __LIBPCF_ARGP_H__ */
