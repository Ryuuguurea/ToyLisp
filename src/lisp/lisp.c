#include "lisp.h"
#include "../collection/map.h"
#include "stdio.h"
#include "../collection/array.h"
#include "string.h"
#include "stdlib.h"
#include "ctype.h"

//forward
Exp* exp_new(VM* vm);

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
        res->type = Number;
        res->number = atof(str);
        free(str);
    }else{
        res->type = Symbol;
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
        list_value->type = List;
        list_value->list = array_create(sizeof(Exp*));

        while (strcmp(*(char**)array_get(tokens,0),")")!=0)
        {
            Exp* left_value = read_from_tokens(vm,tokens);
            array_push(list_value->list,&left_value); 
        }
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


Exp* eval(Exp* exp,Exp* env){
    Exp* ret = NULL;
    if(exp->type == Symbol){
        ret = *env_find(env,exp->symbol);
    }else if(exp->type == Number){
        ret = exp;
    }else{
        Exp** head = array_get(exp->list,0);
        
        if(strcmp("define",(*head)->symbol)==0){

            Exp** key = array_get(exp->list,1);
            Exp** val = array_get(exp->list,2);
            env_set(env,(*key)->symbol,eval(*val,env));

        }else if(strcmp("lambda",(*head)->symbol)==0){

            ret = exp_new(env->env.vm);
            ret->type= Procedure;
            Exp** param = array_get(exp->list,1);
            Exp** body = array_get(exp->list,2);
            ret->proc.env = env;
            ret->proc.param = *param;
            ret->proc.body = *body;

        }else if(strcmp("set!",(*head)->symbol)==0){
            Exp** key = array_get(exp->list,1);
            Exp** val = array_get(exp->list,2);
            Exp** obj = env_find(env,(*key)->symbol);
            *obj = eval(*val,env);

        }else if(strcmp("if",(*head)->symbol)==0){
            Exp** condition_exp = array_get(exp->list,1);
            Exp** seq = NULL;
            Exp* condition = eval(*condition_exp,env);
            
            if(condition->type == Number&&condition->number 
            || condition->type == List&&condition->list->size){
                seq = array_get(exp->list,2);
            }else{
                seq = array_get(exp->list,3);
            }
            ret = eval(*seq,env);

        }else if(strcmp("quote",(*head)->symbol)==0){
            Exp** quote = array_get(exp->list,1);
            ret = *quote;

        }else{
            Exp* proc = eval(*head,env);
            Exp* args = exp_new(env->env.vm);
            args->type = List;
            args->list = array_create(sizeof(Exp*));
            for(int i =1;i<exp->list->size;i++){
                Exp** item = array_get(exp->list,i);
                Exp* argv = eval(*item,env);
                array_push(args->list,&argv);
            }
            if(proc->type == Function){
                ret = proc->call(env,args);
            }else{
                Exp* proc_env = exp_new(env->env.vm);
                proc_env->type = Environment;
                proc_env->env.vm = env->env.vm;
                proc_env->env.outer = proc->proc.env;
                proc_env->env.map = map_create(sizeof(Exp*));
                for(int i = 0;i<proc->proc.param->list->size;i++){
                    Exp** key = array_get(proc->proc.param->list,i);
                    Exp** value = array_get(args->list,i);
                    map_insert(proc_env->env.map,(*key)->symbol,value);
                }
                ret = eval(proc->proc.body,proc_env);
            }
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
    num->type = Number;
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
    num->type = Number;
    num->number = (*first)->number - (*second) ->number;
    return num;
}
Exp* build_in_mult(Exp* env,Exp* body){
    Exp** first = array_get(body->list,0);
    Exp** second = array_get(body->list,1);
    Exp* num = exp_new(env->env.vm);
    num->type = Number;
    num->number = (*first)->number * (*second) ->number;
    return num;
}
Exp* build_in_div(Exp* env,Exp* body){
    Exp** first = array_get(body->list,0);
    Exp** second = array_get(body->list,1);
    Exp* num = exp_new(env->env.vm);
    num->type = Number;
    num->number = (*first)->number / (*second) ->number;
    return num;
}
Exp* build_in_gt(Exp* env,Exp* body){
    Exp** first = array_get(body->list,0);
    Exp** second = array_get(body->list,1);
    Exp* num = exp_new(env->env.vm);
    num->type = Number;
    num->number = (*first)->number > (*second) ->number;
    return num;
}
Exp* build_in_lt(Exp* env,Exp* body){
    Exp** first = array_get(body->list,0);
    Exp** second = array_get(body->list,1);
    Exp* num = exp_new(env->env.vm);
    num->type = Number;
    num->number = (*first)->number < (*second) ->number;
    return num;
}
Exp* build_in_eq(Exp* env,Exp* body){
    Exp** first = array_get(body->list,0);
    Exp** second = array_get(body->list,1);
    Exp* num = exp_new(env->env.vm);
    num->type = Number;
    num->number = (*first)->number == (*second) ->number;
    return num;
}
Exp* build_in_list(Exp* env,Exp* body){

    return body;
}

Exp* build_in_apply(Exp* env,Exp* body){
    
    Exp* list = exp_new(env->env.vm);
    list->type = List;
    list->list = array_create(sizeof(Exp*));
    for(int i =0;i<body->list->size;i++){
        Exp** item = array_get(body->list,i);
        array_push(list->list,item);
    }

    return eval(list,env);
}
Exp* build_in_cons(Exp* env,Exp* body){
    
    Exp* list = exp_new(env->env.vm);
    list->type = List;
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
    list->type = List;
    list->list = array_create(sizeof(Exp*));
    for(int i =1;i<(*first)->list->size;i++){
        Exp**item = array_get((*first)->list,i);
        array_push(list->list,item);
    }
    return list;
}
Exp* make_build_in(VM* vm,Callable func){
    Exp* v = exp_new(vm);
    v->type=Function;
    v->call = func;
    return v;
}

Exp* standard_env(VM* vm){
    Exp* env = exp_new(vm);
    env->type = Environment;
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
    env_set(env,"=",make_build_in(vm,build_in_eq));
    return env;
}


//memory
Exp* exp_new(VM* vm){
    Exp* exp = malloc(sizeof(Exp));
    memset(exp,0,sizeof(Exp));
    exp->next = vm->head;
    vm->head = exp;
    vm->exp_num++;
    return exp;
}
void mark(Exp* exp){
    if(exp->marked)return;
    exp->marked = 1;
    if(exp->type == List){
        for(int i =0;i<exp->list->size;i++){
            Exp** item = array_get(exp->list,i);
            mark(*item);
        }
    }else if(exp->type == Procedure){
        mark(exp->proc.body);
        mark(exp->proc.env);
        mark(exp->proc.param);
    }else if(exp->type == Environment){
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
    if(exp->type == Symbol){
        free(exp->symbol);
    }else if(exp->type == List){
        array_destroy(exp->list);
    }else if(exp->type == Environment){
        map_destroy(exp->env.map);
    }
    free(exp);
}
void sweep(VM* vm){
    Exp* head = vm->head;
    while (head)
    {
        if(head->marked){
            head->marked = 0;
            head = head->next;
        }else{
            vm->head = head->next;
            exp_free(head);
            head = vm->head;
            vm->exp_num--;
        }
    }
    
}

//interface
Exp* vm_eval(VM* vm,char* str){
    Array* tokens = tokenize(str);
    Exp* exp = read_from_tokens(vm,tokens);
    array_destroy(tokens);
    return eval(exp,vm->global_env);
}
void vm_init(VM* vm){
    memset(vm,0,sizeof(VM));
    vm->global_env = standard_env(vm);
}
void vm_gc(VM* vm){
    mark(vm->global_env);
    sweep(vm);
}

void to_string(Exp* obj,char* str){
    
    switch (obj->type)
    {
    case Number:
        sprintf(str," %f ",obj->number);
        break;
    case List:
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
    case Function:
        sprintf(str,"<Native>");
        break;
    case Procedure:
        sprintf(str,"<Procedure>");
        break;
    default:
        break;
    }
}