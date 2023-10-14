#ifndef CIMS_SERVER_H
#define CIMS_SERVER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define CIMS_BACKLOG 0xff

/* server types end */
typedef struct server_info *Server_Info;
typedef struct client_info *Client_Info;
/* server types end */


/* server functions */
Server_Info start_server();
void stop_server(Server_Info server);
void close_connection(Client_Info client);
Client_Info accept_connection(Server_Info server);
/* server functions end */


#endif/* CIMS_SERVER_H */
