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

#ifndef broker_impl_queue_h
#define broker_impl_queue_h

typedef enum {
    RANDOM,
    ROUND_ROBIN,
    USER_DEFINED
} balancing_policy_t;

typedef enum {
    SUCCESS,
    NULL_POINTER_EXCEPTION,
    OUT_OF_MEMORY_EXCEPTION,
    EMPTY_QUEUE_EXCEPTION,
    KEY_NOT_FOUND_EXCEPTION
} queue_exception_t;

typedef void *queue_t;

/* Creates a new queue. */
queue_t queue_new(balancing_policy_t balancing_policy);

/* Frees the memory occupied by this queue. */
void queue_delete(queue_t queue);

/* Pushes a key in the queue, returns 0 for success and -1 for failure. */
int queue_push(queue_t queue, void *key);

/* Removes a key from the queue, returns 0 for success and -1 for failure. */
int queue_remove_key(queue_t queue, void *key, int (*compare)(void *key1, void *key2));

/* Returns an element in the queue, according to the queue's balancing policy. */
void *queue_get_key(queue_t queue);

/* Iterates over a queue and calls the iterator for every key in the queue */
void queue_iterate(queue_t queue, void (*iterator)(void *key));

#endif
