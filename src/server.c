
#include <CIMS/server.h>
#include <CIMS/cims.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <getopt.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <net/if.h>

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
#define EXPORT_FLAG 'e'

#define ACTIVE 1
#define INACTIVE !ACTIVE

/* keep track of the interfaces inside a loop */
#define IS_IF_END(_if_) ((_if_)->if_name == NULL && (_if_)->if_index == 0)

struct server_info {
    int fd;             /* socket fd */
    int backlog;
    int mode;           /* CLI_MODE or GFX_MODE */
    int verbose_log;
    struct sockaddr_in address;
    FILE *log_file;
    char *interface_name;
};

struct client_info {
    int fd;
    struct sockaddr_in address;
};

/* indeces for the flag- description pairs */
enum option_idx {
   HEADLESS_IDX = 0,
   HELP_IDX,
   LOCAL_IDX,
   LIST_IF_IDX,
   ADDRESS_IDX,
   VERBOSE_IDX,
   VERSION_IDX,
   PORT_IDX,
   DEVICE_IDX,
   EXPORT_IDX,
};

/* static function declaration start */
static void parse_args(Server_Info server, const int cnt, const char **v);
static void parse_sys_env(Server_Info server);
static int is_loopback(struct sockaddr *addr_p) __attribute__((deprecated));
static int is_ipv4(char *addr);
static int is_valid_port(int port);
static int is_valid_if_name(char *name);
static int has_data_path();
static int env_exported();
static void list_options(struct option *options, int count);
static void list_interfaces();
static void set_cli_mode(Server_Info server);
static void send_msg(Server_Info server, Client_Info client, char *message);
static void server_log(Server_Info server, char *str);
static void server_log_fmt(Server_Info server, char *fmt, ...);
static void server_error(Server_Info, char *str);
static void server_error_fmt(Server_Info server, char *fmt, ...);
/* static function declaration end */

Server_Info start_server(int c, char **v)
{
    Server_Info server = core_cims_calloc(1, sizeof(struct server_info));

    if (!env_exported()) {
        /* equal to '$ CIMS_Server -export_env' */
        cims_export_env();
    }

    if (!has_data_path()) {
        cims_create_data_path(get_program_stat(v));
    }

    server->fd = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_RC(server->fd);

    cims_open_logfile(&server->log_file);

    /* the Server data is created as followed:
     *
     *  - default data that the programm falls back to
     *  - system variables that set custom data
     *  - user submitted arguments have the highest priority and are the last
     *    ones to modify the data
     * */

    /* default server values */
    server->address.sin_family = AF_INET;
    server->address.sin_port = htons(CIMS_PORT);
    server->address.sin_addr.s_addr = inet_addr(CIMS_FALLBACK_ADDR);

    server->backlog = CIMS_BACKLOG;
    server->mode = GFX_MODE; /* default to gfx mode */
    server->verbose_log = INACTIVE;

    /* override with system values */
    parse_sys_env(server);
    /* override with user submitted values */
    parse_args(server, c, v);

    cims_assert(server->log_file != NULL, "server log file NULL");

    ASSERT_SYSCALL(setsockopt(server->fd, SOL_SOCKET, SO_REUSEADDR, (void *) &(int) { 1 }, sizeof(int)));
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
        inet_ntop(AF_INET, &(client->address.sin_addr.s_addr), ip, INET_ADDRSTRLEN);
        server_log_fmt(server, "connection from %s", ip);
        send_msg(server, client, "connection successful!");
    }

    ASSERT_RC(client->fd);

    return client;
}

void close_connection(Client_Info client)
{
    close(client->fd);
}
void stop_server(Server_Info server)
{
    server_log(server, "shutting down...");
    close(server->fd);
    fclose(server->log_file);
    free(server->interface_name);
    free(server);
}

void print_success(Server_Info server)
{
    server_log_fmt(server, "server running on %s:%d", inet_ntoa(server->address.sin_addr),
                ntohs(server->address.sin_port));
}

static void parse_sys_env(Server_Info server)
{
    /* parse the system environment for modification
     * values
     * */
    char *env_value = NULL;

    enum cims_env_key_idx {
        CIMS_PORT_IDX = 0,
        CIMS_FALLBACK_ADDR_IDX,
        CIMS_DEVICE_FLAG_IDX,
    };

    const char *cims_env_keys[] = {
        STRING_SYMBOL(CIMS_PORT),
        STRING_SYMBOL(CIMS_FALLBACK_ADDR),
    };

    for (int i = 0; i < ARRAY_SIZE(cims_env_keys); ++i) {
        env_value = getenv(cims_env_keys[i]);

        if (env_value == NULL)
            continue;

        switch(i) {
        case CIMS_PORT_IDX:
            cims_assert(is_valid_port(atoi(env_value)), "%s is not a valid port", env_value);
            server->address.sin_port = atoi(env_value);
            break;
        case CIMS_FALLBACK_ADDR_IDX:
            cims_assert(is_ipv4(env_value), "%s is not a valid ip address", env_value);
            server->address.sin_addr.s_addr = inet_addr(env_value);
            break;
        case CIMS_DEVICE_FLAG_IDX:
            cims_assert(is_valid_if_name(env_value), "%s is not a valid interface", env_value);
            server->interface_name = env_value;
        }
    }
}

static int is_valid_if_name(char *if_name_str)
{
    struct if_nameindex *if_names, *if_name;
    int ret = FALSE;

    if_names = if_nameindex();

    for (if_name = if_names; !IS_IF_END(if_name); if_name++) {
        if (!strcmp(if_names->if_name, if_name_str)) {
            ret = TRUE;
            break;
        }
    }

    if_freenameindex(if_names);

    return ret;
}

static void parse_args(Server_Info server, const int cnt, const char **v)
{

    int option_index = 0;

    opterr = 0;

    for (;;) {

        static const struct option options[] = {
            [HEADLESS_IDX]  = { "headless",   no_argument,          0,      HEADLESS_FLAG },
            [HELP_IDX]      = { "help",       no_argument,          0,      HELP_FLAG },
            [LOCAL_IDX]     = { "local",      no_argument,          0,      LOCAL_FLAG },
            [ADDRESS_IDX]   = { "address",    required_argument,    0,      ADDRESS_FLAG },
            [LIST_IF_IDX]   = { "list",       no_argument,          0,      LIST_IF_FLAG },
            [VERBOSE_IDX]   = { "verbose",    no_argument,          0,      VERBOSE_FLAG },
            [VERSION_IDX]   = { "version",    no_argument,          0,      VERSION_FLAG },
            [PORT_IDX]      = { "port",       required_argument,    0,      PORT_FLAG },
            [DEVICE_IDX]    = { "device",     required_argument,    0,      DEVICE_FLAG },
            [EXPORT_IDX]    = { "export_env", no_argument,          0,      EXPORT_FLAG },
            { 0, 0, 0, 0 },
        };

        int c = getopt_long_only(cnt, v, "", options, &option_index);

        if (c == -1)
            break;

        switch (c) {
        case HEADLESS_FLAG:
            set_cli_mode(server);
            break;
        case HELP_FLAG:         // NORETURN
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
        case LIST_IF_FLAG:      // NORETURN
            list_interfaces();
            exit(EXIT_SUCCESS);
        case VERBOSE_FLAG:
            server->verbose_log = ACTIVE;
            break;
        case VERSION_FLAG:      // NORETURN
            printf("\n\nCIMS server version \"%s\" %d.%d\n",
                    CIMS_VERSION_CODENAME, CIMS_VERSION_MAJOR, CIMS_VERSION_MINOR);
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
        case EXPORT_FLAG:   // NORETURN
    //        export_env();
            exit(EXIT_SUCCESS);
            break;
        case '?':           // NORETURN
            if (cnt > option_index)
                option_index++;

            printf("unrecognized option: \"%s\"\n", v[option_index]);
        default:
            exit(EXIT_FAILURE);
        }
    }
}

static void list_options(struct option *options, int count)
{
    char *descriptions[] = {
        [HEADLESS_IDX]  = "run the server in headless mode",
        [HELP_IDX]      = "list available options",
        [LOCAL_IDX]     = "run the server locally",
        [ADDRESS_IDX]   = "specify an address on which the server runs",
        [LIST_IF_IDX]   = "list all available network interfaces",
        [VERBOSE_IDX]   = "set logging to max output",
        [VERSION_IDX]   = "print current version",
        [PORT_IDX]      = "specify a port on which the server listens",
        [DEVICE_IDX]    = "specify a network device on which the server listens",
        [EXPORT_IDX]    = "export only the environent variables for the system",
    };


    cims_assert(count == ARRAY_SIZE(descriptions), BUG_MSG);

    for (int i = 0; i < count; ++i) {
        printf("\t-%s : %10s\n", options[i].name, descriptions[i]);
    }
}

static void list_interfaces()
{

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

static int env_exported()
{
    TODO(FUNC_IMPL_WARNING());
    return 1;
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

static void set_cli_mode(Server_Info server)
{
    server->mode = CLI_MODE;
    server_log_fmt(server, "set mode to: \t%s", STRING_SYMBOL(CLI_MODE));
}

static void send_msg(Server_Info server, Client_Info client, char *message)
{
    server_log_fmt(server, "sending message:\"%s\"", message);
    dprintf(client->fd, "%s", message);

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

