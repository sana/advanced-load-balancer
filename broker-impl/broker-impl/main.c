#include "lib/zhelpers.h"
#include <float.h>
#include <signal.h>
#include <dispatch/dispatch.h>
#include "include/common.h"
#include "queue.h"
#include "worker.h"

#define REBALANCE_PACE_IN_SECONDS       1

typedef enum {
    UNIFORM_DISTRIBUTION,
    RESOURCES_MANAGEMENT
} tasks_mapping_strategy_t;

typedef struct __broker_state_t {
    void *frontend;
    void *backend;
    int workers_count;
    pthread_t backend_thread;
    
    pthread_mutex_t mutex;
    
    tasks_mapping_strategy_t tasks_mapping_strategy;
    
    /* Do not accept more than 1024 server connections */
    worker_state_t worker_queue[1024];
    
    void (*old_sigterm_handler)(int);
    
    int rebalance_pace_in_seconds;
} broker_state_t;

/* Singleton containing the current broker's state */
static broker_state_t *instance;


/* Debugging method */
static
void dump_broker_snapshot(void);


/* Compares two pointers (memory addresses) */
static
int __pointer_compare(void *key1, void *key2);


/* Backend thread's loop; it does the followings:
 *   1) if an available server has assigned a task, it sends
 *   out the task for execution;
 *   2) load balancing: tasks stealing (another server has more
 *   tasks to execute) and tasks shrinking (a server has too few
 *   tasks to execute, so another server takes them)
 */
static
void *backend_loop(void *input);


/* Searches for the first free slot for a new worker state */
static
int find_new_worker_index(void);

/* Searches for a worker to take care of a new task */
static
int find_best_worker_for_new_task(void);

/* Searches for workers that are available and have tasks to execute */
static
int find_best_worker_for_task_dispatch(void);


/* Server interaction delegate */
static void server_delegate(void);

/* Client interaction delegate */
static void client_delegate(void);


/* SIGTERM signal handler used to free the resources allocated by this broker */
static
void sigterm_handler(int signum);

/* Initializes the dispatch queue for the broker's rebalancing module */
static
void init_rebalance_broker(void);

/* Stops the world and rebalances the broker, e.g. relocates tasks from a loaded
 * worker; do not call this function directly, it is scheduled on the global
 * dispatch queue, to run every REBALANCE_PACE_IN_SECONDS seconds
 */
static
void rebalance_broker(void);

int main(void) {
    void *context = zmq_ctx_new ();
    
    void *frontend = zmq_socket (context, ZMQ_ROUTER);
    zmq_bind (frontend, FRONTEND_IPC_LABEL);
    
    void *backend  = zmq_socket (context, ZMQ_ROUTER);
    zmq_bind (backend,  BACKEND_IPC_LABEL);
    
    instance = (broker_state_t *)malloc(sizeof(broker_state_t));
    instance->frontend = frontend;
    instance->backend = backend;
    instance->workers_count = 0;
    instance->tasks_mapping_strategy = RESOURCES_MANAGEMENT;
    pthread_mutex_init(&instance->mutex, NULL);
    pthread_create(&instance->backend_thread, NULL, backend_loop, NULL);
    
    instance->old_sigterm_handler = signal(SIGTERM, sigterm_handler);

    init_rebalance_broker();
    
    while (1) {
        zmq_pollitem_t items[] = {
            { backend, 0, ZMQ_POLLIN, 0 },
            { frontend, 0, ZMQ_POLLIN, 0 },
        };
        
        int rc = zmq_poll (items, instance->workers_count ? 2 : 1, -1);
        if (rc == -1)
            break;
        
        if (items[0].revents & ZMQ_POLLIN) {
            server_delegate();
        }
        if (items[1].revents & ZMQ_POLLIN) {
            client_delegate();
        }
    }
    
    zmq_close(instance->frontend);
    zmq_close(instance->backend);
    zmq_ctx_destroy(context);
    free(instance);
    
    return 0;
}


void server_delegate(void) {
    char *worker_id = s_recv (instance->backend);
    BROKER_PRINT("received reply from %s\n", worker_id);
    
    char *empty = s_recv (instance->backend); free(empty);
    
    char *client_id = s_recv (instance->backend);
    
    if (!strcmp (client_id, "READY")) {
        /* Create the worker's state */
        worker_state_t worker_state = (worker_state_t)
            malloc(sizeof(struct __worker_state_t));
        worker_state->worker_id = worker_id;
        worker_state->status = AVAILABLE;
        worker_state->tasks = queue_new(ROUND_ROBIN);
        pthread_mutex_init(&worker_state->mutex, NULL);
        init_default_runtime_settings(&worker_state->runtime);
        
        int worker_index = find_new_worker_index();
        instance->worker_queue[worker_index] = worker_state;
    } else {
        empty = s_recv(instance->backend); free(empty);
        
        char *reply = s_recv(instance->backend);
        
        s_sendmore (instance->frontend, client_id);
        s_sendmore (instance->frontend, "");
        s_send     (instance->frontend, reply);
        
        free (reply);
        
        int it;
        for (it = 0; it < instance->workers_count; it++) {
            worker_state_t worker_state = instance->worker_queue[it];
            if (worker_state->status == BUSY &&
                !strcmp(worker_state->worker_id, worker_id)) {
                pthread_mutex_lock (&worker_state->mutex);
                worker_state->status = AVAILABLE;
                worker_state->runtime.completed_tasks++;
                update_worker_runtime(&(worker_state->runtime), NULL, -1);
                pthread_mutex_unlock (&worker_state->mutex);
                break;
            }
        }
    }
    free(client_id);
}

void client_delegate(void) {
    // Received a new request from a client
    char *client_id = s_recv (instance->frontend);
    char *empty = s_recv (instance->frontend); free (empty);
    char *request = s_recv (instance->frontend);
    
    // Find the best worker to can deal with the task
    pthread_mutex_lock (&instance->mutex);
    int worker_id = find_best_worker_for_new_task();
    pthread_mutex_unlock (&instance->mutex);
    
    // Get the current worker's state
    worker_state_t worker_state = instance->worker_queue[worker_id];
    
    // Get the current worker's tasks queue
    queue_t tasks = worker_state->tasks;

    // Create a new task object
    worker_task_t task = new_task(client_id, request);
    
    // Add the task to the current worker's task
    pthread_mutex_lock (&worker_state->mutex);
    
    queue_push(tasks, task);
    
    update_worker_runtime(&worker_state->runtime, request, 1);
    
    pthread_mutex_unlock (&worker_state->mutex);
}

void *backend_loop(void *input) {
    while (1) {
        pthread_mutex_lock (&instance->mutex);
        int worker_id = find_best_worker_for_task_dispatch();
        pthread_mutex_unlock (&instance->mutex);
        
        if (worker_id == INVALID_WORKER_ID) {
            // No workers are available
            // FIXME: replace this with a monitor event
            usleep(100);
            continue;
        }
        
        worker_state_t worker_state = instance->worker_queue[worker_id];
        queue_t tasks = worker_state->tasks;
        
        pthread_mutex_lock (&worker_state->mutex);
        worker_task_t task = (worker_task_t) queue_get_key(tasks);

        if (task) {
            queue_remove_key(tasks, task, __pointer_compare);
        }
        pthread_mutex_unlock (&worker_state->mutex);
        
        if (!task) {
            // No tasks available
            continue;
        }

        worker_state->status = BUSY;
        
        s_sendmore (instance->backend, worker_state->worker_id);
        s_sendmore (instance->backend, "");
        s_sendmore (instance->backend, task->client_id);
        s_sendmore (instance->backend, "");
        s_send     (instance->backend, task->request);
        free(task);
    }
    return NULL;
}

int __pointer_compare(void *key1, void *key2) {
    return ((key1 < key2) ? -1 : ((key1 == key2) ? 0 : 1));
}

int find_new_worker_index(void) {
    int it;
    for (it = 0; it < instance->workers_count; it++) {
        /* Return the first DEAD worker index */
        if (instance->worker_queue[it]->status == DEAD) {
            return it;
        }
    }
    return instance->workers_count++;
}

int find_best_worker_for_new_task(void) {
    int it;
    double best_load = DBL_MAX, least_load = 1.0;
    int best_worker_id = INVALID_WORKER_ID;
    
    if (instance->tasks_mapping_strategy == RESOURCES_MANAGEMENT) {
        // If we do resource management, then we find a non-full loaded
        // worker who can take care of the task
        for (it = 0; it < instance->workers_count; it++) {
            if (instance->worker_queue[it]->status == DEAD) {
                continue;
            }
            
            worker_state_t worker_state = instance->worker_queue[it];
            double worker_load = get_runtime_load(&worker_state->runtime);
            
            if (worker_load == 0.0) {
                // Ignore IDLE workers
                continue;
            }
            
            if (least_load > worker_load) {
                least_load = worker_load;
                best_worker_id = it;
            }
        }
    }
    
    if (best_worker_id == INVALID_WORKER_ID) {
        for (it = 0; it < instance->workers_count; it++) {
            if (instance->worker_queue[it]->status == DEAD) {
                continue;
            }
        
            double worker_load = get_runtime_effort(
                &(instance->worker_queue[it]->runtime),
                instance->worker_queue[it]->status
            );

            if (worker_load < best_load) {
                best_load = worker_load;
                best_worker_id = it;
            }
        }
    }
    
    assert(best_worker_id >= 0 && best_worker_id < instance->workers_count);
    
    return best_worker_id;
}

int find_best_worker_for_task_dispatch(void) {
    int it;
    for (it = 0; it < instance->workers_count; it++) {
        if (instance->worker_queue[it]->status == AVAILABLE) {
            return it;
        }
    }
    return INVALID_WORKER_ID;
}

void dump_broker_snapshot(void) {
    pthread_mutex_lock (&instance->mutex);
    int worker_id;
    
    printf("tasks mapping strategy %d\n", instance->tasks_mapping_strategy);
    for (worker_id = 0; worker_id < instance->workers_count; worker_id++) {
        printf("worker id %d\n", worker_id);
        debug_worker_state(instance->worker_queue[worker_id]);
        printf("\n");
    }
    pthread_mutex_unlock (&instance->mutex);
}

void sigterm_handler(int signum)
{
    dump_broker_snapshot();
    instance->old_sigterm_handler(signum);
}

void init_rebalance_broker(void)
{
    instance->rebalance_pace_in_seconds = REBALANCE_PACE_IN_SECONDS;
    dispatch_after(
        dispatch_time(
            DISPATCH_TIME_NOW,
            (int64_t)(instance->rebalance_pace_in_seconds * NSEC_PER_SEC)
        ),
        dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0),
        ^(void) {
            rebalance_broker();
        }
    );
}

static
void _rebalance_broker(void);

static
int _relebance_needed(double *snapshot, int workers_count);

void rebalance_broker(void) {
    pthread_mutex_lock (&instance->mutex);
    
    _rebalance_broker();
    
    pthread_mutex_unlock (&instance->mutex);
    
    dispatch_after(
        dispatch_time(
            DISPATCH_TIME_NOW,
            (int64_t)(instance->rebalance_pace_in_seconds * NSEC_PER_SEC)
        ),
        dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0),
        ^(void) {
            rebalance_broker();
        }
    );
}

// Relocates all the tasks from the source worker to the destination worker
static
void _relocate_all_tasks(int src_worker_id, int dst_worker_id);

// Relocates some of the tasks from the source worker to the destination worker
static
void _relocate_some_tasks(int src_worker_id, int dst_worker_id);

void _rebalance_broker(void) {
    // Workers load
    static double snapshot[1024];
    static int idle_candidates[1024], idle_count;
    static int overload_candidates[1024], overload_count;
    
    int worker_id;
    for (worker_id = 0; worker_id < instance->workers_count; worker_id++) {
        worker_state_t worker_state = instance->worker_queue[worker_id];
        snapshot[worker_id] = get_runtime_load(&worker_state->runtime);
    }
    
    if (_relebance_needed(snapshot, instance->workers_count)) {
        idle_count = 0;
        overload_count = 0;
        // Compute the IDLE and the overloaded candidates
        for (worker_id = 0; worker_id < instance->workers_count; worker_id++) {
            if (snapshot[worker_id] <= WORKER_IDLE_LOAD_THRESHOLD) {
                idle_candidates[idle_count++] = worker_id;
            } else if (snapshot[worker_id] >= WORKER_OVER_LOAD_THRESHOLD) {
                overload_candidates[overload_count++] = worker_id;
            }
        }
        
        // Initially, relocate the IDLE and then the overloaded candidates
        for (worker_id = 0; worker_id < 2 * instance->workers_count; worker_id++) {
            int _worker_id = worker_id;
            if (_worker_id >= instance->workers_count) {
                _worker_id -= instance->workers_count;
            }
            
            if (snapshot[_worker_id] > WORKER_IDLE_LOAD_THRESHOLD &&
                snapshot[_worker_id] < WORKER_OVER_LOAD_THRESHOLD) {
                if (idle_count > 0) {
                    _relocate_all_tasks(idle_candidates[--idle_count], _worker_id);
                } else if (overload_count > 0) {
                    _relocate_some_tasks(overload_candidates[--overload_count], _worker_id);
                } else {
                    break;
                }
            } else if (snapshot[_worker_id] <= WORKER_IDLE_LOAD_THRESHOLD) {
                if (overload_count > 0) {
                    _relocate_some_tasks(overload_candidates[--overload_count], _worker_id);
                }
            }
        }
    }
}

int _relebance_needed(double *snapshot, int workers_count) {
    int i;
    int idle_candidates = 0, host_candidates = 0, split_candidates = 0;
    for (i = 0; i < workers_count; i++) {
        if (snapshot[i] <= WORKER_IDLE_LOAD_THRESHOLD) {
            idle_candidates++;
        } else if (snapshot[i] <= WORKER_ACCEPT_LOAD_THRESHOLD) {
            host_candidates++;
        } else if (snapshot[i] >= WORKER_OVER_LOAD_THRESHOLD) {
            split_candidates++;
        }
    }
    
    return split_candidates > 0 && (host_candidates > 0 || idle_candidates > 0);
}

static
void _relocate_tasks_count(int src_worker_id, int dst_worker_id, unsigned int tasks_count) {
    worker_state_t src_worker_state = instance->worker_queue[src_worker_id];
    worker_state_t dst_worker_state = instance->worker_queue[dst_worker_id];
 
    while (tasks_count > 0) {
        worker_task_t task = (worker_task_t) queue_get_key(src_worker_state->tasks);
        queue_remove_key(src_worker_state->tasks, task, __pointer_compare);
        queue_push(dst_worker_state->tasks, task);
        
        tasks_count--;
        
        src_worker_state->runtime.assigned_tasks--;
        
        update_worker_runtime(&src_worker_state->runtime, task->request, -1);
        update_worker_runtime(&dst_worker_state->runtime, task->request, 1);
    }
}

void _relocate_all_tasks(int src_worker_id, int dst_worker_id) {
    _relocate_tasks_count(src_worker_id,
        dst_worker_id,
        queue_get_size(instance->worker_queue[src_worker_id]->tasks));
}

void _relocate_some_tasks(int src_worker_id, int dst_worker_id) {
    _relocate_tasks_count(src_worker_id,
        dst_worker_id,
        (queue_get_size(instance->worker_queue[src_worker_id]->tasks) + 1) >> 1);
}
