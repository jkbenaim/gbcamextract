#ifndef __ERR_SHIM_H__
#define __ERR_SHIM_H__

#ifdef __MINGW32__
#define warn(...) do {fprintf(stderr, __VA_ARGS__); fprintf(stderr, ": %s\n", strerror(errno));} while (0);
#define warnx(...) do {fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n");} while (0);
#define err(eval, ...) do {fprintf(stderr, __VA_ARGS__); fprintf(stderr, ": %s\n", strerror(errno)); exit(eval);} while (0);
#define errx(eval, ...) do {fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); exit(eval);} while (0);
#else
#include <err.h>
#endif

#endif
