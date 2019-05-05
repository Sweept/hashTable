/**
 * HashTable
 */

 //creates items for the hash table

typedef struct _ht_item {
	void *next;
	char* key;
	char* value;
} ht_item;

//creates a hash table using ht_item's items(key and value)
typedef struct {
	// Add your implementation here.
	int size;
	int count;
	ht_item** items;
} ht_hash_table;


ht_hash_table* ht_new();
void ht_del_hash_table(ht_hash_table* ht);
void ht_insert(ht_hash_table* ht, const char* key, const char* value);
char* ht_search(ht_hash_table* ht, const char* key);
void ht_delete(ht_hash_table* h, const char* key);