/**
 * @file getopt.c
 * @author Daniel Starke
 * @see getopt.h
 * @date 2017-05-22
 * @version 2017-05-25
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
#include "getopt.h"


int argps_optind = 1;
int argps_opterr = 1;
int argps_optopt = '?';
char * argps_optarg = NULL;
static tArgPS argps_ctx = { 0 };


static int argps_internalGetopt(int argc, char * const argv[], const char * optstring, const tArgPES * longopts, int * longindex) {
	argps_ctx.i = argps_optind;
	argps_ctx.shortOpts = optstring;
	argps_ctx.longOpts = longopts;
	const int result = argps_parse(&argps_ctx, argc, argv);
	argps_optind = argps_ctx.i;
	argps_optopt = argps_ctx.opt;
	argps_optarg = (char *)argps_ctx.arg;
	if (longindex != NULL) *longindex = argps_ctx.longMatch;
	return result;
}


int argps_getopt(int argc, char * const argv[], const char * optstring) {
	argps_ctx.flags = (tArgPFlag)(
		(size_t)(argps_ctx.flags | ARGP_SHORT | ARGP_GNU_SHORT | ((argps_opterr == 0) ? ARGP_FORWARD_ERRORS : 0))
		& (size_t)(ARGP_LONG)
	);
	return argps_internalGetopt(argc, argv, optstring, NULL, NULL);
}


int argps_getoptLong(int argc, char * const argv[], const char * optstring, const tArgPES * longopts, int * longindex) {
	argps_ctx.flags = (tArgPFlag)(argps_ctx.flags | ARGP_SHORT | ARGP_LONG | ARGP_GNU_SHORT | ((argps_opterr == 0) ? ARGP_FORWARD_ERRORS : 0));
	return argps_internalGetopt(argc, argv, optstring, longopts, longindex);
}


int argps_getoptLongOnly(int argc, char * const argv[], const char * optstring, const tArgPES * longopts, int * longindex) {
	argps_ctx.flags = (tArgPFlag)(
		(size_t)(argps_ctx.flags | ARGP_LONG | ((argps_opterr == 0) ? ARGP_FORWARD_ERRORS : 0))
		& (size_t)(ARGP_SHORT)
	);
	return argps_internalGetopt(argc, argv, optstring, longopts, longindex);
}


int argpus_optind = 1;
int argpus_opterr = 1;
int argpus_optopt = '?';
wchar_t * argpus_optarg = NULL;
static tArgPUS argpus_ctx = { 0 };


static int argpus_internalGetopt(int argc, wchar_t * const argv[], const wchar_t * optstring, const tArgPEUS * longopts, int * longindex) {
	argpus_ctx.i = argpus_optind;
	argpus_ctx.shortOpts = optstring;
	argpus_ctx.longOpts = longopts;
	const int result = argpus_parse(&argpus_ctx, argc, argv);
	argpus_optind = argpus_ctx.i;
	argpus_optopt = argpus_ctx.opt;
	argpus_optarg = (wchar_t *)argpus_ctx.arg;
	if (longindex != NULL) *longindex = argpus_ctx.longMatch;
	return result;
}


int argpus_getopt(int argc, wchar_t * const argv[], const wchar_t * optstring) {
	argpus_ctx.flags = (tArgPFlag)(
		(size_t)(argps_ctx.flags | ARGP_SHORT | ARGP_GNU_SHORT | ((argps_opterr == 0) ? ARGP_FORWARD_ERRORS : 0))
		& (size_t)(ARGP_LONG)
	);
	return argpus_internalGetopt(argc, argv, optstring, NULL, NULL);
}


int argpus_getoptLong(int argc, wchar_t * const argv[], const wchar_t * optstring, const tArgPEUS * longopts, int * longindex) {
	argpus_ctx.flags = (tArgPFlag)(argps_ctx.flags | ARGP_SHORT | ARGP_LONG | ARGP_GNU_SHORT | ((argps_opterr == 0) ? ARGP_FORWARD_ERRORS : 0));
	return argpus_internalGetopt(argc, argv, optstring, longopts, longindex);
}


int argpus_getoptLongOnly(int argc, wchar_t * const argv[], const wchar_t * optstring, const tArgPEUS * longopts, int * longindex) {
	argpus_ctx.flags = (tArgPFlag)(
		(size_t)(argps_ctx.flags | ARGP_LONG | ((argps_opterr == 0) ? ARGP_FORWARD_ERRORS : 0))
		& (size_t)(ARGP_SHORT)
	);
	return argpus_internalGetopt(argc, argv, optstring, longopts, longindex);
}
