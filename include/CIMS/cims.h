#ifndef CIMS_CIMS_H
#define CIMS_CIMS_H

#include <stdarg.h>
#include <stdio.h>
#include <stddef.h>
#include <sys/types.h>

/* CIMS types */
struct env_data;
#define Env_Data struct env_data *
#define SA struct sockaddr /* not to be used as a type but rather as a shorthand for (struct sockaddr *) casts */
/* CIMS types end */



/* core macros */
#define CIMS_PORT (4035)
#define MAX_HOSTNAME_LENGTH (253 + NULL_TERM_SIZE)

#define TRUE 1
#define FALSE 0
#define NULL_TERM_SIZE 1
#define NULL_TERM_BUFF(bf, sz) bf[sz] = (typeof(*bf)) 0

#define STRING_SYMBOL(sym) #sym
#define FMTARGS(...) __VA_ARGS__ /* i find this readable */
#define FUNC_IMPL_WARNING() FMTARGS("Function %s in file %s on line %d needs implementation\n", __func__, __FILE__, __LINE__ - 2)
#define FUNC_RETURN_FMT(fn, rt) FMTARGS(#fn " returned: %d", rt)
#define TODO(...) printf("[TODO] ");printf(__VA_ARGS__)

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

#define ASSERT_SYSCALL(sc)          \
            do {                    \
            if (sc < 0) {           \
                perror(#sc);        \
                exit(EXIT_FAILURE); \
            }                       \
            } while (0)

#define ASSERT_RC(rc)                                                               \
            do {                                                                    \
            if(rc < 0) {                                                            \
                fprintf(stderr,"prior syscall failed (in %s on line %d): %s\n",     \
                        __func__, __LINE__, strerror(errno)); exit(EXIT_FAILURE);   \
            }                                                                       \
            } while (0)

#define INFO_HEADER " Additional info: "
#define BUG_MSG "<THIS IS A SOFTWARE BUG> PLEASE CONTACT THE DEVELOPER"
/* core macros end */

/* cims data */
#define CIMS_FALLBACK_ADDR "0.0.0.0"
#define CIMS_PATH "/etc/cims"
#define CIMS_DATA_PATH CIMS_PATH "/data/"
#define CIMS_SERVER_LOGFILE_PATH CIMS_DATA_PATH "server.log"
/* cims data end */



/* cims functions */
void impl_cims_assert(char *expr, int eval, char *file, int line, char *function, char *err_fmt, ...) __attribute__((printf(5, 6)));


#define cims_assert(expr, ...) impl_cims_assert(#expr, (expr), __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)

struct stat get_program_stat(char **v);
int cims_create_data_path(struct stat exec_stat);
int cims_open_logfile(FILE **logfile);
int cims_export_env();
/* cims functions end */



/* core functions */
/* libc-like functions that don't give me headaches */
int core_cims_mkpath(const char *s); /* recursively create a path */
void *core_cims_calloc(size_t chunk, size_t count); /* allocate a zero filled buffer */
void core_cims_strncreat(char *buff, char **arr); /* create a string from array */
char *core_cims_strtok(char *str, char *delim, int *offset, int len); /* tokenize a string */
int core_cims_strcat(char *dst, char *src, int *offset, int len); /* append a string */
/* core functions end */

#endif /* CIMS_CIMS_H */
