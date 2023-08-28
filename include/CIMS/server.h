#ifndef CIMS_SERVER_H
#define CIMS_SERVER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define TRUE 1
#define FALSE 0

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))
#define STRING_SYMBOL(sym) #sym
#define NULL_TERM_SIZE 1
#define CIMS_BACKLOG 0xff
#define CIMS_PORT (4035)
#define MAX_HOSTNAME_LENGTH (253 + NULL_TERM_SIZE)

typedef struct server_info *Server_Info;
typedef struct client_info *Client_Info;

Server_Info start_server();
void stop_server(Server_Info server);
void close_connection(Server_Info server);
Client_Info accept_connection(Server_Info server);

#endif /* CIMS_SERVER_H */
