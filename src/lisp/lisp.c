#include "lisp.h"
#include "../collection/map.h"
#include "stdio.h"
#include "../collection/array.h"
#include "string.h"
#include "stdlib.h"
#include "ctype.h"

//forward
Exp* exp_new(VM* vm);
Exp* eval(Exp* env,Exp* exp);
Exp* apply_proc(Exp* env,Exp* proc,Exp* args);
//syntax
Array* tokenize(char* str){
    Array* res = array_create(sizeof(char*));
    Array* buf = array_create(sizeof(char));
    int index = 0;
    char* item = NULL;
    while(str[index]){
        if(str[index]=='('||str[index]==')'||str[index]==' '){
            if(buf->size>0){
                item = malloc((buf->size+1)*sizeof(char));
                item[buf->size] = '\0';
                memcpy(item,buf->data,buf->size*sizeof(char));
                array_push(res,&item);
                array_clear(buf);
            }
            if(str[index]=='('||str[index]==')'){
                item = malloc(2*sizeof(char));
                item[0] = str[index];
                item[1] = '\0';
                array_push(res,&item);
            }
        }else{
            array_push(buf,&str[index]);
        }
        index++;
    }
    array_destroy(buf);
    return res;
}


Exp* atom(VM* vm, char* str){
    Exp* res = exp_new(vm);
    int idx = 0;
    int is_number = 1;
    while(str[idx]){
        is_number = is_number && (isdigit(str[idx])||str[idx]=='.'||(str[idx]=='-' && idx !=0));
        idx++;
    }
    if(is_number){
        res->type = ExpTypeNum;
        res->number = atof(str);
        free(str);
    }else{
        res->type = ExpTypeSymbol;
        res->symbol = str;
    }
    return res;
}
Exp* read_from_tokens(VM* vm,Array* tokens){
    Exp* list_value = NULL;
    char** token_ptr = array_shift(tokens);
    if(strcmp(*token_ptr,"(")==0){
        free(*token_ptr);
        free(token_ptr);
        list_value = exp_new(vm);
        list_value->type = ExpTypeList;
        list_value->list = array_create(sizeof(Exp*));
        list_value->flags |= ExpFlagRoot;
        while (strcmp(*(char**)array_get(tokens,0),")")!=0)
        {
            Exp* left_value = read_from_tokens(vm,tokens);
            array_push(list_value->list,&left_value); 
        }
        list_value->flags &= ~ExpFlagRoot;
        token_ptr = array_shift(tokens);
        free(*token_ptr);
        free(token_ptr);
    }else if(strcmp(*token_ptr,")")!=0){
        list_value = atom(vm,*token_ptr);
    }
    return list_value;
}

Exp** env_find(Exp* self,char* var){
    Exp** e = map_get(self->env.map,var);
    if(!e&&self->env.outer){
        return env_find(self->env.outer,var);
    }
    return e;
}
void env_set(Exp*self,char* key,Exp* var){
    map_insert(self->env.map,key,&var);
}

Exp* apply_proc(Exp* env,Exp* proc,Exp* args){
    Exp* ret = NULL;
    CallStack stack;
    stack.proc = proc;
    stack.args = args;
    if(proc->type == ExpTypeFunc){
        stack.env = env;
        array_push(env->env.vm->call_stack,&stack);
        ret = proc->call(env,args);
    }else{
        args->flags |= ExpFlagRoot;
        Exp* proc_env = exp_new(env->env.vm);
        proc_env->type = ExpTypeEnv;
        proc_env->env.vm = env->env.vm;
        proc_env->env.outer = proc->proc.env;
        proc_env->env.map = map_create(sizeof(Exp*));
        for(int i = 0;i<proc->proc.param->list->size;i++){
            Exp** key = array_get(proc->proc.param->list,i);
            Exp** value = array_get(args->list,i);
            map_insert(proc_env->env.map,(*key)->symbol,value);
        }
        stack.env = proc_env;
        array_push(env->env.vm->call_stack,&stack);
        args->flags &= ~ExpFlagRoot;
        ret = eval(proc_env,proc->proc.body);
    }
    array_pop(env->env.vm->call_stack);
    return ret;
}

Exp* eval(Exp* env,Exp* exp){
    Exp* ret = NULL;
    if(exp->type == ExpTypeSymbol){
        ret = *env_find(env,exp->symbol);
    }else if(exp->type == ExpTypeNum){
        ret = exp;
    }else if(exp->list->size==0){
        ret = exp;
    }else{
        Exp** head = array_get(exp->list,0);
        
        if(strcmp("define",(*head)->symbol)==0){

            Exp** key = array_get(exp->list,1);
            Exp** val = array_get(exp->list,2);
            env_set(env,(*key)->symbol,eval(env,*val));
            ret = *key;
        }else if(strcmp("lambda",(*head)->symbol)==0){

            ret = exp_new(env->env.vm);
            ret->type= ExpTypeProc;
            Exp** param = array_get(exp->list,1);
            Exp** body = array_get(exp->list,2);
            ret->proc.env = env;
            ret->proc.param = *param;
            ret->proc.body = *body;

        }else if(strcmp("set!",(*head)->symbol)==0){
            Exp** key = array_get(exp->list,1);
            Exp** val = array_get(exp->list,2);
            Exp** obj = env_find(env,(*key)->symbol);
            *obj = eval(env,*val);

        }else if(strcmp("if",(*head)->symbol)==0){
            Exp** condition_exp = array_get(exp->list,1);
            Exp** seq = NULL;
            Exp* condition = eval(env,*condition_exp);
            
            if(condition->type == ExpTypeNum&&condition->number 
            || condition->type == ExpTypeList&&condition->list->size){
                seq = array_get(exp->list,2);
            }else{
                seq = array_get(exp->list,3);
            }
            ret = eval(env,*seq);

        }else if(strcmp("quote",(*head)->symbol)==0){
            Exp** quote = array_get(exp->list,1);
            ret = *quote;
        }else{
            Exp* proc = eval(env,*head);
            Exp* args = exp_new(env->env.vm);
            args->type = ExpTypeList;
            args->list = array_create(sizeof(Exp*));
            args->flags |= ExpFlagRoot;
            for(int i =1;i<exp->list->size;i++){
                Exp** item = array_get(exp->list,i);
                Exp* argv = eval(env,*item);
                array_push(args->list,&argv);
            }
            args->flags &= ~ExpFlagRoot;
            ret = apply_proc(env,proc,args);
        }
    }
    return ret;
}

//lib
Exp* build_in_begin(Exp* env,Exp* body){
    Exp** last = array_get(body->list,body->list->size-1);
    return *last;
}
Exp* build_in_add(Exp* env,Exp* body){
    Exp* num = exp_new(env->env.vm);
    num->type = ExpTypeNum;
    for(int i =0;i<body->list->size;i++){
        Exp** item = array_get(body->list,i);
        num->number += (*item)->number;
    }
    return num;
}
Exp* build_in_sub(Exp* env,Exp* body){
    Exp** first = array_get(body->list,0);
    Exp** second = array_get(body->list,1);
    Exp* num = exp_new(env->env.vm);
    num->type = ExpTypeNum;
    num->number = (*first)->number - (*second) ->number;
    return num;
}
Exp* build_in_mult(Exp* env,Exp* body){
    Exp** first = array_get(body->list,0);
    Exp** second = array_get(body->list,1);
    Exp* num = exp_new(env->env.vm);
    num->type = ExpTypeNum;
    num->number = (*first)->number * (*second) ->number;
    return num;
}
Exp* build_in_div(Exp* env,Exp* body){
    Exp** first = array_get(body->list,0);
    Exp** second = array_get(body->list,1);
    Exp* num = exp_new(env->env.vm);
    num->type = ExpTypeNum;
    num->number = (*first)->number / (*second) ->number;
    return num;
}
Exp* build_in_gt(Exp* env,Exp* body){
    Exp** first = array_get(body->list,0);
    Exp** second = array_get(body->list,1);
    Exp* num = exp_new(env->env.vm);
    num->type = ExpTypeNum;
    num->number = (*first)->number > (*second) ->number;
    return num;
}
Exp* build_in_lt(Exp* env,Exp* body){
    Exp** first = array_get(body->list,0);
    Exp** second = array_get(body->list,1);
    Exp* num = exp_new(env->env.vm);
    num->type = ExpTypeNum;
    num->number = (*first)->number < (*second) ->number;
    return num;
}
Exp* build_in_ge(Exp* env,Exp* body){
    Exp** first = array_get(body->list,0);
    Exp** second = array_get(body->list,1);
    Exp* num = exp_new(env->env.vm);
    num->type = ExpTypeNum;
    num->number = (*first)->number >= (*second) ->number;
    return num;
}
Exp* build_in_le(Exp* env,Exp* body){
    Exp** first = array_get(body->list,0);
    Exp** second = array_get(body->list,1);
    Exp* num = exp_new(env->env.vm);
    num->type = ExpTypeNum;
    num->number = (*first)->number <= (*second) ->number;
    return num;
}
Exp* build_in_eq(Exp* env,Exp* body){
    Exp** first = array_get(body->list,0);
    Exp** second = array_get(body->list,1);
    Exp* num = exp_new(env->env.vm);
    num->type = ExpTypeNum;
    num->number = (*first)->number == (*second) ->number;
    return num;
}
Exp* build_in_list(Exp* env,Exp* body){

    return body;
}

Exp* build_in_apply(Exp* env,Exp* body){
    Exp* ret;
    Exp** proc = array_get(body->list,0);
    for(int i =1;i<body->list->size;i++){
        Exp** list_to_apply = array_get(body->list,i);
        ret = apply_proc(env,*proc,*list_to_apply);
    }
    return ret;
}

Exp* build_in_cons(Exp* env,Exp* body){
    
    Exp* list = exp_new(env->env.vm);
    list->type = ExpTypeList;
    list->list = array_create(sizeof(Exp*));
    Exp** first = array_get(body->list,0);
    Exp** second = array_get(body->list,1);

    array_push(list->list,first);
    array_append(list->list,(*second)->list);
    return list;
}
Exp* build_in_car(Exp* env,Exp* body){
    Exp** first = array_get(body->list,0);
    Exp** car = array_get((*first)->list,0);
    return *car;
}
Exp* build_in_cdr(Exp* env,Exp* body){
    Exp** first = array_get(body->list,0);

    Exp* list = exp_new(env->env.vm);
    list->type = ExpTypeList;
    list->list = array_create(sizeof(Exp*));
    for(int i =1;i<(*first)->list->size;i++){
        Exp**item = array_get((*first)->list,i);
        array_push(list->list,item);
    }
    return list;
}
Exp* make_build_in(VM* vm,Callable func){
    Exp* v = exp_new(vm);
    v->type=ExpTypeFunc;
    v->call = func;
    return v;
}

Exp* standard_env(VM* vm){
    Exp* env = exp_new(vm);
    env->flags|=ExpFlagRoot;
    env->type = ExpTypeEnv;
    env->env.vm = vm;
    env->env.outer = NULL;
    env->env.map = map_create(sizeof(Exp*));
    env_set(env,"begin",make_build_in(vm,build_in_begin));
    env_set(env,"cons",make_build_in(vm,build_in_cons));
    env_set(env,"car",make_build_in(vm,build_in_car));
    env_set(env,"cdr",make_build_in(vm,build_in_cdr));
    env_set(env,"list",make_build_in(vm,build_in_list));
    env_set(env,"apply",make_build_in(vm,build_in_apply));
    env_set(env,"+",make_build_in(vm,build_in_add));
    env_set(env,"-",make_build_in(vm,build_in_sub));
    env_set(env,"*",make_build_in(vm,build_in_mult));
    env_set(env,"/",make_build_in(vm,build_in_div));
    env_set(env,">",make_build_in(vm,build_in_gt));
    env_set(env,"<",make_build_in(vm,build_in_lt));
    env_set(env,">=",make_build_in(vm,build_in_ge));
    env_set(env,"<=",make_build_in(vm,build_in_le));
    env_set(env,"=",make_build_in(vm,build_in_eq));
    env_set(env,"equal?",make_build_in(vm,build_in_eq));
    return env;
}


//memory
Exp* exp_new(VM* vm){
    if(vm->exp_num-vm->last_gc_num > vm->gc_thre){
        vm_gc(vm);
    }
    Exp* exp = malloc(sizeof(Exp));
    memset(exp,0,sizeof(Exp));
    exp->next = vm->head;
    vm->head = exp;
    vm->exp_num++;
    return exp;
}
void mark(Exp* exp){
    if(exp->flags&ExpFlagMarked)return;
    exp->flags |= ExpFlagMarked;
    if(exp->type == ExpTypeList){
        for(int i =0;i<exp->list->size;i++){
            Exp** item = array_get(exp->list,i);
            mark(*item);
        }
    }else if(exp->type == ExpTypeProc){
        mark(exp->proc.body);
        mark(exp->proc.env);
        mark(exp->proc.param);
    }else if(exp->type == ExpTypeEnv){
        MapIter iter = map_iter(exp->env.map);
        char* key;
        while (key = map_next(exp->env.map,&iter))
        {
            Exp** item = map_get(exp->env.map,key);
            mark(*item);
        }
    }
}
void exp_free(Exp* exp){
    if(exp->type == ExpTypeSymbol){
        free(exp->symbol);
    }else if(exp->type == ExpTypeList){
        array_destroy(exp->list);
    }else if(exp->type == ExpTypeEnv){
        map_destroy(exp->env.map);
    }
    memset(exp,0,sizeof(Exp));
    free(exp);
}
void sweep(VM* vm){
    Exp** head = &(vm->head);
    while (*head)
    {
        if((*head)->flags&ExpFlagMarked){
            (*head)->flags &= ~ExpFlagMarked;
            head = &((*head)->next);
        }else{
            Exp* unreached = *head;
            *head = unreached->next;
            exp_free(unreached);
            vm->exp_num--;
        }
    }
    
}

//interface
Exp* vm_eval(VM* vm,char* str){
    Array* tokens = tokenize(str);
    Exp* list_to_eval = read_from_tokens(vm,tokens);
    array_destroy(tokens);

    list_to_eval->flags |= ExpFlagRoot;
    Exp* res = eval(vm->global_env,list_to_eval);
    list_to_eval->flags &= ~ExpFlagRoot;
    return res;
}
void vm_init(VM* vm){
    memset(vm,0,sizeof(VM));
    vm->call_stack = array_create(sizeof(CallStack));
    vm->gc_thre = 64;
    vm->global_env = standard_env(vm);


}
void vm_gc(VM* vm){
    Exp* head = vm->head;
    //mark root
    while (head)
    {
        if(head->flags&ExpFlagRoot){
            mark(head);
        }
        head = head->next;
    }
    //mark callstack
    for(int i =0;i<vm->call_stack->size;i++){
        CallStack* item = array_get(vm->call_stack,i);
        mark(item->args);
        mark(item->env);
        mark(item->proc);
    }
    int num = vm->exp_num;
    sweep(vm);
    vm->last_gc_num = vm->exp_num;
    vm->gc_thre = vm->exp_num * 2;
    printf("gc triggered! %d exp free,%d exp remain,%d new for next gc.\n",
        num - vm->exp_num,vm->exp_num,vm->gc_thre);
}

void to_string(Exp* obj,char* str){
    
    switch (obj->type)
    {
    case ExpTypeSymbol:
        sprintf(str," %s ",obj->symbol);
        break;
    case ExpTypeNum:
        sprintf(str," %f ",obj->number);
        break;
    case ExpTypeList:
        int offset = 0;
        sprintf(str+offset,"(");
        offset += 1;
        for(int i =0;i<obj->list->size;i++){
            char buf[128]={0};
            Exp** item = array_get(obj->list,i);
            to_string(*item,buf);
            sprintf(str+offset,"%s",buf);
            offset += strlen(buf);
        }
        sprintf(str+offset,")");
        offset += 1;
        break;
    case ExpTypeFunc:
        sprintf(str,"<Func>");
        break;
    case ExpTypeProc:
        sprintf(str,"<Proc>");
        break;
    case ExpTypeEnv:
        sprintf(str,"<Env>");
        break;
    default:
        break;
    }
}