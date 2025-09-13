/**
 * @file getopt.h
 * @author Daniel Starke
 * @see getopt.h
 * @date 2017-05-22
 * @version 2025-06-14
 */
#ifndef __LIBPCF_GETOPT_H__
#define __LIBPCF_GETOPT_H__
#define __GETOPT_H__ /* avoid collision with cygwins getopt.h include in sys/unistd.h */

#include "argp.h"
#include "argpus.h"
#define option tArgPEUS


#ifdef __cplusplus
extern "C" {
#endif


extern int argpus_optind;
extern int argpus_opterr;
extern int argpus_optopt;
extern wchar_t * argpus_optarg;


extern int argpus_getopt(int argc, wchar_t * const argv[], const wchar_t * optstring);
extern int argpus_getoptLong(int argc, wchar_t * const argv[], const wchar_t * optstring, const tArgPEUS * longopts, int * longindex);
extern int argpus_getoptLongOnly(int argc, wchar_t * const argv[], const wchar_t * optstring, const tArgPEUS * longopts, int * longindex);

#define optind argpus_optind
#define opterr argpus_opterr
#define optopt argpus_optopt
#define optarg argpus_optarg
#define getopt argpus_getopt
#define getopt_long argpus_getoptLong
#define getopt_long_only argpus_getoptLongOnly


#ifdef __cplusplus
}
#endif


#endif /* __LIBPCF_GETOPT_H__ */
