#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> 
#include <time.h>
#define _POSIX_C_SOURCE 200809L
#include <string.h>
#include <pthread.h>
#include <stdbool.h>

int counter;
pthread_mutex_t readerLock;
pthread_mutex_t writerLock;
int readCount = 0;
#define MAX_INSERT_THREADS 100
#define MAX_COMPARE_THREADS 1
#define KEY_LENGTH 3
#define VALUE_LENGTH 6
#define NO_OF_BUCKETS 256
//creates items for the hash table

typedef struct _ht_item {
	_ht_item *next;
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
typedef struct {
	ht_hash_table *ht;
	char *key;
	char *value;
} insert_item_t;

typedef struct {
	ht_hash_table *ht;
	char *key;
} search_item_t;

int ht_calc_hash(const char* key)
{
	int h = 5381;
	while (*(key++))
		h = ((h << 5) + h) + (unsigned char)(*key);
	return h % NO_OF_BUCKETS; //places the string into a random bucket from 0 to 255 decided by the hash of the string
}

ht_item* ht_new_item(const char* k, const char* v)
{
	ht_item* i = (ht_item*)malloc(sizeof(ht_item));
	i->key = strdup(k);
	i->value = strdup(v);
	i->next = (ht_item *)0; //initializes to NULL for last entry like Linked List
	return i;
}

static void hashDeleteItem(ht_item *i)
{

	//deallocate the key then value then the ptr
	free(i->key);
	free(i->value);
	free(i);
}

void ht_del_hash_table(ht_hash_table* ht)
{
	pthread_mutex_lock(&writerLock);
	//using hashDeleteItem, deletes each item, similar to deleting nodes in linked list
	int size = ht->size;
	int i;
	for (i = 0; i < size; i++)
	{
		ht_item* itemPtr = ht->items[i];
		//deletes each item until NULL
		if (itemPtr != (void *)0)
			hashDeleteItem(itemPtr);
	}
	//then deallocate the ht item and ht itself
	free(ht->items);
	free(ht);
	pthread_mutex_unlock(&writerLock);
}

void ht_insert(ht_hash_table* ht, const char* key, const char* value)
{
	pthread_mutex_lock(&writerLock);
	ht_item* item = ht_new_item(key, value);
	int hash = ht_calc_hash(item->key);
	ht_item* my_bucket = ht->items[hash];
	if (!my_bucket) {
		ht->items[hash] = item;
	}
	else {
		while (my_bucket->next) {
			my_bucket = my_bucket->next;
		}
		my_bucket->next = item;
	}
	ht->count++;
	ht->size++;
	pthread_mutex_unlock(&writerLock);
}

char* ht_search(ht_hash_table* ht, const char* key)
{
	//reader
	pthread_mutex_lock(&readerLock);
	readCount++;
	if (readCount == 1)
	{
		pthread_mutex_lock(&writerLock);
	}
	pthread_mutex_unlock(&readerLock);
	int hash = ht_calc_hash(key);
	ht_item* my_bucket = ht->items[hash];
	if (!my_bucket) {
		return (char *)0;
	}
	while (my_bucket) {
		if (strcmp(my_bucket->key, key) == 0) {
			return my_bucket->value;
		}
		my_bucket = my_bucket->next;
	}
	//release reader
	pthread_mutex_lock(&readerLock);
	readCount--;
	if (readCount == 0)
	{
		pthread_mutex_unlock(&writerLock);
	}
	pthread_mutex_unlock(&readerLock);
	return (char *)0;
}

void ht_delete(ht_hash_table* ht, const char* key)
{
	pthread_mutex_lock(&writerLock);
	int hash = ht_calc_hash(key);
	ht_item* my_bucket = ht->items[hash];
	if (!my_bucket) {
		return;
	}
	ht_item* prev = my_bucket;
	while (my_bucket) {
		if (strcmp(my_bucket->key, key) == 0) {
			prev->next = my_bucket->next;
			hashDeleteItem(my_bucket);
			return;
		}
		my_bucket = my_bucket->next;
	}
	ht->count--;
	ht->size--;
	pthread_mutex_unlock(&writerLock);
}

void *insert_worker(void* in)
{
	insert_item_t *insert_item = (insert_item_t*)in;
	printf("Inserting key: %s, value: %s\n", insert_item->key, insert_item->value);
	ht_insert(insert_item->ht, insert_item->key, insert_item->value);
	free(insert_item);
	return (void *)0;
}

void *search(void *in)
{
	search_item_t *search_item = (search_item_t*)in;
	printf("Searching for key: %s\n", search_item->key);
	char *value = ht_search(search_item->ht, search_item->key);
	return (void*)value;
}

char *random_string(const int size, char **blacklist, size_t blacklist_count)
{
	char upper = 90;
	char lower = 65;

	char *buffer = (char *)calloc(size + 1, sizeof(char));
	FILE *f = fopen("/dev/urandom", "r");
	fread(buffer, size, sizeof(char), f);
	for (int i = 0; i < size; i++) {
		if (buffer[i] < 0) {
			buffer[i] = -buffer[i];
		}
		buffer[i] = (buffer[i] % (upper - lower + 1)) + lower;
	}
	fclose(f);

	return buffer;
}

ht_hash_table* ht_new() {
	int size = sizeof(ht_hash_table);
	int i;
	ht_hash_table *ht = (ht_hash_table *)malloc(size);
	ht->count = 0;
	ht->size = 0;
	ht->items = (ht_item **)malloc(sizeof(ht_item *) * NO_OF_BUCKETS);
	for (i = 0; i < NO_OF_BUCKETS; i++) {
		ht->items[i] = (ht_item *)0;
	}
	return ht;
}


int main() {
	//creates ht
	ht_hash_table *ht = ht_new();
	pthread_t insert_threads[MAX_INSERT_THREADS];
	pthread_t compare_threads[MAX_COMPARE_THREADS];

	if (pthread_mutex_init(&readerLock, NULL) != 0)
	{
		printf("\n mutex init has failed\n");
		return 1;
	}
	if (pthread_mutex_init(&writerLock, NULL) != 0)
	{
		printf("\n mutex init has failed\n");
		return 1;
	}

	int mismatch_count = 0;
	char *keys[MAX_INSERT_THREADS];
	char *values[MAX_INSERT_THREADS];

	for (int i = 0; i < MAX_INSERT_THREADS; i++) {
		keys[i] = random_string(KEY_LENGTH, keys, MAX_INSERT_THREADS);
		values[i] = random_string(VALUE_LENGTH, values, MAX_INSERT_THREADS);
		insert_item_t *insert_item = (insert_item_t *)malloc(sizeof(insert_item_t));
		insert_item->ht = ht;
		insert_item->key = keys[i];
		insert_item->value = values[i];
		pthread_create(&insert_threads[i], (pthread_attr_t *)0, insert_worker, (void*)insert_item);
	}

	for (int i = 0; i < MAX_INSERT_THREADS; i++) {
		pthread_join(insert_threads[i], (void **)0);
	}

	for (int i = 0; i < MAX_INSERT_THREADS;) {
		for (int j = 0; j < MAX_COMPARE_THREADS && i < MAX_INSERT_THREADS; i++, j++) {
			search_item_t search_item = { ht, keys[i] };
			pthread_create(&compare_threads[i], (pthread_attr_t *)0, search, &search_item);
			char *result;
			pthread_join(compare_threads[i], (void**)&result);
			if (strcmp(result, values[i]) != 0) {
				printf("Mismatch. Expected:%s - Obtained: %s\n", values[i], result);
				mismatch_count++;
			}
		}
		for (int i = 0; i < MAX_COMPARE_THREADS; i++) {
			pthread_join(compare_threads[i], (void **)0);
		}
	}

	for (int i = 0; i < MAX_INSERT_THREADS; i++) {
		free(keys[i]);
		free(values[i]);
	}

	printf("Process ended. Mismatches found: %d", mismatch_count);

	return 0;
}