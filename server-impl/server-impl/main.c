/*!
 Load balancer for clients that want to execute commands on remote servers,
 using the 0-MQ library.
 
 We implemented a simple server that runs a read-only request received from
 the broker. It can be extended to run processes in a sandbox that address
 various security requirements, such as no network connectivity, disabling
 certain syscalls e.g. fork, pthread etc.
 
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

#include "include/common.h"
#include "lib/zhelpers.h"

#define RESPONSE_SIZE     (1 << 12)
#define BUFFER_SIZE       (1 << 8)

/* Static buffer used to respond to requests */
static char buffer[RESPONSE_SIZE];
static int buffer_size;

/* Returns 0 if success and -1 if error */
static int execute_remote_command(char *request);

int main(void) {
    void *context = zmq_ctx_new ();
    void *worker = zmq_socket (context, ZMQ_REQ);
    s_set_id_server (worker);
    int rc = zmq_connect (worker, BACKEND_IPC_LABEL);
    char server_id[17];
    size_t server_id_len;
    
    if (s_get_id(worker, server_id, &server_id_len)) {
        return -1;
    }
    
    s_send (worker, "READY");
    SERVER_PRINT(server_id, "worker is ready!\n");
    
    while (1) {
        char *identity = s_recv (worker);
        SERVER_PRINT(server_id, "fetching request from |%s|\n", identity);
        char *empty = s_recv (worker); free (empty);
        
        //  Get request
        char *request = s_recv (worker);
        SERVER_PRINT(server_id, "processing request |%s|\n", request);
        
        // Solve the request
        char *result = execute_remote_command(request) ? SERVER_ERROR_MESSAGE : buffer;
        free (request);
        
        // Send the response
        s_sendmore (worker, identity);
        s_sendmore (worker, "");
        s_send     (worker, result);
        free (identity);
    }
    
    zmq_close (worker);
    zmq_ctx_destroy (context);
    
    return 0;
}

int execute_remote_command(char *request) {
    FILE *fp = popen(request, "r");
    if (!fp) {
        return -1;
    }
    
    memset(buffer, 0, sizeof(buffer));
    buffer_size = 0;
    static char line[BUFFER_SIZE];
    int line_size;
    memset(line, 0, sizeof(line));
    
    while (fgets(line, sizeof(line), fp) != NULL) {
        line_size = strlen(line);
        strncpy(buffer + buffer_size, line, line_size);
        buffer_size += line_size;
    }
    
    pclose(fp);
    
    return 0;
}
