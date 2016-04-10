
/**
 * @file hmap.h
 *
 * @brief string to object hashmap
 */
#ifndef _HMAP_H_INCLUDED
#define _HMAP_H_INCLUDED

#include <stdint.h>

/**
 * @type hmap_header_t
 * @brief object must have a hmap_header_t field at the head.
 */
struct hmap_header_s {
	uint32_t reserved1[2];
	uint64_t reserved2;
};
typedef struct hmap_header_s hmap_header_t;

/**
 * @struct hmap_s
 */
struct hmap_s {
	uint32_t reserved1[2];
	uint64_t reserved2[2];
	void *reserved3;
	uint64_t reserved4[2];
	void *object;
	uint32_t reserved5[2];
	void *reserved6;
};
typedef struct hmap_s hmap_t;

/**
 * @struct hmap_key_s
 * @brief return value container for get_key function
 */
struct hmap_key_s {
	char const *str;
	int32_t len;
};
typedef struct hmap_key_s hmap_key_t;

/**
 * @fn hmap_init
 */
hmap_t *hmap_init(
	uint64_t hmap_size,
	uint64_t object_size);

/**
 * @fn hmap_clean
 */
void hmap_clean(
	hmap_t *hmap);

/**
 * @fn hmap_get_id
 * @brief returns index in the object array
 */
uint32_t hmap_get_id(
	hmap_t *hmap,
	struct hmap_key_s key);

/**
 * @fn hmap_get_key
 */
struct hmap_key_s hmap_get_key(
	hmap_t *hmap,
	uint32_t id);

/**
 * @fn hmap_get_object
 */
void *hmap_get_object(
	hmap_t *hmap,
	uint32_t id);

#endif /* _HMAP_H_INCLUDED */
/**
 * end of hmap.h
 */
