/*!
 Load balancer for clients that want to execute commands on remote servers,
 using the 0-MQ library.
 
 Worker abstraction used by the broker to do resource management. 
 
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


#ifndef broker_impl_worker_h
#define broker_impl_worker_h

#include "queue.h"
#include <pthread.h>
#include <libkern/OSAtomic.h>


typedef struct __worker_task_t {
    char *client_id;
    char *request;
} *worker_task_t;

typedef enum  {
    AVAILABLE,
    BUSY,
    DEAD
} worker_status_t;

#define INVALID_WORKER_ID                      -1

/* Available CPU cycles per second */
#define DEFAULT_RESOURCE_CPU                10000

/* Available memory in megabytes */
#define DEFAULT_RESOURCE_MEMORY             10000

/* Available network bandwidth, in megabytes per second */
#define DEFAULT_RESOURCE_NETWORK            10000


/* Worker's maximum load for which it is recommended to become IDLE */
#define WORKER_IDLE_LOAD_THRESHOLD           0.20

/* Worker's maximum load for which it still accepts tasks from other workers,
 * and would not quality for IDLE conversion
 */
#define WORKER_ACCEPT_LOAD_THRESHOLD         0.70

/* Worker's minimum load for which it is considered overloaded and tasks
 * relocation is heavily recommended
 */
#define WORKER_OVER_LOAD_THRESHOLD           0.95


typedef struct __worker_statistics_t {
    /* Available worker's resources */
    long network;
    long memory;
    long cpu;
    
    /* Worker's load in percentages [0, 1.0] */
    double network_load;
    double memory_load;
    double cpu_load;
    
    /* Number of assigned tasks */
    int assigned_tasks;
    
    /* Number of completed tasks */
    int completed_tasks;
} worker_statistics_t;

typedef struct __worker_state_t {
    char *worker_id;
    worker_status_t status;
    queue_t tasks;
    pthread_mutex_t mutex;
    worker_statistics_t runtime;
} *worker_state_t;

void debug_worker_state(worker_state_t state);

/* Creates a new task */
worker_task_t new_task(char *client_id, char *request);

/* Initializes the default runtime settings for a worker */
void init_default_runtime_settings(worker_statistics_t *runtime);

/* Returns the runtime effort, e.g. load, of a worker */
double get_runtime_effort(worker_statistics_t *runtime, worker_status_t status);

/* Updates the worker's runtime information */
void update_worker_runtime(worker_statistics_t *runtime, char *request, int sign);

/* Returns a double in [0, 1.0] proportional with the worker's current load */
double get_runtime_load(worker_statistics_t *runtime);

#endif
