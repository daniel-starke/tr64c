/**
 * @file argps.h
 * @author Daniel Starke
 * @see argp.h
 * @see ARGP_FUNC()
 * @date 2017-05-18
 * @version 2017-05-28
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
#ifndef __LIBPCF_ARGPS_H__
#define __LIBPCF_ARGPS_H__

#include "target.h"
#include "argp.h"


#ifdef __cplusplus
extern "C" {
#endif


/**
 * Defines the type of a single long option list element.
 */
typedef struct tArgPES {
	const char * name;
	int has_arg;
	int * flag;
	int val;
} tArgPES;


/**
 * Defines the argument parser context.
 * All fields are expected to be set 0 on the initial state, except flags, shortOpts and longOpts.
 */
typedef struct tArgPS {
	int i; /**< next argument list index (may be changed by user) */
	int nextI; /**< argument list index to next character to be parsed */
	int lastOpt; /**< index to the previous option (for reordering) */
	int opt; /**< erroneous option */
	tArgPFlag flags; /**< see tArgPFlag */
	const char * arg; /**< next argument to be parsed */
	const char * next; /**< next character to be parsed */
	const char * shortOpts; /**< short options */
	const tArgPES * longOpts; /**< long option list */
	int longMatch; /**< long option list index on match, else <0 */
	tArgPState state; /**< internal argument parser state (starts with 0) */
} tArgPS;


/** @copydoc ARGP_FUNC() */
int argps_parse(tArgPS * o, int argc, char * const * argv);


#ifdef __cplusplus
}
#endif


#endif /* __LIBPCF_ARGPS_H__ */
