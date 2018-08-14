/**
 * @file argp.i
 * @author Daniel Starke
 * @see argps.h
 * @see argpus.h
 * @date 2017-05-18
 * @version 2017-06-03
 * @internal This file is never used or compiled directly but only included.
 * @remarks Define ARGP_UNICODE for the Unicode before including this file.
 * Defaults to ASCII.
 * @remarks See ARGP_FUNC() or further notes.
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
#include <stdio.h>
#include <string.h>
#include "target.h"


/**
 * @fn int ARGP_FUNC(ARGP_CTX * o, int argc, CHAR_T * const * argv)
 * Implements a getopt argument parser according to the POSIX and GNU usage description.
 * The functions works in the following way (simplified):@n
 * @image html ARGP_FUNC.svg
 * 
 * @param[in,out] o - argument parser instance handle
 * @param[in] argc - number of arguments in argv
 * @param[in] argv - argument list to parse (may be modified)
 * @return -1 on end of argument list or parsing error (check errno and o->opt)
 * @remarks Define ARGP_FUNC for name of the function.
 * @remarks Define ARGP_CTX for the context structure type.
 * @remarks Define ARGP_LOPT for the long option list element type.
 * @remarks errno is set to 0 if there was no error or to EFAULT or EINVAL on internal error.
 * @see https://linux.die.net/man/3/getopt
 */


#ifdef ARGP_UNICODE
#define CHAR_T wchar_t
#define INT_T wchar_t
#ifndef TCHAR_STLEN
#define TCHAR_STLEN wcslen
#endif /* TCHAR_STLEN */
#ifndef TCHAR_STCHR
#define TCHAR_STCHR(str, c) wcschr((str), (wchar_t)(c))
#endif /* TCHAR_STCHR */
#ifndef TCHAR_ERROR
#define TCHAR_ERROR(fmt, ...) fwprintf(stderr, L##fmt, __VA_ARGS__)
#endif /* TCHAR_ERROR */
#else /* ! ARGP_UNICODE */
#define CHAR_T char
#define INT_T int
#ifndef TCHAR_STLEN
#define TCHAR_STLEN strlen
#endif /* TCHAR_STLEN */
#ifndef TCHAR_STCHR
#define TCHAR_STCHR(str, c) strchr((str), (int)(c))
#endif /* TCHAR_STCHR */
#ifndef TCHAR_ERROR
#define TCHAR_ERROR(fmt, ...) fprintf(stderr, fmt, __VA_ARGS__)
#endif /* TCHAR_ERROR */
#endif /* ARGP_UNICODE */


/**
 * Checks if the given string starts with the passed prefix.
 * The prefix needs to contain at least one character.
 * The prefix is terminated by a null character or '='.
 * 
 * @param[in] p - prefix
 * @param[in] s - string
 * @return pointer to the end of the prefix (after '=') or NULL on mismatch
 */
static const CHAR_T * matchPrefixStr(const CHAR_T * p, const CHAR_T * s) {
	if (*p == 0 || *p != *s) return NULL;
	for (; *p == *s && *p != 0 && *p != '=' && *s != 0; p++, s++);
	if (*p == '=') return ++p;
	if (*p == 0) return p;
	return NULL;
}


/**
 * Moves the last option to the front, just next to the previous last option.
 * Also updates the last option index.
 * 
 * @param[in,out] o - argument parser instance handle
 * @param[in] argc - number of argument list elements
 * @param[in] argv - argument list
 */
static void moveOptToFront(ARGP_CTX * o, int argc, CHAR_T ** argv) {
	int i = PCF_MAX(o->lastOpt, 1);
	o->lastOpt = o->i;
	if ((o->flags & ARGP_POSIXLY_CORRECT) != 0 || i >= argc) return;
	for (; i > 1 && argv[i - 1][0] != '-'; i--) {
		CHAR_T * tmp = argv[i - 1];
		argv[i - 1] = argv[i];
		argv[i] = tmp;
	}
}



/* goto propagates only downwards the code */
int ARGP_FUNC(ARGP_CTX * o, int argc, CHAR_T * const * argv) {
	int found, result = -1;
	int rescan = 0;
	int argType = no_argument;
	const char * envVar = NULL;
	const CHAR_T * opt = NULL; /* only used for long options */
	const CHAR_T * sopt = NULL;
	const ARGP_LOPT * lopt = NULL;
	errno = 0;
	/* check input arguments */
	if (o == NULL || argv == NULL) goto onNullPtr;
	if ((o->flags & ARGP_SHORT) != 0 && o->shortOpts == NULL) goto onNullPtr;
	if ((o->flags & ARGP_LONG) != 0 && o->longOpts == NULL) goto onNullPtr;
	if (o->i < o->lastOpt || (o->i == 1 && o->next == NULL)) o->state = APST_START;
	if (o->i != o->nextI) o->next = NULL;
	if (o->i <= 0 || o->lastOpt <= 0) {
		rescan = 1;
		o->state = APST_START;
	}
	if (o->state >= APST_END) {
		if (o->i == 1) {
			/* assume the user did reset the parser after an error */
			o->state = APST_START;
		} else {
			/* assume the user wants to continue after an error */
			if (o->next == NULL && o->state != APST_ERROR_NON_OPT) {
				moveOptToFront(o, argc, (CHAR_T **)argv);
			}
			o->state = APST_NEXT;
		}
	}
	if (argc <= 1) o->state = APST_END;
	while (o->state < APST_END) {
		if (o->i >= argc || argv[o->i] == NULL) o->state = APST_END;
		switch (o->state) {
		case APST_START:
			/* re-initialize context */
			o->i = 1;
			o->lastOpt = 1;
			o->arg = NULL;
			o->next = NULL;
			if (rescan != 0) {
				o->flags = (tArgPFlag)((size_t)(o->flags) & (size_t)(~(ARGP_POSIXLY_CORRECT | ARGP_ARG_ONE | ARGP_FORWARD_ERRORS | ARGP_GNU_W)));
			}
			o->state = APST_NEXT;
			if ((envVar = getenv("POSIXLY_CORRECT")) != NULL && *envVar != 0) {
				o->flags = (tArgPFlag)(o->flags | ARGP_POSIXLY_CORRECT);
			}
			/* parse special prefix characters in short option string */
			if (o->shortOpts != NULL) {
				sopt = TCHAR_STCHR(o->shortOpts, 'W');
				if (sopt != NULL && sopt[1] == ';') o->flags = (tArgPFlag)(o->flags | ARGP_GNU_W);
				for (; o->shortOpts[0] != 0; o->shortOpts++) {
					switch (o->shortOpts[0]) {
					case '+': o->flags = (tArgPFlag)(o->flags | ARGP_POSIXLY_CORRECT); break;
					case '-': o->flags = (tArgPFlag)(o->flags | ARGP_ARG_ONE); break;
					case ':': o->flags = (tArgPFlag)(o->flags | ARGP_FORWARD_ERRORS); break;
					default: goto onEndOfShortFlags; break;
					}
				}
onEndOfShortFlags:;
			}
			break;
		case APST_NEXT:
			/* process next option */
			o->arg = NULL;
			o->longMatch = -1;
			if (o->next != NULL) {
				/* we can only reach this point if ARGP_SHORT was set */
				o->state = APST_SHORT;
			} else {
				if (argv[o->i][0] == '-') {
					if (argv[o->i][1] == '-') {
						if (argv[o->i][2] != 0) {
							if ((o->flags & ARGP_LONG) != 0) {
								opt = argv[o->i] + 2;
								o->state = APST_LONG;
							} else {
								o->opt = argv[o->i][1];
								o->state = APST_ERROR_INVALID_OPT;
							}
						} else {
							o->i++;
							o->state = APST_ERROR_NON_OPT;
						}
					} else if (argv[o->i][1] == 0) {
						o->i++;
						o->state = APST_ERROR_NON_OPT;
					} else if ((o->flags & ARGP_SHORT) != 0) {
						o->next = argv[o->i] + 1;
						o->nextI = o->i;
						o->state = APST_SHORT;
					} else if ((o->flags & ARGP_LONG) != 0) {
						opt = argv[o->i] + 1;
						o->state = APST_LONG;
					} else {
						o->opt = argv[o->i][1];
						o->state = APST_ERROR_INVALID_OPT;
					}
				} else if ((o->flags & ARGP_POSIXLY_CORRECT) == 0) {
					if ((o->flags & ARGP_ARG_ONE) == 0) {
						o->state = APST_ERROR_NON_OPT;
					} else {
						result = 1;
						argType = required_argument;
						o->arg = argv[o->i];
						o->state = APST_ARG;
					}
				} else {
					/* skip argument (NEXT -> NEXT) */
					o->i++;
				}
			}
			break;
		case APST_SHORT:
			/* process a single short option */
			if ((o->flags & ARGP_GNU_W) != 0 && o->next[0] == 'W') {
				/* process GNU extension for long options */
				if (o->next[1] == '=') {
					opt = o->next + 2;
				} else if (o->next[1] == 0) {
					moveOptToFront(o, argc, (CHAR_T **)argv);
					o->i++;
					opt = argv[o->i];
				} else if ((o->flags & ARGP_GNU_SHORT) != 0) {
					opt = o->next + 1;
				} else {
					/* ignore remaining short options and treat next argument as option argument */
					moveOptToFront(o, argc, (CHAR_T **)argv);
					o->i++;
					opt = argv[o->i];
				}
				o->next = NULL;
				o->state = APST_LONG;
			} else {
				o->opt = o->next[0];
				sopt = TCHAR_STCHR(o->shortOpts, o->opt);
				argType = no_argument;
				if (sopt != NULL) {
					result = *sopt;
					if (sopt[1] == ':') {
						if (sopt[2] != ':') {
							argType = required_argument;
						} else {
							argType = optional_argument;
						}
					} /* else: no_argument */
					if ((o->flags & ARGP_LONG) != 0) {
						/* search for matching long option */
						for (lopt = o->longOpts; lopt->name != NULL; lopt++) {
							if (lopt->val == o->opt) {
								o->longMatch = (int)(lopt - o->longOpts);
								if (lopt->flag != NULL) {
									*(lopt->flag) = 1;
									result = 0;
								}
								argType = lopt->has_arg; /* long option argument type overwrites short one */
								break;
							}
						}
					}
					if (argType == no_argument
						|| (argType == optional_argument && (o->flags & ARGP_GNU_SHORT) == 0 && o->next[1] != 0 && o->next[1] != '=')) {
						(o->next)++;
						if (o->next[0] == 0 || o->next[0] == '=') {
							o->next = NULL;
							moveOptToFront(o, argc, (CHAR_T **)argv);
							o->i++;
						}
						o->state = APST_NEXT;
						return result;
					} else {
						if (o->next[1] == '=') {
							o->arg = o->next + 2;
						} else if (o->next[1] == 0) {
							if ((o->i + 1) < argc) {
								o->arg = argv[o->i + 1];
							} /* else: o->arg = NULL */
						} else if ((o->flags & ARGP_GNU_SHORT) != 0) {
							o->arg = o->next + 1;
						} /* else: o->arg = NULL */
						o->next = NULL;
						o->state = APST_ARG;
					}
				} else {
					(o->next)++;
					if (o->next[0] == 0 || o->next[0] == '=') o->next = NULL;
					o->state = APST_ERROR_INVALID_OPT;
				}
			}
			break;
		case APST_LONG:
			/* process a single long option */
			o->opt = 0;
			found = 0;
			argType = no_argument;
			sopt = NULL;
			for (lopt = o->longOpts; lopt->name != NULL; lopt++) {
				sopt = matchPrefixStr(opt, lopt->name);
				if (sopt != NULL) {
					o->opt = lopt->val;
					o->longMatch = (int)(lopt - o->longOpts);
					if (result != -1) {
						found = -1;
						break;
					}
					result = o->opt;
					if (lopt->flag != NULL) {
						*(lopt->flag) = 1;
						result = 0;
					}
					argType = lopt->has_arg;
					if (*sopt != 0) o->arg = sopt;
					found = 1;
				}
			}
			if (found == 1) {
				if (argType == no_argument) {
					moveOptToFront(o, argc, (CHAR_T **)argv);
					o->i++;
					o->arg = NULL;
					o->state = APST_NEXT;
					return result;
				} else {
					if (o->arg == NULL && (o->i + 1) < argc) {
						o->arg = argv[o->i + 1];
					}
					o->state = APST_ARG;
				}
			} else if (found == -1) {
				o->state = APST_ERROR_AMBIGUOUS;
			} else {
				o->longMatch = -1;
				o->state = APST_ERROR_INVALID_OPT;
			}
			break;
		case APST_ARG:
			if (o->arg != NULL && o->arg[0] == '-' && o->arg[1] != 0) o->arg = NULL;
			if (o->arg == NULL) {
				if (argType != required_argument) {
					moveOptToFront(o, argc, (CHAR_T **)argv);
				} else {
					o->lastOpt = o->i;
				}
				o->i++;
				if (argType == required_argument) {
					o->state = APST_ERROR_MISSING_ARG;
				} else {
					o->state = APST_NEXT;
					return result;
				}
			} else {
				/* o->arg was only set for optional or required arguments */
				o->next = NULL;
				moveOptToFront(o, argc, (CHAR_T **)argv);
				if (argv[o->i + 1] == o->arg) o->i++;
				o->i++;
				o->state = APST_NEXT;
				return result;
			}
			break;
		default: break; /* other states are handled outside the while loop */
		}
	} /* while (o->state < APST_END) */
	switch (o->state) {
	case APST_END:
		if (o->i > o->lastOpt) moveOptToFront(o, argc, (CHAR_T **)argv);
		o->opt = 0;
		o->longMatch = -1;
		break;
	case APST_ERROR_NON_OPT:
		break;
	case APST_ERROR_MISSING_ARG:
		o->arg = NULL;
		o->next = NULL;
		if ((o->flags & ARGP_FORWARD_ERRORS) == 0) {
			if (opt != NULL) {
				TCHAR_ERROR("%s: option '%s' requires an argument\n", argv[0], argv[o->lastOpt]);
			} else {
				TCHAR_ERROR("%s: option '-%c' requires an argument\n", argv[0], o->opt);
			}
		}
		if ((o->flags & ARGP_FORWARD_ERRORS) != 0) return ':';
		return '?';
		break;
	case APST_ERROR_INVALID_OPT:
		o->arg = NULL;
		if (o->next == NULL) {
			moveOptToFront(o, argc, (CHAR_T **)argv);
			o->i++;
		}
		if ((o->flags & ARGP_FORWARD_ERRORS) == 0) {
			if (opt != NULL) {
				TCHAR_ERROR("%s: unrecognized option '%s'\n", argv[0], argv[o->lastOpt]);
			} else {
				TCHAR_ERROR("%s: unrecognized option '-%c'\n", argv[0], o->opt);
			}
		}
		return '?';
		break;
	case APST_ERROR_AMBIGUOUS:
		o->arg = NULL;
		moveOptToFront(o, argc, (CHAR_T **)argv);
		o->i++;
		if ((o->flags & ARGP_FORWARD_ERRORS) == 0) {
			TCHAR_ERROR("%s: option '%s' is ambiguous\n", argv[0], argv[o->lastOpt]);
		}
		return '?';
		break;
	case APST_ERROR_NULL_PTR:
onNullPtr:
		if (o != NULL) o->state = APST_ERROR_NULL_PTR;
#if defined(EFAULT)
		errno = EFAULT;
#elif defined(EINVAL)
		errno = EINVAL;
#endif
		break;
	default: /* unreachable */ break;
	}
	return -1;
}
