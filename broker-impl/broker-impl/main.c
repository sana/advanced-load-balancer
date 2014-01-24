#include "lib/zhelpers.h"
#include "include/common.h"
#include "queue.h"

typedef struct __worker_task_t {
    char *client_id;
    char *request;
} worker_task_t;

typedef struct __worker_state_t {
    char *worker_id;
    char is_available;
    queue_t tasks;
} worker_state_t;

typedef struct __broker_state_t {
    void *frontend;
    void *backend;
    int available_workers;
    int current_available_worker;
    pthread_t backend_thread;
    worker_state_t worker_queue[10];
} broker_state_t;

static broker_state_t *instance;

static void *backend_loop();
static void server_delegate();
static void client_delegate();
int it = 0, it2 = 0;

static void devel() {
    printf("OK!\n");
    exit(0);
}

int main(void) {
    devel();
    void *context = zmq_ctx_new ();
    
    void *frontend = zmq_socket (context, ZMQ_ROUTER);
    zmq_bind (frontend, FRONTEND_IPC_LABEL);
    
    void *backend  = zmq_socket (context, ZMQ_ROUTER);
    zmq_bind (backend,  BACKEND_IPC_LABEL);
    
    instance = (broker_state_t *)malloc(sizeof(broker_state_t));
    instance->frontend = frontend;
    instance->backend = backend;
    instance->available_workers = 0;
    instance->current_available_worker = 0;
    pthread_create(&instance->backend_thread, NULL, backend_loop, NULL);
    
    while (1) {
        zmq_pollitem_t items[] = {
            { backend, 0, ZMQ_POLLIN, 0 },
            { frontend, 0, ZMQ_POLLIN, 0 },
        };
        
        int rc = zmq_poll (items, instance->available_workers ? 2 : 1, -1);
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


void server_delegate() {
    char *worker_id = s_recv (instance->backend);
    BROKER_PRINT("received reply from %s\n", worker_id);
    
    char *empty = s_recv (instance->backend); free(empty);
    
    char *client_id = s_recv (instance->backend);
    
    if (!strcmp (client_id, "READY")) {
        BROKER_PRINT("worker ready %d => %s, |%p|\n", instance->available_workers, worker_id, worker_id);
        instance->worker_queue [instance->available_workers].worker_id = worker_id;
        instance->worker_queue [instance->available_workers].is_available = 1;
        instance->available_workers++;
    } else {
        empty = s_recv(instance->backend); free(empty);
        
        char *reply = s_recv(instance->backend);
        
        s_sendmore (instance->frontend, client_id);
        s_sendmore (instance->frontend, "");
        s_send     (instance->frontend, reply);
        
        free (reply);
        
        instance->worker_queue[instance->current_available_worker].is_available = 1;
    }
    free(client_id);
}

void client_delegate() {
    // Received a new request from a client
    char *client_id = s_recv (instance->frontend);
    char *empty = s_recv (instance->frontend); free (empty);
    char *request = s_recv (instance->frontend);
    
    printf("MARE REQUEST %d %p %s\n", it, client_id, request);
    //TODO:aici
        //instance->worker_queue[instance->current_available_worker].tasks[it].client_id = client_id;
    //instance->worker_queue[instance->current_available_worker].tasks[it++].request = request;
}

void *backend_loop() {
    while (1) {
        if (!instance->worker_queue[instance->current_available_worker].is_available) {
            continue;
        }
        
        if (!instance->available_workers) {
            continue;
        }
        
        if (it2 >= it) {
            continue;
        }
        
        instance->worker_queue[instance->current_available_worker].is_available = 0;
        
        s_sendmore (instance->backend, instance->worker_queue[instance->current_available_worker].worker_id);
        s_sendmore (instance->backend, "");
        // TODO: POP TASK AICI
        //s_sendmore (instance->backend, instance->worker_queue[instance->current_available_worker].tasks[it2].client_id);
        s_sendmore (instance->backend, "");
        //s_send     (instance->backend, instance->worker_queue[instance->current_available_worker].tasks[it2++].request);
        
        instance->current_available_worker = (instance->current_available_worker + 1) % instance->available_workers;
    }
    return NULL;
}
