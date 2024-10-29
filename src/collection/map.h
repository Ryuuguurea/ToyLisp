

typedef struct Map
{
    void** bucket;
    int bucket_size;
    int item_size;
    int node_num;
}Map;
typedef struct MapIter{
    int bucket_idx;
    void* node;
}MapIter;

Map* map_create(int item_size);
void map_destroy(Map* self);
void map_insert(Map* self,char* key,void* value);
void* map_get(Map*self,char* key);
void map_remove(Map* self,char* key);
MapIter map_iter(Map*);
char* map_next(Map*,MapIter*);