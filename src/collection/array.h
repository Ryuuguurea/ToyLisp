typedef struct Array{
    void* data;
    int size;
    int item_size;
    int data_size;
}Array;

Array* array_create(int size);
void array_destroy(Array* self);
void array_push(Array* self,void* item);
void* array_get(Array* self,int index);
void array_clear(Array* self);
void array_append(Array* self,Array* other);
void* array_shift(Array* self);