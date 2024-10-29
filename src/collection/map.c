#include "map.h"
#include "string.h"
#include "stdlib.h"
struct MapNode;
typedef struct MapNode{
    int hash;
    char* key;
    void* value;
    void* next;
}MapNode;

static int map_hash(char* key){
    int hash = 5381;
    while (*key)
    {
        hash=((hash<<5) + hash)^ *key++;
    }
    return hash;
}
static int map_bucket_idx(Map* self,int hash){
    return hash&(self->bucket_size-1);
}
static MapNode* map_get_node(Map* self,char* key){
    int hash = map_hash(key);
    int idx = map_bucket_idx(self,hash);
    MapNode* next = self->bucket[idx];
    while (next)
    {
        if(!strcmp(key,next->key)){
            return next;
        }
        next = next->next;
    }
    return NULL;

}
static void map_add_node(Map* self,MapNode* node){
    int idx = map_bucket_idx(self,node->hash);
    node->next = self->bucket[idx];
    self->bucket[idx] = node;
}
static void map_resize(Map* self,int new_size){
    int i = self->bucket_size;
    MapNode* node = NULL;
    MapNode* next = NULL;
    MapNode* head = NULL;
    while (i--)
    {
        node = self->bucket[i];
        while(node){
            next = node->next;
            node->next = head;
            head = node;
            node = next;
        } 
    }
    self->bucket = realloc(self->bucket,sizeof(MapNode*)*new_size);
    self->bucket_size = new_size;
    memset(self->bucket,0,sizeof(MapNode*)*new_size);
    node = head;
    while(node){
        next = node->next;
        map_add_node(self,node);
        node = next;
    }
}
static void* map_new_node(Map* self,char* key){
    int hash = map_hash(key);

    MapNode* node = malloc(sizeof(MapNode));
    node->hash = hash;
    node->next = NULL;
    node->key = malloc(strlen(key)+1);
    strcpy(node->key,key);
    node->value = malloc(self->item_size);

    if(self->node_num>=self->bucket_size){
        map_resize(self,self->bucket_size*2);
    }

    map_add_node(self,node);
    self->node_num++;
    return node;
}

Map* map_create(int item_size){
    Map* self = malloc(sizeof(Map));
    self->bucket_size = 1;
    self->item_size = item_size;
    self->node_num = 0;
    self->bucket = malloc(self->bucket_size*sizeof(MapNode*));
    memset(self->bucket,0,self->bucket_size*sizeof(MapNode*));

    return self;
}
void map_destroy(Map* self){
    MapNode* next = NULL;
    MapNode* node = NULL;
    int i = self->bucket_size;
    while (i--)
    {
        node = self->bucket[i];
        while (node)
        {
            next = node->next;
            free(node);
            node = next;
        }
    }
    free(self->bucket);
    free(self);
}
void map_insert(Map* self,char* key,void* value){
    MapNode* node = map_get_node(self,key);
    if(!node){
        node = map_new_node(self,key);
    }
    memcpy(node->value,value,self->item_size);

}
void* map_get(Map*self,char* key){
    MapNode* node = map_get_node(self,key);
    if(node){
        return node->value;
    }
    return NULL;
}
void map_remove(Map* self,char* key){
    int hash = map_hash(key);
    int idx = map_bucket_idx(self,hash);
    MapNode* node = map_get_node(self,key);
    if(node){
        self->bucket[idx] = node->next;
        free(node->value);
        free(node);
        self->node_num--;
    }
}

MapIter map_iter(Map*self){
    MapIter iter;
    iter.node = NULL;
    iter.bucket_idx = -1;
    return iter;
}
char* map_next(Map*self,MapIter*iter){
    MapNode* next_node;
    if(iter->node && ((MapNode*)iter->node)->next){
        next_node = ((MapNode*)iter->node)->next;
    }else{

        do{
            if(++iter->bucket_idx>=self->bucket_size){
                return NULL;
            }
            next_node = self->bucket[iter->bucket_idx];
        }while(next_node == NULL);

    }
    iter->node = next_node;
    return next_node->key;
}