/**
 * @file getopt.c
 * @author Daniel Starke
 * @see getopt.h
 * @date 2017-05-22
 * @version 2025-06-14
 */
#include "getopt.h"


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
		(size_t)(argpus_ctx.flags | ARGP_SHORT | ARGP_GNU_SHORT | ((argpus_opterr == 0) ? ARGP_FORWARD_ERRORS : 0))
		& (size_t)(~ARGP_LONG)
	);
	return argpus_internalGetopt(argc, argv, optstring, NULL, NULL);
}


int argpus_getoptLong(int argc, wchar_t * const argv[], const wchar_t * optstring, const tArgPEUS * longopts, int * longindex) {
	argpus_ctx.flags = (tArgPFlag)(argpus_ctx.flags | ARGP_SHORT | ARGP_LONG | ARGP_GNU_SHORT | ((argpus_opterr == 0) ? ARGP_FORWARD_ERRORS : 0));
	return argpus_internalGetopt(argc, argv, optstring, longopts, longindex);
}


int argpus_getoptLongOnly(int argc, wchar_t * const argv[], const wchar_t * optstring, const tArgPEUS * longopts, int * longindex) {
	argpus_ctx.flags = (tArgPFlag)(
		(size_t)(argpus_ctx.flags | ARGP_LONG | ((argpus_opterr == 0) ? ARGP_FORWARD_ERRORS : 0))
		& (size_t)(~ARGP_SHORT)
	);
	return argpus_internalGetopt(argc, argv, optstring, longopts, longindex);
}
