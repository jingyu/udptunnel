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

#include <string.h>
#include <stdlib.h>

#include "hashtable.h"

Hashtable *hashtable_create(int capacity, float factor)
{
    Hashtable *htab;
    Hashentry **buckets;
    Hashentry *cache;
    int threshold;

    threshold = (int)(capacity * factor);

    cache = (Hashentry *)calloc(threshold, sizeof(Hashentry));
    if (!cache)
        return NULL;

    buckets = (Hashentry **)calloc(capacity, sizeof(Hashentry *));
    if (!buckets) {
        free(cache);
        return NULL;
    }

    htab = (Hashtable *)malloc(sizeof(Hashtable));
    if (!htab) {
        free(buckets);
        free(cache);
        return NULL;
    }

    htab->capacity = capacity;
    htab->factor = factor;
    htab->threshold = threshold;
    htab->count = 0;
    htab->buckets = buckets;
    htab->cache = cache;
    htab->free = cache;

    while (threshold-- > 1) {
        cache->next = cache + 1;
        cache++;
    }
    cache->next = NULL;

    return htab;
}

void hashtable_free(Hashtable *htab, hashtable_entry_free entry_free)
{
    if (!htab)
        return;

    if (entry_free)
        hashtable_clear(htab, entry_free);

    free(htab->buckets);
    free(htab->cache);
    free(htab);
}

static void hashtable_add(Hashtable *htab, uint32_t key, void *value)
{
    Hashentry *entry;
    int idx;

    idx = key % htab->capacity;

    entry = htab->free;
    htab->free = entry->next;

    entry->key = key;
    entry->data = value;
    entry->next = htab->buckets[idx];

    htab->buckets[idx] = entry;

    htab->count++;
}

static Hashentry *hashtable_get_entry(Hashtable *htab, uint32_t key)
{
    Hashentry *entry;
    int idx;

    idx = key % htab->capacity;
    entry = htab->buckets[idx];

    while (entry) {
        if (entry->key == key)
            return entry;

        entry = entry->next;
    }

    return NULL;
}

static int hashtable_rehash(Hashtable *htab)
{
    int capacity;
    Hashentry **buckets;
    Hashentry *cache;
    Hashentry *entry;
    int cache_count;
    Hashtable old_table;
    int i;

    capacity = (htab->capacity << 1) + 1;
    cache_count = (int)(capacity * htab->factor);

    cache = (Hashentry *)calloc(cache_count, sizeof(Hashentry));
    if (!cache)
        return 0;

    buckets = (Hashentry **)calloc(capacity, sizeof(Hashentry *));
    if (!buckets) {
        free(cache);
        return 0;
    }

    old_table = *htab;

    htab->capacity = capacity;
    htab->threshold = cache_count;
    htab->count = 0;
    htab->buckets = buckets;
    htab->cache = cache;
    htab->free = cache;

    while (cache_count-- > 1) {
        cache->next = cache + 1;
        cache++;
    }
    cache->next = NULL;

    for (i = 0; i < old_table.capacity || old_table.count > 0; i++) {
        entry = old_table.buckets[i];
        while (entry) {
            hashtable_add(htab, entry->key, entry->data);
            entry = entry->next;

            old_table.count--;
        }
    }

    free(old_table.cache);
    free(old_table.buckets);

    return 1;
}

int hashtable_put(Hashtable *htab, uint32_t key, void *value)
{
    Hashentry *entry;

    if (!htab)
        return 0;

    if (htab->count == htab->threshold) {
        if (!hashtable_rehash(htab))
            return 0;
    }

    entry = hashtable_get_entry(htab, key);
    if (entry)
        entry->data = value;
    else
        hashtable_add(htab, key, value);

    return 1;
}

void *hashtable_get(Hashtable *htab, uint32_t key)
{
    Hashentry *entry;

    if (!htab)
        return NULL;

    entry = hashtable_get_entry(htab, key);
    if (entry)
        return entry->data;

    return NULL;

}

int hashtable_exist(Hashtable *htab, uint32_t key)
{
    return hashtable_get_entry(htab, key) != NULL;
}

int hashtable_count(Hashtable *htab)
{
    if (htab)
        return htab->count;

    return 0;
}

int hashtable_remove(Hashtable *htab, uint32_t key, 
                     hashtable_entry_free entry_free)
{
    Hashentry **entry;
    Hashentry *to_remove;
    int idx;

    idx = key % htab->capacity;
    entry = &htab->buckets[idx];

    while (*entry) {
        if ((*entry)->key == key) {
            to_remove = *entry;
            *entry = (*entry)->next;

            if (entry_free)
                entry_free(to_remove->data);

            // Update current traverse pointer
            if (htab->traverse_ptr == to_remove)
                htab->traverse_ptr = to_remove->next;

            to_remove->next = htab->free;
            htab->free = to_remove;

            htab->count--;

            return 1;
        }

        entry = &((*entry)->next);
    }

    return 0;
}

void hashtable_clear(Hashtable *htab, hashtable_entry_free entry_free)
{
    Hashentry *entry;
    Hashentry *to_remove;
    int i;

    if (!htab || htab->count == 0)
        return;

    for (i = 0; i < htab->capacity && htab->count > 0; i++) {
        entry = htab->buckets[i];
        while (entry) {
            to_remove = entry;
            entry = entry->next;

            if (entry_free)
                entry_free(to_remove->data);

            to_remove->next = htab->free;
            htab->free = to_remove;

            htab->count--;
        }
        htab->buckets[i] = NULL;
    }
}

static Hashentry *hashtable_next_entry(Hashtable *htab)
{
    if (htab->traverse_ptr)
        htab->traverse_ptr = htab->traverse_ptr->next;

    while (!htab->traverse_ptr) {
        if (htab->traverse_idx >= htab->capacity)
            return NULL;

        htab->traverse_ptr = htab->buckets[htab->traverse_idx++];
    }

    return htab->traverse_ptr;
}

int hashtable_first(Hashtable *htab, uint32_t *key, void **value)
{
    if (!htab || (!key && !value))
        return 0;

    htab->traverse_idx = 0;
    htab->traverse_ptr = NULL;

    return hashtable_next(htab, key, value);
}

int hashtable_next(Hashtable *htab, uint32_t *key, void **value)
{
    Hashentry *entry;

    if (!htab || (!key && !value))
        return 0;

    entry = hashtable_next_entry(htab);
    if (entry) {
        if (key)
            *key = entry->key;
        if (value)
            *value = entry->data;

        return 1;
    }

    return 0;
}
