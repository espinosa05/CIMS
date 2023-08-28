#include <CIMS/cims.h>
#include <CIMS/server.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define CLI_MODE 1
#define GFX_MODE 0

#define HEADLESS_FLAG 'H'
#define HELP_FLAG 'h'
#define LOCAL_FLAG 'L'
#define LIST_IF_FLAG 'l'
#define ADDRESS_FLAG 'a'
#define VERBOSE_FLAG 'V'
#define VERSION_FLAG 'v'
#define PORT_FLAG 'p'
#define DEVICE_FLAG 'd'

#define ACTIVE 1
#define INACTIVE !ACTIVE


struct server_info {
    int fd;             /* socket fd */
    int backlog;
    int mode;           /* CLI_MODE or GFX_MODE */
    int verbose_log;
    struct sockaddr_in address;
    FILE *log_file;
};

struct client_info {
    int fd;
    struct sockaddr_in address;
};

/* static function declaration start */
static void parse_args(Server_Info server, int cnt, char **v);
static int is_loopback(struct sockaddr *addr_p) __attribute__((deprecated));
static int is_ipv4(char *addr);
static int is_valid_port(int port);
static int has_data_path();
static void list_options(struct option *options, int count);
static void set_cli_mode(Server_Info server);
static void server_log(Server_Info server, char *str);
static void server_log_fmt(Server_Info server, char *fmt, ...);
static void server_error(Server_Info, char *str);
static void server_error_fmt(Server_Info server, char *fmt, ...);
/* static function declaration end */


Server_Info start_server(int c, char **v)
{

    Server_Info server = calloc(1, sizeof(struct server_info));

    server->fd = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_RC(server->fd);

    /* default values */
    server->address.sin_family = AF_INET;
    server->address.sin_port = htons(CIMS_PORT);
    server->address.sin_addr.s_addr = inet_addr("0.0.0.0");

    server->backlog = CIMS_BACKLOG;
    server->mode = GFX_MODE; /* default to gfx mode */

    server->verbose_log = INACTIVE;

    /* user submitted values */
    parse_args(server, c, v);

    if (!has_data_path()) {
        cims_create_data_path(get_program_stat(v));
    }

    cims_open_logfile(&server->log_file);

    perror("swlf");
    cims_assert(server->log_file != NULL, "server log file NULL");
    ASSERT_SYSCALL(bind(server->fd, (SA *)&(server->address), sizeof(server->address)));
    ASSERT_SYSCALL(listen(server->fd, server->backlog));

    return server;
}

Client_Info accept_connection(Server_Info server)
{
    Client_Info client = calloc(1, sizeof(struct client_info));

    client->fd = accept(server->fd, (SA *)&client->address, &(socklen_t) { sizeof(client->address) });
    {
        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, ip, client->address.sin_addr.s_addr, sizeof(ip));
        server_log_fmt("connection from %s", ip);
    }

    ASSERT_RC(client->fd);

    return client;
}

void stop_server(Server_Info server)
{
    server_log(server, "shutting down...");
    close(server->fd);
    fclose(server->log_file);
    free(server);
}

void print_success(Server_Info server)
{
    server_log_fmt(server, "server running on %s:%d", inet_ntoa(server->address.sin_addr),
                ntohs(server->address.sin_port));
}

static void parse_args(Server_Info server, int cnt, char **v)
{

    int option_index = 0;

    opterr = 0;

    for (;;) {

        static const struct option options[] = {
            { "headless",   no_argument,        0,      HEADLESS_FLAG},
            { "help",       no_argument,        0,      HELP_FLAG },
            { "local",      no_argument,        0,      LOCAL_FLAG },
            { "address",    required_argument,  0,      ADDRESS_FLAG },
            { "list",       no_argument,        0,      LIST_IF_FLAG },
            { "verbose",    no_argument,        0,      VERBOSE_FLAG },
            { "version",    no_argument,        0,      VERSION_FLAG },
            { "port",       required_argument,  0,      PORT_FLAG },
            { "device",     required_argument,  0,      DEVICE_FLAG },
            { "help",       no_argument,        0,      HELP_FLAG },
            { 0,            0,                  0,      0 },
        };

        int c = getopt_long_only(cnt, v, "", options, &option_index);

        if (c == -1)
            break;

        switch (c) {
        case HEADLESS_FLAG:
            set_cli_mode(server);
            break;
        case HELP_FLAG:
            list_options(options, ARRAY_SIZE(options) - 1);
            exit(EXIT_SUCCESS);
            break;
        case LOCAL_FLAG:
            /* now we ensure that the connection is only local */
            server->address.sin_addr.s_addr = inet_addr("127.0.0.1");
            break;
        case ADDRESS_FLAG:
            cims_assert(optarg != NULL, "no ip address specified!");
            cims_assert(is_ipv4(optarg), "invalid ip address \"%s\"", optarg);
            server->address.sin_addr.s_addr = inet_addr(optarg);
            break;
        case LIST_IF_FLAG:
            TODO("implement " STRING_SYMBOL(LIST_IF_FLAG));
            exit(EXIT_SUCCESS);
        case VERBOSE_FLAG:
            server->verbose_log = ACTIVE;
            break;
        case VERSION_FLAG:
            printf("\n\nCIMS server version \"%s\" %d.%d\n", CIMS_VERSION_CODENAME, CIMS_VERSION_MAJOR, CIMS_VERSION_MINOR);
            printf("compiled on %s\n\n\n", __DATE__);
            exit(EXIT_SUCCESS);
            break;
        case PORT_FLAG:
            cims_assert(is_valid_port(atoi(optarg)), "%s is not a valid port", optarg);
            server->address.sin_port = htons(atoi(optarg));
            break;
        case DEVICE_FLAG:
            TODO("implement " STRING_SYMBOL(DEVICE_FLAG));
            break;
        case '?':
            printf("unrecognized option: \"%s\"\n", v[option_index]);
        default:
            exit(EXIT_FAILURE);
        }
    }
}

static int is_valid_port(int port)
{
    return (port < 65535) && (port != 0);
}

static int has_data_path()
{
    struct stat dps;

    if (stat(CIMS_DATA_PATH, &dps) < 0)
        return !(errno == ENOENT);

    return true;
}

static int is_loopback(struct sockaddr *addr_p)
{
    struct sockaddr_in addr = *(struct sockaddr_in *)addr_p;

    in_addr_t loopback_4_address = inet_addr("127.0.0.1");

    return addr.sin_addr.s_addr == loopback_4_address;
}

static int is_ipv4(char *str)
{
    return inet_aton(str, &(struct in_addr) {0}) != 0;
}

static void list_options(struct option *options, int count)
{
    for (int i = 0; i < count; ++i) {

        printf("%s:\n", options[i].name);
    }
}


static void set_cli_mode(Server_Info server)
{
    server->mode = CLI_MODE;
    server_log(server, "set mode to:\t" STRING_SYMBOL(CLI_MODE));
}

static void server_error(Server_Info server, char *str)
{
    fprintf(server->log_file, "[ERROR] %s\n", str);
    fprintf(stderr, "[ERROR] %s\n", str);
}

static void server_error_fmt(Server_Info server, char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char str[256];
    vsnprintf(str, sizeof(str), fmt, args);
    server_error(server, str);
}

static void server_log(Server_Info server, char *str)
{
    if (server->verbose_log)
        fprintf(stderr, "[SERVER] %s\n", str);

    fprintf(server->log_file, "[SERVER] %s\n", str);
}

static void server_log_fmt(Server_Info server, char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char str[256];
    vsnprintf(str, sizeof(str), fmt, args);
    server_log(server, str);
}

