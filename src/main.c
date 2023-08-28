#include <stdlib.h>
#include <CIMS/server.h>

int main(int argc, char **argv)
{
    Server_Info server;

    server = start_server(argc, argv);
    print_success(server);

    while (1) {

        Client_Info client;

        client = accept_connection(server);

    }

    stop_server(server);

    return EXIT_SUCCESS;
}
