/*
 * udptunnel : Lightweight TCP over UDP Tunneling
 *
 * Copyright (C) 2014 Jingyu jingyu.niu@gmail.com
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __HASHTABLE_H__
#define __HASHTABLE_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct hashentry
{
    uint32_t            key;
    void *              data;
    struct hashentry *  next;
} Hashentry;

typedef struct hashtable {
    int         capacity;
    float       factor;
    int         threshold;
    int         count;
    Hashentry** buckets;
    Hashentry*  cache;
    Hashentry*  free;

    int         traverse_idx;
    Hashentry*  traverse_ptr;
} Hashtable;

typedef void(*hashtable_entry_free)(void *);

Hashtable *hashtable_create(int capacity, float factor);

void hashtable_free(Hashtable *htab, hashtable_entry_free entry_free);

int hashtable_put(Hashtable *htab, uint32_t key, void *value);

void *hashtable_get(Hashtable *htab, uint32_t key);

int hashtable_exist(Hashtable *htab, uint32_t key);

int hashtable_count(Hashtable *htab);

int hashtable_remove(Hashtable *htab, uint32_t key, 
                     hashtable_entry_free entry_free);

void hashtable_clear(Hashtable *htab, hashtable_entry_free entry_free);

int hashtable_first(Hashtable *htab, uint32_t *key, void **value);

int hashtable_next(Hashtable *htab, uint32_t *key, void **value);

#ifdef __cplusplus
}
#endif

#endif /* __HASHTABLE_H__ */
