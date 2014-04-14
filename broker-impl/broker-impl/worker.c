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

#include <stdio.h>
#include <stdlib.h>
#include "worker.h"

/* Weight for various signals used in computing a worker's load */
#define ASSIGNED_TASKS_WEIGHT    0.1
#define COMPLETED_TASKS_WEIGHT   0.2
#define CPU_LOAD_WEIGHT          1.0
#define NETWORK_LOAD_WEIGHT      0.5
#define MEMORY_LOAD_WEIGHT       0.2
#define WORKER_BUSY_WEIGHT       1.0

static
void debug_worker_task(void *key) {
    worker_task_t task = (worker_task_t) key;
    printf("    task: client_id %s, request |%s|\n",
        task->client_id,
        task->request);
}

void debug_worker_state(worker_state_t state) {
    printf("  worker internal id %s, worker state %d\n",
        state->worker_id, state->status);
    printf("  assigned tasks %d, completed tasks %d\n",
        state->runtime.assigned_tasks,
        state->runtime.completed_tasks);
    printf("  worker load %lf %lf %lf\n",
        state->runtime.cpu_load,
        state->runtime.memory_load,
        state->runtime.network_load);
    
    printf("  tasks\n");
    queue_iterate(state->tasks, debug_worker_task);
    printf("\n");
}

worker_task_t new_task(char *client_id, char *request) {
    worker_task_t result = (worker_task_t)
        malloc(sizeof(struct __worker_task_t));
    result->client_id = client_id;
    result->request = request;
    return result;
}

void init_default_runtime_settings(worker_statistics_t *runtime) {
    if (!runtime) {
        return;
    }
    runtime->assigned_tasks = 0;
    runtime->completed_tasks = 0;
    runtime->cpu = DEFAULT_RESOURCE_CPU;
    runtime->memory = DEFAULT_RESOURCE_MEMORY;
    runtime->network = DEFAULT_RESOURCE_NETWORK;
    runtime->cpu_load = runtime->memory_load = runtime->network_load = 0.0;
}

double get_runtime_effort(worker_statistics_t *runtime,
    worker_status_t status) {
    
    if (!runtime) {
        return -1;
    }
    
    double score = 0.0;
    
    score += ASSIGNED_TASKS_WEIGHT * runtime->assigned_tasks;
    score += COMPLETED_TASKS_WEIGHT * runtime->completed_tasks;
    score += CPU_LOAD_WEIGHT * runtime->cpu_load;
    score += NETWORK_LOAD_WEIGHT * runtime->network_load;
    score += MEMORY_LOAD_WEIGHT * runtime->memory_load;
    
    if (status == BUSY) {
        score += WORKER_BUSY_WEIGHT;
    }
    
    return score;
}

#include <string.h>

void estimate_request(char *request, long *cpu, long *memory, long *network) {
    if (request && strstr(request, "ping") == request) {
        *cpu = DEFAULT_RESOURCE_CPU;
        *memory = DEFAULT_RESOURCE_MEMORY;
        *network = DEFAULT_RESOURCE_NETWORK;
        return;
    }
    /* Some default estimates ... */
    *cpu = 0.2 * DEFAULT_RESOURCE_CPU;
    *memory = 0.2 * DEFAULT_RESOURCE_MEMORY;
    *network = 0.2 * DEFAULT_RESOURCE_NETWORK;
}

void update_worker_runtime(worker_statistics_t *runtime, char *request,
    int sign) {
    
    if (sign == 1) {
        runtime->assigned_tasks++;
    }
    
    long cpu, memory, network;
    
    estimate_request(request, &cpu, &memory, &network);
    
    runtime->cpu_load += sign * ((double) cpu / runtime->cpu);
    runtime->memory_load += sign * ((double) memory / runtime->memory);
    runtime->network_load += sign * ((double) network / runtime->network);
}

double get_runtime_load(worker_statistics_t *runtime) {
    /* Each resource vector has the same weight in the worker's load */
    return (runtime->cpu_load +
        runtime->network_load +
        runtime->memory_load) / 3.0;
}