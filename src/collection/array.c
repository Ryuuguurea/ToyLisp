#include "array.h"
#include "stdlib.h"
#include "string.h"
#include "stdio.h"
Array* array_create(int size){
    Array* self = malloc(sizeof(Array));
    self->item_size = size;
    self->data_size = 1;
    self->size = 0;
    self->data = malloc(self->data_size*self->item_size);
    return self;
}
void array_destroy(Array* self){
    free(self->data);
    free(self);
}
void array_resize(Array* self,int new_size){
    void* new_data = malloc(new_size * self->item_size);
    int copy_size = 0;
    int array_size = self->size;
    if(new_size > self->data_size){
        copy_size = self->data_size;
    }else{
        copy_size = new_size;
        array_size = new_size;
    }
    memcpy(new_data,self->data,copy_size*self->item_size);
    free(self->data);
    self->data_size = new_size;
    self->data = new_data;
    self->size = array_size;
}
void array_push(Array* self,void* item){
    if(self->size+1 > self->data_size){
        array_resize(self,self->data_size* 2);
    }
    memcpy((char*)(self->data)+self->size*self->item_size,item,self->item_size);
    self->size++;
}
void* array_get(Array* self,int index){
    return (char*)(self->data)+index*self->item_size;
}
void array_clear(Array* self){
    self->size = 0;
}
void array_append(Array* self,Array* other){

    if(self->data_size < self->size+other->size){
        int new_size = self->data_size;
        while(new_size < self->size+other->size){
            new_size *= 2;
        }
        array_resize(self,new_size);
    }
    memcpy((char*)self->data+self->size*self->item_size,
        other->data,
        other->size*self->item_size
    );
    self->size+=other->size;
}
void* array_shift(Array* self){
    void* res = malloc(self->item_size);
    void* new_data = malloc(self->data_size*self->item_size);
    memcpy(res,self->data,self->item_size);
    memcpy(new_data,(char*)self->data+self->item_size,self->item_size*self->size--);
    free(self->data);
    self->data = new_data;
    return res;
}
void array_pop(Array* self){
    self->size--;
}