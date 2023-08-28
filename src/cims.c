#include <CIMS/cims.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>

#define ROOT_UID 0

/* static function declarations start */
static ssize_t recursive_chown(char *dir_path, uid_t owner, gid_t group);
static int is_delim(char c, char *delim, int len);
/* static function declarations end */

void impl_cims_assert(char *expr, int eval, char *file, int line, char *function, char *err_fmt, ...)
{
    if (!eval) {
        va_list argp;
        va_start(argp, err_fmt);
        fprintf(stderr, "[CIMS_ASSERT] ");
        fprintf(stderr, "[%s:%d in %s] ", file, line, function);
        fprintf(stderr, "Assertion '%s' failed: ", expr);
        vfprintf(stderr, err_fmt, argp);
        fprintf(stderr, "\n");

        abort();
    }
}

int has_data_path()
{
    struct stat data_path_stat;

    if (stat(CIMS_DATA_PATH, &data_path_stat) < 0) {
        return !(errno == ENOENT);
    }


}

struct stat get_program_stat(char **v)
{
    struct stat exec_stat;
    char *real_program_path = malloc(pathconf(v[0], _PC_PATH_MAX));

    realpath(v[0], real_program_path);
    ASSERT_SYSCALL(stat(real_program_path, &exec_stat));

    free(real_program_path);
    return exec_stat;
}


int cims_create_data_path(struct stat exec_stat)
{

    /* now we actually need the user to be root */
    cims_assert(geteuid() == ROOT_UID, "please rerun as superuser!");

    core_cims_mkpath(CIMS_DATA_PATH);

    /* we want the owner of the executable to have full permissions */
    recursive_chown(CIMS_PATH, exec_stat.st_uid, exec_stat.st_gid);
}

int cims_open_logfile(FILE **logfile)
{
    *logfile = fopen(CIMS_SERVER_LOGFILE_PATH, "w+");

    return *logfile != NULL;
}

static ssize_t recursive_chown(char *dir_path, uid_t owner, gid_t group)
{
    DIR *dir_ptr;
    struct dirent *entry;

    dir_ptr = opendir(dir_path);

    /* NOTE: the program WILL fail if either the opening or modifying fails */
    cims_assert(dir_ptr != NULL, "cannot open DIR \"%s\" for chown", dir_path);

    ASSERT_SYSCALL(chown(dir_path, owner, group));

    while (NULL != (entry = readdir(dir_ptr))) {
        if (entry->d_name[0] == '.')
            continue;
        printf("directory %s\n", entry->d_name);
        if (entry->d_type == DT_DIR) {
            char *sub_dir = malloc(pathconf(dir_path, _PC_PATH_MAX));
            /* append the directory name to the path and execute the function again */
            core_cims_strncreat(sub_dir, (char *[]) {dir_path, "/", entry->d_name, NULL});

            printf("sub_dir chown %s\n", sub_dir);
            recursive_chown(sub_dir, owner, group);
            free(sub_dir);
        }
    }

    closedir(dir_ptr);
}

static int is_delim(char c, char *delim, int len)
{
    for (int i = 0; i < len; ++i)
        if (c == delim[i])
            return 1;

    return 0;
}

/* i really hate the strncpy strcat flow of concatenating strings into a new buffer
 * */
void core_cims_strncreat(char *buff, char **arr)
{
    strcpy(buff, arr[0]);
    NULL_TERM_BUFF(buff, strlen(arr[0]));

    for (int i = 1; arr[i] != NULL; ++i)
        strcat(buff, arr[i]);
}

static void zero_delim(char *str, int *offset, char *delim, size_t delim_len)
{
    while (is_delim(str[*offset], delim, delim_len)) {
        str[*offset] = '\0';
        *offset++;
    }
}

/* libc's strtok is one of the most annoying functions in existence.
 * It is beyond me why they didn't do it with an offset pointer.
 * */
char *core_cims_strtok(char *str, char *delim, int *offset, int len)
{
    size_t tok_start = *offset;
    size_t delim_len = strlen(delim);

    char *tok = NULL;

    /* if the first character is a delimiter we skip it */
    if (tok_start == 0)
        if (is_delim(str[0], delim, delim_len))
            tok_start = 1;

    for (int i = tok_start; i < len; ++i) {

        if (str[i] == '\0') {
            *offset = i;
            break;
        }

        if (is_delim(str[i], delim, delim_len)) {
            zero_delim(str, &i, delim, delim_len);
            tok = &str[tok_start];
            *offset = i;
            break;
        }
    }

    *offset += NULL_TERM_SIZE;
    return tok;
}

int core_cims_strcat(char *dst, char *src, int *offset, int len)
{
    size_t src_len = strlen(src);

    memcpy(dst + *offset, src, src_len + NULL_TERM_SIZE);
    *offset += src_len;

    return 0;
}

void *core_cims_calloc(size_t chunk, size_t count)
{
    void *null_buff;

    null_buff = malloc(chunk * count);
    explicit_bzero(null_buff, chunk * count);

    return null_buff;
}

int core_cims_mkpath(const char *s)
{
    size_t len          = strlen(s);
    size_t path_offset  = 0;
    size_t str_offset   = 0;
    char *copy          = strdup(s);
    char *sub_dir       = NULL;
    char *path_buf      = core_cims_calloc(sizeof(char), len + NULL_TERM_SIZE);

    while (NULL != (sub_dir = core_cims_strtok(copy, "/", &path_offset, len))) {

        /* append the path component */
        core_cims_strcat(path_buf, "/", &str_offset, len);
        core_cims_strcat(path_buf, sub_dir, &str_offset, len);

        if (mkdir(path_buf, 0777) != 0) {
            if (errno == EEXIST) {/* this is on purpose... trust me */
                errno = 0;
                continue;
            } else {
                perror("mkdir");
                exit(EXIT_FAILURE);
            }
        }
    }

    free(path_buf);
    free(copy);


    return 0;
}

