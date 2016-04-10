
/**
 * @file hmap.c
 *
 * @brief string to object hashmap
 */
#include <string.h>
#include <stdint.h>
#include "hmap.h"
#include "kvec.h"
#include "log.h"
#include "sassert.h"

#define UNITTEST_UNIQUE_ID			55
#define UNITTEST 					1

#include "unittest.h"

/* inline directive */
#define _force_inline				inline

/* max, min */
#define MAX2(x, y)					( (x) < (y) ? (y) : (x) )
#define MIN2(x, y)					( (x) > (y) ? (y) : (x) )


/**
 * MurmurHash3 string hashing function,
 * extracted from https://github.com/aappleby/smhasher
 * modified to make the functions static and to return
 * hashed value directly.
 */
static _force_inline
uint32_t rotl32(uint32_t x,int8_t r)
{
	return((x << r) | (x >> (32 - r)));
}

#define BIG_CONSTANT(x) (x##LLU)

//-----------------------------------------------------------------------------
// Block read - if your platform needs to do endian-swapping or can only
// handle aligned reads, do the conversion here

static _force_inline
uint32_t getblock32(const uint32_t *p, int i)
{
	return(p[i]);
}

//-----------------------------------------------------------------------------
// Finalization mix - force all bits of a hash block to avalanche

static _force_inline
uint32_t fmix32(uint32_t h)
{
	h ^= h >> 16;
	h *= 0x85ebca6b;
	h ^= h >> 13;
	h *= 0xc2b2ae35;
	h ^= h >> 16;

	return(h);
}

//-----------------------------------------------------------------------------
static _force_inline
uint32_t MurmurHash3_x86_32(
	const void *key,
	int32_t len,
	uint32_t seed)
{
	const uint8_t * data = (const uint8_t*)key;
	const int nblocks = len / 4;

	uint32_t h1 = seed;

	const uint32_t c1 = 0xcc9e2d51;
	const uint32_t c2 = 0x1b873593;

	//----------
	// body

	const uint32_t * blocks = (const uint32_t *)(data + nblocks*4);

	for(int i = -nblocks; i; i++)
	{
		uint32_t k1 = getblock32(blocks,i);

		k1 *= c1;
		k1 = rotl32(k1,15);
		k1 *= c2;

		h1 ^= k1;
		h1 = rotl32(h1,13); 
		h1 = h1*5+0xe6546b64;
	}

	//----------
	// tail

	const uint8_t * tail = (const uint8_t*)(data + nblocks*4);

	uint32_t k1 = 0;

	switch(len & 3)
	{
	case 3: k1 ^= tail[2] << 16;
	case 2: k1 ^= tail[1] << 8;
	case 1: k1 ^= tail[0];
		k1 *= c1; k1 = rotl32(k1,15); k1 *= c2; h1 ^= k1;
	};

	//----------
	// finalization

	h1 ^= len;

	h1 = fmix32(h1);

	return(h1);
}
/* end of MurmurHash3.cpp */



/**
 * MurmurHash3 wrapper functions
 */
static _force_inline
uint32_t hash_string(
	char const *str,
	int32_t len)
{
	return(MurmurHash3_x86_32(
		(void const *)str,
		(int)len,
		0xcafebabe));
}
static _force_inline
uint32_t hash_uint32(uint32_t val)
{
	return(MurmurHash3_x86_32(
		(void const *)&val,
		4,
		val));
}

/**
 * @struct hmap_header_intl_s
 */
struct hmap_header_intl_s {
	uint32_t obj_id;
	uint32_t key_len;
	uint64_t key_base;
};
_static_assert(sizeof(struct hmap_header_intl_s) == sizeof(struct hmap_header_s));
_static_assert_offset(struct hmap_header_intl_s, obj_id, struct hmap_header_s, id, 0);

/**
 * @struct hmap_pair_s
 */
struct hmap_pair_s {
	uint32_t id;
	uint32_t hash_val;
};
_static_assert(sizeof(struct hmap_pair_s) == 8);

/**
 * @struct hmap_intl_s
 */
struct hmap_intl_s {
	uint32_t mask;
	uint32_t object_size;
	kvec_t(uint8_t) key_arr;
	kvec_t(uint8_t) object_arr;
	uint32_t next_id;
	uint32_t reserved;
	struct hmap_pair_s *table;
};
_static_assert_offset(struct hmap_intl_s, object_arr.a, struct hmap_s, object, 0);

/**
 * @fn hmap_init
 */
hmap_t *hmap_init(
	uint64_t hmap_size,
	uint64_t object_size)
{
	/* size must be power of 2 */
	if(hmap_size == 0 || (hmap_size & (hmap_size - 1)) != 0) {
		return(NULL);
	}

	/* malloc mem */
	struct hmap_intl_s *hmap = malloc(sizeof(struct hmap_intl_s));
	struct hmap_pair_s *table = malloc(sizeof(struct hmap_pair_s) * hmap_size);
	if(hmap == NULL || table == NULL) {
		goto _hmap_init_error_handler;
	}

	/* init context */
	hmap->mask = (uint32_t)hmap_size - 1;
	hmap->object_size = object_size;
	hmap->next_id = 0;
	hmap->table = table;
	kv_init(hmap->key_arr);
	kv_init(hmap->object_arr);

	/* init hashmap with invalid mark */
	memset(hmap->table, 0xff, sizeof(struct hmap_pair_s) * hmap_size);
	return((hmap_t *)hmap);

_hmap_init_error_handler:;
	if(hmap != NULL) { free(hmap); }
	if(table != NULL) { free(table); }
	return(NULL);
}

/**
 * @fn hmap_clean
 */
void hmap_clean(
	hmap_t *_hmap)
{
	struct hmap_intl_s *hmap = (struct hmap_intl_s *)_hmap;

	if(hmap != NULL) {
		if(kv_ptr(hmap->key_arr) != NULL) {
			kv_destroy(hmap->key_arr);
		}
		if(kv_ptr(hmap->object_arr) != NULL) {
			kv_destroy(hmap->object_arr);
		}
		if(hmap->table != NULL) {
			free(hmap->table);
		}
		free(hmap);
	}
	return;
}

/**
 * @fn hmap_object_get_ptr
 */
static _force_inline
struct hmap_header_intl_s *hmap_object_get_ptr(
	struct hmap_intl_s *hmap,
	uint32_t id)
{
	return((struct hmap_header_intl_s *)(kv_ptr(hmap->object_arr) + (uint64_t)id * hmap->object_size));
}

/**
 * @fn hmap_object_get_key
 */
static _force_inline
struct hmap_key_s hmap_object_get_key(
	struct hmap_intl_s *hmap,
	uint32_t id)
{
	struct hmap_header_intl_s *obj = hmap_object_get_ptr(hmap, id);
	return((struct hmap_key_s){
		.str = (char const *)kv_ptr(hmap->key_arr) + obj->key_base,
		.len = obj->key_len
	});
}

/**
 * @fn hmap_get_key
 */
struct hmap_key_s hmap_get_key(
	hmap_t *_hmap,
	uint32_t id)
{
	return(hmap_object_get_key((struct hmap_intl_s *)_hmap, id));
}

/**
 * @fn hmap_expand
 */
static _force_inline
void hmap_expand(
	struct hmap_intl_s *hmap)
{
	uint32_t prev_mask = hmap->mask;
	uint32_t prev_size = prev_mask + 1;
	uint32_t size = 2 * prev_size;
	uint32_t mask = size - 1;

	/* realloc */
	hmap->table = (struct hmap_pair_s *)realloc(hmap->table,
		sizeof(struct hmap_pair_s) * (uint64_t)size);

	/* update context */
	hmap->mask = mask;

	/* init expanded area with invalid mark */
	memset(&hmap->table[prev_size], 0xff, sizeof(struct hmap_pair_s) * prev_size);

	/* rehash */
	for(int64_t i = 0; i < prev_size; i++) {
		/* check id */
		uint32_t const invalid_id = (uint32_t)-1;
		uint32_t id = hmap->table[i].id;
		if(id == invalid_id) { continue; }

		/* check if move is needed */
		uint32_t base_hash_val = hmap->table[i].hash_val;
		if((base_hash_val & mask) == i) { continue; }

		/* move */
		uint32_t hash_val = base_hash_val;
		while(hmap->table[hmap->mask & hash_val].id != invalid_id) {
			hash_val = hash_uint32(hash_val);
		}
		hmap->table[i] = (struct hmap_pair_s){ (uint32_t)-1, (uint32_t)-1 };
		hmap->table[hmap->mask & hash_val] = (struct hmap_pair_s){
			.id = id,
			.hash_val = base_hash_val
		};
		debug("move id(%u) form %lld to %u", id, i, hmap->mask & hash_val);
	}
	debug("expanded, mask(%u)", hmap->mask);
	return;
}

/**
 * @fn hmap_get_id
 */
uint32_t hmap_get_id(
	hmap_t *_hmap,
	struct hmap_key_s key)
{
	struct hmap_intl_s *hmap = (struct hmap_intl_s *)_hmap;

	uint32_t const invalid_id = (uint32_t)-1;
	uint32_t id = invalid_id;
	uint32_t tmp_id = invalid_id;

	uint32_t base_hash_val = hash_string(key.str, key.len);
	uint32_t hash_val = base_hash_val;

	while((tmp_id = hmap->table[hmap->mask & hash_val].id) != invalid_id) {
		struct hmap_key_s ex_key = hmap_object_get_key(hmap, tmp_id);
		debug("ex_key at %u: str(%p), len(%d)", tmp_id, ex_key.str, ex_key.len);

		/* compare string */
		if(ex_key.len == key.len && strncmp(ex_key.str, key.str, MIN2(ex_key.len, key.len) + 1) == 0) {
			/* matched with existing string in the section array */
			id = tmp_id; break;
		}

		debug("collision at %u, key(%s), ex_key(%s), hash_val(%x)",
			hmap->mask & hash_val, key.str, ex_key.str, hash_uint32(hash_val));

		/* not matched, rehash */
		hash_val = hash_uint32(hash_val);
	}

	if(id == invalid_id) {
		/* rehash if occupancy exceeds 0.5 */
		if(hmap->next_id > (hmap->mask + 1) / 2) {
			debug("check size next_id(%u), size(%u)",
				hmap->next_id, (hmap->mask + 1) / 2);
			hmap_expand(hmap);
		}

		/* add (hash_val, id) pair to table */
		debug("id(%u), mask(%x), hash_val(%x), id(%u), base_hash_val(%x)",
			hmap->mask & hash_val, hmap->mask, hash_val, hmap->next_id, base_hash_val);
		hmap->table[hmap->mask & hash_val] = (struct hmap_pair_s){
			.id = (id = hmap->next_id++),
			.hash_val = base_hash_val
		};

		/* reserve working area */
		uint8_t tmp[hmap->object_size];
		struct hmap_header_intl_s *h = (struct hmap_header_intl_s *)tmp;

		/* push key string to key_arr */
		h->obj_id = id;
		h->key_len = key.len;
		h->key_base = kv_size(hmap->key_arr);
		kv_pushm(hmap->key_arr, key.str, key.len);
		kv_push(hmap->key_arr, '\0');

		/* add object to object array */
		kv_pushm(hmap->object_arr, tmp, hmap->object_size);
	}
	return(id);
}

/**
 * @fn hmap_get_object
 */
void *hmap_get_object(
	hmap_t *hmap,
	uint32_t id)
{
	return((void *)hmap_object_get_ptr((struct hmap_intl_s *)hmap, id));
}

/**
 * @fn hmap_get_count
 */
uint32_t hmap_get_count(
	hmap_t *_hmap)
{
	struct hmap_intl_s *hmap = (struct hmap_intl_s *)_hmap;
	return(hmap->next_id);
}


/* unittests */
unittest_config(
	.name = "hmap",
);

#define make_string(x) ({ \
	char buf[128]; \
	sprintf(buf, "key-%lld", (int64_t)(x)); \
	(struct hmap_key_s){ \
		.str = buf, \
		.len = strlen(buf) \
	}; \
})

#define UNITTEST_HASH_SIZE			( 128 )
#define UNITTEST_KEY_COUNT			( 32768 )

/* create context */
unittest()
{
	/* valid hash size */
	hmap_t *hmap = hmap_init(UNITTEST_HASH_SIZE, sizeof(hmap_header_t));

	assert(hmap != NULL, "%p", hmap);
	hmap_clean(hmap);

	/* invalid hash size */
	hmap = hmap_init(127, 16);
	
	assert(hmap == NULL, "%p", hmap);
	hmap_clean(hmap);
}

/* append and get */
unittest()
{
	hmap_t *hmap = hmap_init(UNITTEST_HASH_SIZE, sizeof(hmap_header_t));

	/* append key */
	for(int64_t i = 0; i < UNITTEST_KEY_COUNT; i++) {
		uint32_t id = hmap_get_id(hmap, make_string(i));
		
		assert((int64_t)id == i, "i(%lld), id(%lld)", i, id);
	}
	assert(hmap_get_count(hmap) == UNITTEST_KEY_COUNT, "count(%u)", hmap_get_count(hmap));

	/* get key */
	for(int64_t i = 0; i < UNITTEST_KEY_COUNT; i++) {
		struct hmap_key_s a = make_string(i);
		struct hmap_key_s b = hmap_get_key(hmap, i);

		assert(a.len == b.len, "a(%d), b(%d)", a.len, b.len);
		assert(strcmp(a.str, b.str) == 0, "a(%s), b(%s)", a.str, b.str);
	}
	assert(hmap_get_count(hmap) == UNITTEST_KEY_COUNT, "count(%u)", hmap_get_count(hmap));

	/* get key in reverse order */
	for(int64_t i = UNITTEST_KEY_COUNT - 1; i >= 0; i--) {
		struct hmap_key_s a = make_string(i);
		struct hmap_key_s b = hmap_get_key(hmap, i);

		assert(a.len == b.len, "a(%d), b(%d)", a.len, b.len);
		assert(strcmp(a.str, b.str) == 0, "a(%s), b(%s)", a.str, b.str);
	}
	assert(hmap_get_count(hmap) == UNITTEST_KEY_COUNT, "count(%u)", hmap_get_count(hmap));

	hmap_clean(hmap);
}

/* with different object size */
unittest()
{
	/* 32 */
	hmap_t *hmap = hmap_init(UNITTEST_HASH_SIZE, sizeof(hmap_header_t) + 32);
	for(int64_t i = 0; i < UNITTEST_KEY_COUNT; i++) {
		uint32_t id = hmap_get_id(hmap, make_string(i));
		assert((int64_t)id == i, "i(%lld), id(%lld)", i, id);
	}

	for(int64_t i = 0; i < UNITTEST_KEY_COUNT; i++) {
		struct hmap_key_s a = make_string(i);
		struct hmap_key_s b = hmap_get_key(hmap, i);
		assert(a.len == b.len, "a(%d), b(%d)", a.len, b.len);
		assert(strcmp(a.str, b.str) == 0, "a(%s), b(%s)", a.str, b.str);
	}
	hmap_clean(hmap);

	/* 48 */
	hmap = hmap_init(UNITTEST_HASH_SIZE, sizeof(hmap_header_t) + 48);
	for(int64_t i = 0; i < UNITTEST_KEY_COUNT; i++) {
		uint32_t id = hmap_get_id(hmap, make_string(i));
		assert((int64_t)id == i, "i(%lld), id(%lld)", i, id);
	}

	for(int64_t i = 0; i < UNITTEST_KEY_COUNT; i++) {
		struct hmap_key_s a = make_string(i);
		struct hmap_key_s b = hmap_get_key(hmap, i);
		assert(a.len == b.len, "a(%d), b(%d)", a.len, b.len);
		assert(strcmp(a.str, b.str) == 0, "a(%s), b(%s)", a.str, b.str);
	}
	hmap_clean(hmap);

	/* 128 */
	hmap = hmap_init(UNITTEST_HASH_SIZE, sizeof(hmap_header_t) + 128);
	for(int64_t i = 0; i < UNITTEST_KEY_COUNT; i++) {
		uint32_t id = hmap_get_id(hmap, make_string(i));
		assert((int64_t)id == i, "i(%lld), id(%lld)", i, id);
	}

	for(int64_t i = 0; i < UNITTEST_KEY_COUNT; i++) {
		struct hmap_key_s a = make_string(i);
		struct hmap_key_s b = hmap_get_key(hmap, i);
		assert(a.len == b.len, "a(%d), b(%d)", a.len, b.len);
		assert(strcmp(a.str, b.str) == 0, "a(%s), b(%s)", a.str, b.str);
	}
	hmap_clean(hmap);
}

/* get_object */
unittest()
{
	struct str_cont_s {
		hmap_header_t header;
		char s[128];
	};
	hmap_t *hmap = hmap_init(UNITTEST_HASH_SIZE,
		sizeof(struct str_cont_s));

	/* append key */
	for(int64_t i = 0; i < UNITTEST_KEY_COUNT; i++) {
		uint32_t id = hmap_get_id(hmap, make_string(i));
		struct str_cont_s *obj = hmap_get_object(hmap, id);

		assert(obj != NULL, "obj(%p)", obj);

		strcpy(obj->s, make_string(i).str);
	}

	/* get key */
	for(int64_t i = 0; i < UNITTEST_KEY_COUNT; i++) {
		struct str_cont_s *obj = hmap_get_object(hmap, i);

		assert(obj != NULL, "obj(%p)", obj);
		assert(strcmp(obj->s, make_string(i).str) == 0, "%s, %s", obj->s, make_string(i).str);
	}

	/* get key in reverse order */
	for(int64_t i = UNITTEST_KEY_COUNT - 1; i >= 0; i--) {
		struct str_cont_s *obj = hmap_get_object(hmap, i);

		assert(obj != NULL, "obj(%p)", obj);
		assert(strcmp(obj->s, make_string(i).str) == 0, "%s, %s", obj->s, make_string(i).str);
	}

	hmap_clean(hmap);
}

/**
 * end of hmap.c
 */
