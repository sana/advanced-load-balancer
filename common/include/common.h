/*!
 Load balancer for clients that want to execute commands on remote servers,
 using the 0-MQ library.
 
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

#ifndef __COMMON_H_INCLUDED__
#define __COMMON_H_INCLUDED__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>


#define FRONTEND_IPC_LABEL "ipc://frontend.ipc"
#define BACKEND_IPC_LABEL "ipc://backend.ipc"

#define DEBUG 1

#define SERVER "[server]"
#define CLIENT "[client]"
#define BROKER "[broker]"

#ifdef DEBUG
#define PRINT_WITH_ID(header, pid, fmt, args...)\
    do {\
        if (!pid) {\
            printf(header " " fmt, ##args);\
        } else {\
            printf(header " |%s| " fmt, pid, ##args);\
        }\
        fflush(stdout);\
    } while(0)

#define CLIENT_PRINT(client_id, fmt, args...) PRINT_WITH_ID(CLIENT, client_id, fmt, ##args)

#define SERVER_PRINT(server_id, fmt, args...) PRINT_WITH_ID(SERVER, server_id, fmt, ##args)

#define BROKER_PRINT(fmt, args...) PRINT_WITH_ID(BROKER, NULL, fmt, ##args)

#else
#define SERVER_PRINT
#define CLIENT_PRINT
#define BROKER_PRINT
#endif



#define SERVER_ERROR_MESSAGE "server failed to execute requested command"

#endif  //  __COMMON_H_INCLUDED__