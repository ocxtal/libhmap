# libhmap

Libhmap is a string -> object hashmap with intermediate unique ID.

## Usage

```
struct object {
	hmap_header_t header;
	
	/* user-defined member follow header */
	int member1;
	int member2;
	/* ... */
};

/* create hashmap with initial size 128 */
hmap_t *h = hmap_init(128, sizeof(struct object));

/* reserve area for key0 */
uint32_t id = hmap_get_id(h, (struct hmap_key_s){
	.str = "key0",
	.len = strlen("key0")
});

/* get pointer to the reserved area for the object */
struct object *obj = hmap_get_object(h, id);

/* reverse mapping from id to key */
struct hmap_key_s key = hmap_get_key(h, id);

/* cleanup */
hmap_clean(h);
```

## License

MIT
