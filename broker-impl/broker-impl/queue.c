/*!
 Load balancer for clients that want to execute commands on remote servers,
 using the 0-MQ library.
 
 We implemented a queue with atomic operations. The queue supports a shuffeling operation
 for the client code that requests balancing across the requests set.
 
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
    int (*push)(struct __queue_t *q, void *key);

    // Removes an element in the queue; the head might be modified
    int (*remove_key)(struct __queue_t *q, void *key, int (*compare)(void *key1, void *key2));
    
    //_queue_get_key
    
    // Iterates over a queue
    void (*iterate)(void *head, void (*iterator)(void *key));
} *_queue_t;

// Doubly linked list queue
typedef struct __queue_node_t {
    void *key;
    struct __queue_node_t *prev;
    struct __queue_node_t *next;
} *queue_node_t;

static
void _queue_delete(queue_node_t q) {
    if (!q) {
        return;
    }
    if (!q->next) {
        free(q);
    } else {
        queue_node_t prev = q;
        q = q->next;
        do {
            free(prev);
            prev = q;
            q = q ? q->next : NULL;
        } while (prev);
    }
}

static
int _queue_push(_queue_t q, void *key) {
    queue_node_t new_node = (queue_node_t) malloc(sizeof(struct __queue_node_t));

    if (!new_node) {
        return OUT_OF_MEMORY_EXCEPTION;
    }
    
    new_node->key = key;
    new_node->prev = new_node->next = NULL;
    
    if (!q->head) {
        // Set the queue's head to the new node
        q->head = new_node;
    } else {
        // Add the new node to the end of the queue
        queue_node_t it = (queue_node_t) q->head;
        while (it->next) {
            it = it->next;
        }
        new_node->prev = it;
        it->next = new_node;
    }
    
    return SUCCESS;
}

static
int _queue_remove_key(_queue_t queue, void *key, int (*compare)(void *key1, void *key2)) {
    queue_node_t it = (queue_node_t) queue->head;
    if (!it) {
        return NULL_POINTER_EXCEPTION;
    }

    while (it) {
        if (!compare(key, it->key)) {
            queue_node_t prev = it->prev;
            queue_node_t next = it->next;
            
            queue->length--;
            
            if (!prev) {
                queue->head = next;
            } else if (!next) {
                prev->next = NULL;
            } else {
                prev->next = next;
                next->prev = prev;
            }
            
            free(it);
            
            return SUCCESS;
        }
        it = it->next;
    }
    return KEY_NOT_FOUND_EXCEPTION;
}

static void *_queue_get_key(queue_node_t it, int index) {
    while (index > 0) {
        index--;
        it = it->next;
    }
    return it->key;
}

static void _queue_iterate(queue_node_t q, void (*iterator)(void *key)) {
    while (q) {
        iterator(q->key);
        q = q->next;
    }
}
// Doubly linked list queue


queue_t queue_new(balancing_policy_t balancing_policy) {
    _queue_t result = NULL;
    
    result = (_queue_t) malloc(sizeof(struct __queue_t));
    if (!result) {
        goto end;
    }
    result->balancing_policy = balancing_policy;
    result->head = NULL;
    result->length = 0;
    result->dependencies_did_init = 0;
    
    if (balancing_policy == RANDOM) {
        result->delete = (void (*)(void *)) _queue_delete;
        result->push = _queue_push;
        result->remove_key = _queue_remove_key;
        result->iterate = (void (*)(void *head, void (*iterator)(void *key))) _queue_iterate;
    }
    
end:
    return result;
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

int queue_remove_key(queue_t queue, void *key, int (*compare)(void *key1, void *key2)) {
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
    
    int index = rand() % q->length;
    
    return _queue_get_key((queue_node_t) q->head, index);
}

void queue_iterate(queue_t queue, void (*iterator)(void *key)) {
    if (!queue || !iterator) {
        return;
    }
    _queue_t q = (_queue_t) queue;
    if (q->length && q->head) {
        q->iterate((queue_node_t) q->head, iterator);
    }
}
