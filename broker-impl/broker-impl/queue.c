/*!
 Load balancer for clients that want to execute commands on remote servers,
 using the 0-MQ library.
 
 We implemented a queue with atomic operations. The queue supports a shuffeling
 operation for the client code that requests balancing across the requests set.
 
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

#include <stdlib.h>
#include <time.h>
#include "queue.h"

typedef struct __queue_t {
    balancing_policy_t balancing_policy;
    void *head;
    unsigned int length;
    int dependencies_did_init;
    
    /* Callback functions */

    // Deletes the head of the queue
    void (*delete)(void *head);
    
    // Pushes an element in the queue; the head might be modified
    int (*push)(queue_t q, void *key);

    // Removes an element in the queue; the head might be modified
    int (*remove_key)(queue_t q, void *key,
        int (*compare)(void *key1, void *key2));
    
    // Returns an element in the queue
    void *(*get_key)(void *head, int index);
    
    // Iterates over a queue
    void (*iterate)(void *head, void (*iterator)(void *key));
} *_queue_t;

/* Circular doubly linked list implementation */
static void rr_queue_delete(void *head);
static int rr_queue_push(queue_t q, void *key);
static int rr_queue_remove_key(queue_t q, void *key,
    int (*compare)(void *key1, void *key2));
static void *rr_queue_get_key(void *head, int index);
static void rr_queue_iterate(void *head, void (*iterator)(void *key));

/* Array implementation */
static void rnd_queue_delete(void *head);
static int rnd_queue_push(queue_t q, void *key);
static int rnd_queue_remove_key(queue_t q, void *key,
    int (*compare)(void *key1, void *key2));
static void *rnd_queue_get_key(void *head, int index);
static void rnd_queue_iterate(void *head, void (*iterator)(void *key));

queue_t queue_new(balancing_policy_t balancing_policy) {
    _queue_t result = NULL;
    
    if (balancing_policy != ROUND_ROBIN &&
        balancing_policy != RANDOM &&
        balancing_policy != USER_DEFINED) {
        return NULL;
    }
    
    result = (_queue_t) malloc(sizeof(struct __queue_t));
    if (!result) {
        goto end;
    }
    
    // Initialize the queue data structure
    result->balancing_policy = balancing_policy;
    result->head = NULL;
    result->length = 0;
    result->dependencies_did_init = 0;
    
    if (balancing_policy == ROUND_ROBIN) {
        queue_init(result,
            rr_queue_delete,
            rr_queue_push,
            rr_queue_remove_key,
            rr_queue_get_key,
            rr_queue_iterate);
    } else if (balancing_policy == RANDOM) {
        queue_init(result,
            rnd_queue_delete,
            rnd_queue_push,
            rnd_queue_remove_key,
            rnd_queue_get_key,
            rnd_queue_iterate);
    } else if (balancing_policy == USER_DEFINED) {
        // To be filled by calling queue_init
    } else {
        queue_delete(result);
        return NULL;
    }
    
end:
    return result;
}

void queue_init(queue_t queue,
    void (*delete)(void *head),
    int (*push)(queue_t queue, void *key),
    int (*remove_key)(queue_t queue, void *key,
        int (*compare)(void *key1, void *key2)),
    void *(*get_key)(void *head, int index),
    void (*iterate)(void *head, void (*iterator)(void *key))) {

    _queue_t q = (_queue_t) queue;
    q->delete = delete;
    q->push = push;
    q->remove_key = remove_key;
    q->get_key = get_key;
    q->iterate = iterate;
}

void queue_delete(queue_t queue) {
    if (!queue) {
        return;
    }
    
    _queue_t q = (_queue_t) queue;
    
    q->delete(q->head);
    
    q->length = 0;
    q->balancing_policy = 0;
    q->head = NULL;
    q->delete = NULL;
    q->push = NULL;
    q->remove_key = NULL;
    q->iterate = NULL;
    
    free(queue);
}

int queue_push(queue_t queue, void *key) {
    if (!queue) {
        return NULL_POINTER_EXCEPTION;
    }
    
    _queue_t q = (_queue_t) queue;
    int result = q->push(q, key);
    if (!result) {
        q->length++;
    }
    
    return result;
}

int queue_remove_key(queue_t queue, void *key,
    int (*compare)(void *key1, void *key2)) {
    
    _queue_t q = (_queue_t) queue;
    
    if (!q || !q->head) {
        return NULL_POINTER_EXCEPTION;
    }
    
    if (!q->length) {
        return EMPTY_QUEUE_EXCEPTION;
    }
    
    return q->remove_key((_queue_t) q, key, compare);
}

void *queue_get_key(queue_t queue) {
    _queue_t q = (_queue_t) queue;
    
    if (!q || !q->head || !q->length) {
        return NULL;
    }
    
    if (!q->dependencies_did_init) {
        q->dependencies_did_init = 1;
        srand(time(NULL));
    }
    
    int index = -1;
    
    if (q->balancing_policy == USER_DEFINED) {

    } else if (q->balancing_policy == RANDOM) {
        index = rand() % q->length;
    } else if (q->balancing_policy == ROUND_ROBIN) {
        
    } else {
        return NULL;
    }
    
    return q->get_key(q->head, index);
}

void queue_iterate(queue_t queue, void (*iterator)(void *key)) {
    if (!queue || !iterator) {
        return;
    }
    _queue_t q = (_queue_t) queue;
    if (q->length && q->head) {
        q->iterate(q->head, iterator);
    }
}


// Doubly linked list queue (circular)
typedef struct __queue_node_t {
    void *key;
    struct __queue_node_t *prev;
    struct __queue_node_t *next;
} *queue_node_t;

typedef struct __array_queue_t {
    void **data;      // Data allocated
    long capacity;    // Total capacity of allocated data
    long size;        // Current number of elements in the queue
} *array_queue_t;

void rr_queue_delete(void *queue) {
    queue_node_t q = (queue_node_t) queue;
    if (!q) {
        return;
    }

    queue_node_t head = q;
    if (q->next == head) {
        free(q);
    } else {
        queue_node_t next = q->next;
        do {
            free(q);
            q = next;
            next = next->next;
        } while (next != head);
        free(q);
    }
}

int rr_queue_push(queue_t queue, void *key) {
    _queue_t q = (_queue_t) queue;
    queue_node_t new_node = (queue_node_t)
        malloc(sizeof(struct __queue_node_t));
    
    if (!new_node) {
        return OUT_OF_MEMORY_EXCEPTION;
    }
    
    new_node->key = key;
    new_node->prev = new_node->next = new_node;
    
    if (!q->head) {
        // Set the queue's head to the new node
        q->head = new_node;
    } else {
        // Add the new node to the end of the queue
        queue_node_t head = (queue_node_t) q->head;
        queue_node_t prev = (queue_node_t) head->prev;
        prev->next = new_node;
        new_node->prev = prev;
        new_node->next = q->head;
        head->prev = new_node;
    }
    
    return SUCCESS;
}

int rr_queue_remove_key(queue_t queue, void *key,
    int (*compare)(void *key1, void *key2)) {
    
    _queue_t q = (_queue_t) queue;
    queue_node_t head = (queue_node_t) q->head;
    if (!head) {
        return NULL_POINTER_EXCEPTION;
    }

    queue_node_t it = head;
    
    while (it) {
        if (!compare(key, it->key)) {
            queue_node_t prev = it->prev;
            queue_node_t next = it->next;
            
            q->length--;
            
            if (q->length > 0) {
                prev->next = next;
                next->prev = prev;
                q->head = next;
            } else {
                q->head = NULL;
            }
            
            free(it);
            
            return SUCCESS;
        }
        it = it->next;
        if (it == head) {
            break;
        }
    }
    return KEY_NOT_FOUND_EXCEPTION;
}

void *rr_queue_get_key(void *head, int index) {
    queue_node_t it = (queue_node_t) head;
    queue_node_t prev = it->prev;
    queue_node_t next = it->next;
    queue_node_t nextnext = next->next;
    
    if (next == prev) {
        return it->key;
    }

    void *temp = it->key;
    it->key = next->key;
    next->key = temp;
    
    prev->next = next;
    next->prev = prev;
    next->next = it;
    it->next = nextnext;
    it->prev = next;
    nextnext->prev = it;
    
    return it->key;
}

void rr_queue_iterate(void *q, void (*iterator)(void *key)) {
    queue_node_t head = q;
    while (head) {
        iterator(head->key);
        head = head->next;
        if (head == q) {
            break;
        }
    }
}
// Doubly linked list queue (circular)

// Array
void rnd_queue_delete(void *head) {
    array_queue_t it = (array_queue_t) head;
    if (it) {
        free(it->data);
    }
    free(head);
}

int rnd_queue_push(queue_t queue, void *key) {
    _queue_t q = (_queue_t) queue;
    
    if (!q) {
        return NULL_POINTER_EXCEPTION;
    }
    
    array_queue_t head = (array_queue_t) q->head;
    
    if (!head) {
        head = (array_queue_t) malloc(sizeof(struct __array_queue_t));
        head->capacity = 1024;
        head->size = 0;
        head->data = (void **) malloc(head->capacity * sizeof(void *));
        q->head = head;
    }
    
    if (head->size >= head->capacity) {
        head->capacity *= 2;
        void **tmp = (void **) realloc(head->data, head->capacity * sizeof(void *));
        if (!tmp) {
            return OUT_OF_MEMORY_EXCEPTION;
        }
        head->data = tmp;
    }
    
    head->data[head->size++] = key;

    return 0;
}

int rnd_queue_remove_key(queue_t queue, void *key,
    int (*compare)(void *key1, void *key2)) {
    
    _queue_t q = (_queue_t) queue;
    
    if (!q) {
        return NULL_POINTER_EXCEPTION;
    }
    
    array_queue_t head = (array_queue_t) q->head;
    
    if (!head) {
        return NULL_POINTER_EXCEPTION;
    }
    
    long index;
    for (index = 0; index < head->size; index++) {
        if (!compare(key, head->data[index])) {
            while (index < head->size) {
                head->data[index] = head->data[index + 1];
                index++;
            }
            head->size--;
            q->length--;
            head->data[head->size] = NULL;
            break;
        }
    }

    return 0;
}

void *rnd_queue_get_key(void *head, int index) {
    array_queue_t it = (array_queue_t) head;
    if (!it || index < 0 || index >= it->size) {
        return NULL;
    }
    return it->data[index];
}

void rnd_queue_iterate(void *head, void (*iterator)(void *key)) {
    array_queue_t it = (array_queue_t) head;
    
    if (!it) {
        return;
    }
    
    long index;
    for (index = 0; index < it->size; index++) {
        iterator(it->data[index]);
    }
}

// Array
#include <stdio.h>
void print_pointers(void *key) {
    printf("%p\n", key);
}

void queue_debug(queue_t queue) {
    _queue_t q = (_queue_t) queue;
    printf("DEBUG %p %p %d\n", q, q->head, q->length);
    q->iterate(q->head, print_pointers);
}

float execute_task(void (*task)(void)) {
    clock_t start = clock();
    task();
    clock_t end = clock();
    float seconds = (float)(end - start) / CLOCKS_PER_SEC;
    return seconds;
}

unsigned int queue_get_size(queue_t queue) {
    _queue_t q = (_queue_t) queue;
    return !q ? -1 : q->length;
}
