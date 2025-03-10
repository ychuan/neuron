/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2021 EMQ Technologies Co., Ltd All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 **/

#include <assert.h>
#include <math.h>
#include <string.h>

#include <nng/nng.h>
#include <nng/supplemental/util/platform.h>

#include "utils/uthash.h"

#include "define.h"
#include "tag.h"

#include "cache.h"

typedef struct {
    char group[NEU_GROUP_NAME_LEN];
    char tag[NEU_TAG_NAME_LEN];
} tkey_t;

struct elem {
    int64_t timestamp;
    bool    changed;

    neu_dvalue_t value;

    tkey_t         key;
    UT_hash_handle hh;
};

struct neu_driver_cache {
    nng_mtx *mtx;

    struct elem *table;
};

// static void update_tag_error(neu_driver_cache_t *cache, const char *group,
// const char *tag, int64_t timestamp, int error);

inline static tkey_t to_key(const char *group, const char *tag)
{
    tkey_t key = { 0 };

    strcpy(key.group, group);
    strcpy(key.tag, tag);

    return key;
}

neu_driver_cache_t *neu_driver_cache_new()
{
    neu_driver_cache_t *cache = calloc(1, sizeof(neu_driver_cache_t));

    nng_mtx_alloc(&cache->mtx);

    return cache;
}

void neu_driver_cache_destroy(neu_driver_cache_t *cache)
{
    struct elem *elem = NULL;
    struct elem *tmp  = NULL;

    nng_mtx_lock(cache->mtx);
    HASH_ITER(hh, cache->table, elem, tmp)
    {
        HASH_DEL(cache->table, elem);
        free(elem);
    }
    nng_mtx_unlock(cache->mtx);

    nng_mtx_free(cache->mtx);

    free(cache);
}

// void neu_driver_cache_error(neu_driver_cache_t *cache, const char *group,
// const char *tag, int64_t timestamp, int32_t error)
//{
// update_tag_error(cache, group, tag, timestamp, error);
//}

void neu_driver_cache_add(neu_driver_cache_t *cache, const char *group,
                          const char *tag, neu_dvalue_t value)
{
    struct elem *elem = NULL;
    tkey_t       key  = to_key(group, tag);

    nng_mtx_lock(cache->mtx);
    HASH_FIND(hh, cache->table, &key, sizeof(tkey_t), elem);

    if (elem == NULL) {
        elem = calloc(1, sizeof(struct elem));

        strcpy(elem->key.group, group);
        strcpy(elem->key.tag, tag);

        HASH_ADD(hh, cache->table, key, sizeof(tkey_t), elem);
    }

    elem->timestamp = 0;
    elem->changed   = false;
    elem->value     = value;

    nng_mtx_unlock(cache->mtx);
}

void neu_driver_cache_update(neu_driver_cache_t *cache, const char *group,
                             const char *tag, int64_t timestamp,
                             neu_dvalue_t value)
{
    struct elem *elem = NULL;
    tkey_t       key  = to_key(group, tag);

    nng_mtx_lock(cache->mtx);
    HASH_FIND(hh, cache->table, &key, sizeof(tkey_t), elem);
    if (elem != NULL) {
        elem->timestamp = timestamp;
        if (elem->value.type != value.type) {
            elem->changed = true;
        } else {
            switch (value.type) {
            case NEU_TYPE_INT8:
            case NEU_TYPE_UINT8:
            case NEU_TYPE_INT16:
            case NEU_TYPE_UINT16:
            case NEU_TYPE_INT32:
            case NEU_TYPE_UINT32:
            case NEU_TYPE_INT64:
            case NEU_TYPE_UINT64:
            case NEU_TYPE_BIT:
            case NEU_TYPE_BOOL:
            case NEU_TYPE_STRING:
            case NEU_TYPE_BYTES:
            case NEU_TYPE_WORD:
            case NEU_TYPE_DWORD:
            case NEU_TYPE_LWORD:
                if (memcmp(&elem->value.value, &value.value,
                           sizeof(value.value)) != 0) {
                    elem->changed = true;
                }
                break;
            case NEU_TYPE_FLOAT:
                if (elem->value.precision == 0) {
                    elem->changed = elem->value.value.f32 != value.value.f32;
                } else {
                    if (fabs(elem->value.value.f32 - value.value.f32) >
                        pow(0.1, elem->value.precision)) {
                        elem->changed = true;
                    }
                }
                break;
            case NEU_TYPE_DOUBLE:
                if (elem->value.precision == 0) {
                    elem->changed = elem->value.value.d64 != value.value.d64;
                } else {
                    if (fabs(elem->value.value.d64 - value.value.d64) >
                        pow(0.1, elem->value.precision)) {
                        elem->changed = true;
                    }
                }

                break;
            case NEU_TYPE_ERROR:
                elem->changed = true;
                break;
            }
        }

        elem->value.type  = value.type;
        elem->value.value = value.value;
    }

    nng_mtx_unlock(cache->mtx);
}

int neu_driver_cache_get(neu_driver_cache_t *cache, const char *group,
                         const char *tag, neu_driver_cache_value_t *value)
{
    struct elem *elem = NULL;
    int          ret  = -1;
    tkey_t       key  = to_key(group, tag);

    nng_mtx_lock(cache->mtx);
    HASH_FIND(hh, cache->table, &key, sizeof(tkey_t), elem);

    if (elem != NULL) {
        value->timestamp       = elem->timestamp;
        value->value.type      = elem->value.type;
        value->value.precision = elem->value.precision;

        switch (elem->value.type) {
        case NEU_TYPE_INT8:
        case NEU_TYPE_UINT8:
        case NEU_TYPE_BIT:
            value->value.value.u8 = elem->value.value.u8;
            break;
        case NEU_TYPE_INT16:
        case NEU_TYPE_UINT16:
        case NEU_TYPE_WORD:
            value->value.value.u16 = elem->value.value.u16;
            break;
        case NEU_TYPE_INT32:
        case NEU_TYPE_UINT32:
        case NEU_TYPE_DWORD:
        case NEU_TYPE_FLOAT:
        case NEU_TYPE_ERROR:
            value->value.value.u32 = elem->value.value.u32;
            break;
        case NEU_TYPE_INT64:
        case NEU_TYPE_UINT64:
        case NEU_TYPE_DOUBLE:
        case NEU_TYPE_LWORD:
            value->value.value.u64 = elem->value.value.u64;
            break;
        case NEU_TYPE_BOOL:
            value->value.value.boolean = elem->value.value.boolean;
            break;
        case NEU_TYPE_STRING:
        case NEU_TYPE_BYTES:
            memcpy(value->value.value.str, elem->value.value.str,
                   sizeof(elem->value.value.str));
            break;
        }

        ret = 0;
    }

    nng_mtx_unlock(cache->mtx);

    return ret;
}

int neu_driver_cache_get_changed(neu_driver_cache_t *cache, const char *group,
                                 const char *              tag,
                                 neu_driver_cache_value_t *value)
{
    struct elem *elem = NULL;
    int          ret  = -1;
    tkey_t       key  = to_key(group, tag);

    nng_mtx_lock(cache->mtx);
    HASH_FIND(hh, cache->table, &key, sizeof(tkey_t), elem);

    if (elem != NULL && elem->changed) {
        value->timestamp       = elem->timestamp;
        value->value.type      = elem->value.type;
        value->value.precision = elem->value.precision;

        switch (elem->value.type) {
        case NEU_TYPE_INT8:
        case NEU_TYPE_UINT8:
        case NEU_TYPE_BIT:
            value->value.value.u8 = elem->value.value.u8;
            break;
        case NEU_TYPE_INT16:
        case NEU_TYPE_UINT16:
        case NEU_TYPE_WORD:
            value->value.value.u16 = elem->value.value.u16;
            break;
        case NEU_TYPE_INT32:
        case NEU_TYPE_UINT32:
        case NEU_TYPE_FLOAT:
        case NEU_TYPE_ERROR:
        case NEU_TYPE_DWORD:
            value->value.value.u32 = elem->value.value.u32;
            break;
        case NEU_TYPE_INT64:
        case NEU_TYPE_UINT64:
        case NEU_TYPE_DOUBLE:
        case NEU_TYPE_LWORD:
            value->value.value.u64 = elem->value.value.u64;
            break;
        case NEU_TYPE_BOOL:
            value->value.value.boolean = elem->value.value.boolean;
            break;
        case NEU_TYPE_STRING:
        case NEU_TYPE_BYTES:
            memcpy(value->value.value.str, elem->value.value.str,
                   sizeof(elem->value.value.str));
            break;
        }

        if (elem->value.type != NEU_TYPE_ERROR) {
            elem->changed = false;
        }
        ret = 0;
    }

    nng_mtx_unlock(cache->mtx);

    return ret;
}

void neu_driver_cache_del(neu_driver_cache_t *cache, const char *group,
                          const char *tag)
{
    struct elem *elem = NULL;
    tkey_t       key  = to_key(group, tag);

    nng_mtx_lock(cache->mtx);
    HASH_FIND(hh, cache->table, &key, sizeof(tkey_t), elem);

    if (elem != NULL) {
        HASH_DEL(cache->table, elem);
        free(elem);
    }

    nng_mtx_unlock(cache->mtx);
}
