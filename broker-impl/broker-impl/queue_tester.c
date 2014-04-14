/*!
 
 Tester for a queue with atomic operations and various shuffeling policies.
 
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
#include "queue.h"

static
void iterator(void *key) {
    printf("%d\n", (int) key);
}

static
int int_compare(void *key1, void *key2) {
    return (int) key1 - (int) key2;
}

static
void test_alloc(balancing_policy_t balancing_policy) {
    queue_t q = queue_new(balancing_policy);
    queue_delete(q);
}

static
void test_features(balancing_policy_t balancing_policy) {
    queue_t q = queue_new(balancing_policy);
    long i;
    queue_push(q, (void *) 0x1);
    queue_iterate(q, iterator);
    for (i = 15; i < 20; i++) {
        queue_push(q, (void *) i);
    }
    queue_iterate(q, iterator);

    for (i = 0; i < 20; i++) {
        printf("[queue_get_key] %p\n", queue_get_key(q));
    }
    
    queue_iterate(q, iterator);

    int result;
    
    queue_t q_tmp = queue_new(balancing_policy);
    result = queue_remove_key(q_tmp, (void *) 0x100, int_compare);
    printf("[queue_remove_key] NULL_POINTER_EXCEPTION %d\n", result);
    queue_delete(q_tmp);

    result = queue_remove_key(q, (void *) 0x100, int_compare);
    printf("[queue_remove_key] KEY_NOT_FOUND_EXCEPTION %d\n", result);

    for (i = 15; i < 20; i++) {
        result = queue_remove_key(q, (void *) i, int_compare);
        printf("[queue_remove_key] SUCCESS %d\n", result);
    }

    queue_iterate(q, iterator);

    result = queue_remove_key(q, (void *) 0x1, int_compare);
    printf("[queue_remove_key] SUCCESS %d\n", result);

    queue_iterate(q, iterator);

    queue_delete(q);
}

static
void stress_test(balancing_policy_t balancing_policy) {
    queue_t q = queue_new(balancing_policy);
    long i;
    for (i = 0; i < 1 << 24; i++) {
        queue_push(q, (void *) i);
    }
    queue_delete(q);
}

static
void stress_test_round_robin() {
    stress_test(ROUND_ROBIN);
}

static
void stress_test_random() {
    stress_test(RANDOM);
}

static
void debug() {
    queue_t q = queue_new(ROUND_ROBIN);
    queue_push(q, (void *) 0x1);
    printf("Key1 %p\n", queue_get_key(q));
    
    queue_push(q, (void *) 0x2);
    queue_push(q, (void *) 0x3);
    printf("Key2 %p\n", queue_get_key(q));
}

int main(void) {
    
#ifdef DEBUG
    test_alloc(ROUND_ROBIN);
    test_features(ROUND_ROBIN);
    
    test_alloc(RANDOM);
    test_features(RANDOM);
#else
    printf("ROUND_ROBIN %f\n", execute_task(stress_test_round_robin));
    printf("RANDOM %f\n", execute_task(stress_test_random));
#endif
    
    debug();
    
    return 0;
}