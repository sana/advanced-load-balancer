/*!
 Load balancer for clients that want to execute commands on remote servers,
 using the 0-MQ library.
 
 We implemented a simple client that sends a request to a broker.
 
 Copyright (C) 2013 Laurentiu Dascalu (ldascalu@twitter.com).
 
 @author Dascalu Laurentiu
 
 This program is free software; you can redistribute it and
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 3
 of the License, or (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "lib/zhelpers.h"
#include "include/common.h"

#define DEFAULT_COMMAND_TO_EXECUTE "uname -a"

int main(int argc, char **argv) {
    void *context = zmq_ctx_new ();
    
    void *client = zmq_socket (context, ZMQ_REQ);
    s_set_id_client (client);
    zmq_connect (client, FRONTEND_IPC_LABEL);
    
    char client_id[MACHINE_ID_MAXLEN];
    size_t client_id_len;
    if (s_get_id(client, client_id, &client_id_len)) {
        return -1;
    }
    
    // Send a request
    char *command_to_execute = argc == 1 ? DEFAULT_COMMAND_TO_EXECUTE: argv[1];
    CLIENT_PRINT(client_id, "trying to execute %s\n", command_to_execute);
    s_send (client, command_to_execute);
    
    // Get the response and print out its content
    char *reply = s_recv (client);
    CLIENT_PRINT(client_id, "received %s\n", reply);
    
    free (reply);
    zmq_close (client);
    zmq_ctx_destroy (context);
    
    return 0;
}
