#include "lisp.h"
#include "../collection/map.h"
#include "stdio.h"
#include "../collection/array.h"
#include "string.h"
#include "stdlib.h"
#include "ctype.h"
#include "time.h"
//forward
Exp* exp_new(VM* vm);
Exp* eval(Exp* env,Exp* exp);
Exp* apply_proc(Exp* env,Exp* proc,Exp* args);
Exp* vm_load(VM*,char*);
//syntax
Array* tokenize(char* str){
    Array* res = array_create(sizeof(char*));
    Array* buf = array_create(sizeof(char));
    int index = 0;
    char* item = NULL;
    while(str[index]){
        //comment
        if(str[index]==';'){
            while (str[index]!='\n'&&str[index]!='\r')
            {
                index++;
            }
        }
        if(str[index]=='('||str[index]==')'||str[index]==' '||str[index]=='\''||str[index]=='\"'||str[index]=='\n'||str[index]=='\r'){
            if(buf->size>0){
                item = malloc((buf->size+1)*sizeof(char));
                item[buf->size] = '\0';
                memcpy(item,buf->data,buf->size*sizeof(char));
                array_push(res,&item);
                array_clear(buf);
            }
            if(str[index]=='('||str[index]==')'||str[index]=='\''||str[index]=='\"'){
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
    int num_count = 0;
    while(str[idx]){
        if(idx == 0&&(str[idx]!='-')&&!isdigit(str[idx])){
            is_number = 0;
            break;
        }
        if(isdigit(str[idx]))num_count++;
        idx++;
    }
    if(is_number&&num_count>0){
        res->type = ExpTypeNum;
        res->number = atof(str);
        free(str);
    }else if(str[0]=='#'){
        if(str[1]=='\\'){
            res->type = ExpTypeChar;
            res->character = str[2];
        }else if(str[1]=='t'){
            res->type = ExpTypeNum;
            res->number = 1;
        }else{
            res->type = ExpTypeNum;
            res->number = 0;
        }
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
    if(strcmp(*token_ptr,"\"")==0){
        free(*token_ptr);
        free(token_ptr);

        token_ptr = array_shift(tokens);
        list_value = exp_new(vm);
        list_value->type = ExpTypeString;
        list_value->str = *token_ptr;
        free(token_ptr);

        token_ptr = array_shift(tokens);
        free(*token_ptr);
        free(token_ptr);

    }else if(strcmp(*token_ptr,"(")==0){
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
    }else if(strcmp(*token_ptr,"'")==0){
        free(*token_ptr);
        free(token_ptr);

        list_value = exp_new(vm);
        list_value->type = ExpTypeList;
        list_value->list = array_create(sizeof(Exp*));
        list_value->flags |= ExpFlagRoot;

        char quote_expr[] = "quote";
        Exp* quote = exp_new(vm);
        quote->type = ExpTypeSymbol;
        quote->symbol = malloc(sizeof(quote_expr));
        memcpy(quote->symbol,quote_expr,sizeof(quote_expr));

        array_push(list_value->list,&quote);
        Exp* next = read_from_tokens(vm,tokens);
        array_push(list_value->list,&next);
        list_value->flags &= ~ExpFlagRoot;

    }else if(strcmp(*token_ptr,")")!=0){
        list_value = atom(vm,*token_ptr);
        free(token_ptr);
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
Exp* apply_form(Exp* env,Exp* form,Exp* exp){
    return form->call(env,exp);
}
Exp* apply_proc(Exp* env,Exp* proc,Exp* args){
    Exp* ret = NULL;
    CallStack stack;
    stack.proc = proc;
    stack.args = args;
    if(proc->type == ExpTypeFunc||proc->type==ExpTypeForm){
        stack.env = env;
        array_push(env->env.vm->call_stack,&stack);
        ret = proc->call(env,args);
    }else{
        args->flags |= ExpFlagRoot;
        Exp* proc_env = exp_new(env->env.vm);
        proc_env->flags |= ExpFlagRoot;
        proc_env->type = ExpTypeEnv;
        proc_env->env.vm = env->env.vm;
        proc_env->env.outer = proc->proc.env;
        proc_env->env.map = map_create(sizeof(Exp*));
        for(int i = 0;i<proc->proc.param->list->size;i++){
            Exp** key = array_get(proc->proc.param->list,i);
            if(strcmp((*key)->symbol,".")==0){
                key = array_get(proc->proc.param->list,i+1);
                Exp* rest = exp_new(env->env.vm);
                rest->type = ExpTypeList;
                rest->list = array_create(sizeof(Exp*));
                for(int j = i;j<args->list->size;j++){
                    Exp** value = array_get(args->list,j);
                    array_push(rest->list,value);
                }
                map_insert(proc_env->env.map,(*key)->symbol,&rest);
                break;
            }
            Exp** value = array_get(args->list,i);
            map_insert(proc_env->env.map,(*key)->symbol,value);
        }
        stack.env = proc_env;
        array_push(env->env.vm->call_stack,&stack);
        args->flags &= ~ExpFlagRoot;
        proc_env->flags &= ~ExpFlagRoot;
        ret = eval(proc_env,proc->proc.body);
        if(proc->type == ExpTypeMacro){
            ret =eval(env,ret);
        }
    }
    array_pop(env->env.vm->call_stack);
    return ret;
}

Exp* eval(Exp* env,Exp* exp){
    Exp* ret = NULL;
    if(exp->type == ExpTypeSymbol){
        ret = *env_find(env,exp->symbol);
    }else if(exp->type == ExpTypeNum||exp->type == ExpTypeString||exp->type == ExpTypeChar){
        ret = exp;
    }else if(exp->list->size==0){
        ret = exp;
    }else{
        Exp** head = array_get(exp->list,0);
        Exp* proc = eval(env,*head);
        if(proc->type == ExpTypeForm){
            return apply_form(env,proc,exp);
        }else{
            Exp* args = exp_new(env->env.vm);
            args->type = ExpTypeList;
            args->list = array_create(sizeof(Exp*));
            args->flags |= ExpFlagRoot;
            for(int i = 1;i<exp->list->size;i++){
                Exp** item = array_get(exp->list,i);
                if(proc->type == ExpTypeMacro||proc->type == ExpTypeForm){
                    array_push(args->list,item);
                }else{
                    Exp* argv = eval(env,*item);
                    array_push(args->list,&argv);
                }
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
    for(int i = 1;i<body->list->size;i++){
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
Exp* build_in_null(Exp* env,Exp* body){
    Exp** first = array_get(body->list,0);
    Exp* res= exp_new(env->env.vm);
    res->type=ExpTypeNum;
    if((*first)->type==ExpTypeList&&(*first)->list->size==0){
        res->number = 1;
    }
    return res;
}
Exp* build_in_length(Exp* env,Exp* body){
    Exp** first = array_get(body->list,0);
    Exp* res= exp_new(env->env.vm);
    res->type=ExpTypeNum;
    if((*first)->type==ExpTypeList){
        res->number = (*first)->list->size;
    }else if((*first)->type==ExpTypeString){
        res->number = strlen((*first)->str);
    }
    return res;
}
Exp* build_in_sym_str(Exp* env,Exp* body){
    Exp** first = array_get(body->list,0);
    Exp* res= exp_new(env->env.vm);
    res->type=ExpTypeString;
    int size = strlen((*first)->symbol);
    res->str = malloc(size+1);
    memset(res->str,0,size+1);
    memcpy(res->str,(*first)->symbol,size);
    return res;
}
Exp* build_in_str_sym(Exp* env,Exp* body){
    Exp** first = array_get(body->list,0);
    Exp* res= exp_new(env->env.vm);
    res->type=ExpTypeSymbol;
    int size = strlen((*first)->str);
    res->str = malloc(size+1);
    memset(res->str,0,size+1);
    memcpy(res->str,(*first)->str,size);
    return res;
}
Exp* build_in_str_append(Exp* env,Exp* body){
    Exp* res= exp_new(env->env.vm);
    int alloc_size = 1;
    for(int i =0;i<body->list->size;i++){
        Exp** item = array_get(body->list,i);
        alloc_size+=strlen((*item)->str);
    }
    res->type=ExpTypeString;
    res->str = malloc(alloc_size);
    memset(res->str,0,alloc_size);
    int offset = 0;
    for(int i =0;i<body->list->size;i++){
        Exp** item = array_get(body->list,i);
        int copy_size = strlen((*item)->str);
        memcpy(res->str+offset,(*item)->str,copy_size);
        offset+=copy_size;
    }
    return res;
}
Exp* build_in_load(Exp* env,Exp* body){
    Exp** first = array_get(body->list,0);
    return vm_load(env->env.vm,(*first)->str);
}
Exp* build_in_readchar(Exp* env,Exp* body){
    Exp* res = exp_new(env->env.vm);
    res->type=ExpTypeChar;
    res->character = fgetc(stdin);
    return res;
}
Exp* build_in_display(Exp* env,Exp* body){
    Exp** first = array_get(body->list,0);
    char buffer[1024]={0};
    to_string(*first,buffer);
    printf("%s",buffer);
    return *first;
}
Exp* build_in_char_eq(Exp* env,Exp* body){
    Exp** first = array_get(body->list,0);
    Exp** second = array_get(body->list,1);
    Exp* res = exp_new(env->env.vm);
    res->type=ExpTypeNum;
    res->number = (*first)->character == (*second)->character;
    return res;
}
Exp* build_in_random(Exp* env,Exp* body){
    Exp** first = array_get(body->list,0);
    Exp* res = exp_new(env->env.vm);
    res->type=ExpTypeNum;
    res->number = rand()%((int)(*first)->number);
    return res;
}
Exp* make_fun(VM* vm,Callable func){
    Exp* v = exp_new(vm);
    v->type=ExpTypeFunc;
    v->call = func;
    return v;
}
Exp* build_in_define(Exp* env,Exp* exp){
    Exp** key = array_get(exp->list,1);
    Exp** val = array_get(exp->list,2);
    env_set(env,(*key)->symbol,eval(env,*val));
    return *key;
}
Exp* build_in_lambda(Exp* env,Exp* exp){
    Exp* ret = exp_new(env->env.vm);
    ret->type= ExpTypeProc;
    Exp** param = array_get(exp->list,1);
    Exp** body = array_get(exp->list,2);
    ret->proc.env = env;
    ret->proc.param = *param;
    ret->proc.body = *body;
    return ret;
}
Exp* build_in_let(Exp* env,Exp* exp){
    Exp** bind = array_get(exp->list,1);
    Exp** body = array_get(exp->list,2);
    Exp* let_env = exp_new(env->env.vm);
    let_env->type = ExpTypeEnv;
    let_env->env.outer=env;
    let_env->env.map = map_create(sizeof(Exp*));
    let_env->env.vm=env->env.vm;
    for(int i =0;i<(*bind)->list->size;i++){
        Exp** kv = array_get((*bind)->list,i);
        Exp** key = array_get((*kv)->list,0);
        Exp** value = array_get((*kv)->list,1);
        env_set(let_env,(*key)->symbol,*value);
    }
    return eval(let_env,*body);
}
Exp* build_in_set(Exp* env,Exp* exp){
    Exp** key = array_get(exp->list,1);
    Exp** val = array_get(exp->list,2);
    Exp** obj = env_find(env,(*key)->symbol);
    *obj = eval(env,*val);
    return *obj;
}
Exp* build_in_if(Exp* env,Exp* exp){
    Exp** condition_exp = array_get(exp->list,1);
    Exp** seq = NULL;
    Exp* condition = eval(env,*condition_exp);
    
    if(condition->type == ExpTypeNum&&condition->number 
    || condition->type == ExpTypeList&&condition->list->size){
        seq = array_get(exp->list,2);
    }else{
        seq = array_get(exp->list,3);
    }
    return eval(env,*seq);
}
Exp* build_in_quote(Exp* env,Exp* exp){
    Exp** quote = array_get(exp->list,1);
    return *quote;
}
Exp* build_in_macro(Exp* env,Exp* exp){
    Exp** key = array_get(exp->list,1);

    Exp* macro = exp_new(env->env.vm);
    macro->type= ExpTypeMacro;
    Exp** param = array_get(exp->list,2);
    Exp** body = array_get(exp->list,3);
    macro->proc.env = env;
    macro->proc.param = *param;
    macro->proc.body = *body;

    env_set(env,(*key)->symbol,macro);

    return *key;  
}

Exp* build_in_cond(Exp* env,Exp* body){
    Exp* else_exp = NULL;
    for(int i =1;i<body->list->size;i++){
        Exp** cond = array_get(body->list,i);
        Exp** test = array_get((*cond)->list,0);
        Exp** exe = array_get((*cond)->list,1);
        if((*test)->type==ExpTypeSymbol&&strcmp((*test)->symbol,"else")==0){
            else_exp = *exe;
        }else{
            Exp* test_res = eval(env,*test);
            if(test_res->number){
                return eval(env,*exe);
            }
        }
    }
    if(else_exp){
        return eval(env,else_exp);
    }
    return NULL;
}
Exp* make_form(VM* vm,Callable func){
    Exp* v = exp_new(vm);
    v->type=ExpTypeForm;
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
    //special form
    env_set(env,"define",make_form(vm,build_in_define));
    env_set(env,"lambda",make_form(vm,build_in_lambda));
    env_set(env,"let",make_form(vm,build_in_let));
    env_set(env,"set!",make_form(vm,build_in_set));
    env_set(env,"if",make_form(vm,build_in_if));
    env_set(env,"quote",make_form(vm,build_in_quote));
    env_set(env,"define-macro",make_form(vm,build_in_macro));
    env_set(env,"cond",make_form(vm,build_in_cond));

    //func
    env_set(env,"begin",make_fun(vm,build_in_begin));
    env_set(env,"cons",make_fun(vm,build_in_cons));
    env_set(env,"car",make_fun(vm,build_in_car));
    env_set(env,"cdr",make_fun(vm,build_in_cdr));
    env_set(env,"list",make_fun(vm,build_in_list));
    env_set(env,"apply",make_fun(vm,build_in_apply));
    env_set(env,"+",make_fun(vm,build_in_add));
    env_set(env,"-",make_fun(vm,build_in_sub));
    env_set(env,"*",make_fun(vm,build_in_mult));
    env_set(env,"/",make_fun(vm,build_in_div));
    env_set(env,">",make_fun(vm,build_in_gt));
    env_set(env,"<",make_fun(vm,build_in_lt));
    env_set(env,">=",make_fun(vm,build_in_ge));
    env_set(env,"<=",make_fun(vm,build_in_le));
    env_set(env,"=",make_fun(vm,build_in_eq));
    env_set(env,"equal?",make_fun(vm,build_in_eq));
    env_set(env,"null?",make_fun(vm,build_in_null));
    env_set(env,"length",make_fun(vm,build_in_length));
    env_set(env,"symbol->string",make_fun(vm,build_in_sym_str));
    env_set(env,"string->symbol",make_fun(vm,build_in_str_sym));
    env_set(env,"string-append",make_fun(vm,build_in_str_append));

    env_set(env,"read-char",make_fun(vm,build_in_readchar));
    env_set(env,"display",make_fun(vm,build_in_display));
    env_set(env,"char=?",make_fun(vm,build_in_char_eq));

    time_t t;
    srand((unsigned)time(&t));
    env_set(env,"random",make_fun(vm,build_in_random));
    env_set(env,"load",make_fun(vm,build_in_load));
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
    }else if(exp->type == ExpTypeProc || exp->type == ExpTypeMacro){
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
    }else if(exp->type == ExpTypeString){
        free(exp->str);
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
Exp* vm_load(VM*vm,char*path){
    FILE* fp = fopen(path,"r");
    Array* dst = array_create(sizeof(char));

    int offset = 0;
    array_resize(dst,32);
    while(!feof(fp)){
        if(dst->size>=dst->data_size){
            array_resize(dst,dst->data_size+32);
        }
        int rd_size = fread((char*)dst->data + offset,sizeof(char),32,fp);
        offset += rd_size;
        dst->size += rd_size;
    }
    fclose(fp);
    char end = '\0';
    array_push(dst,&end);
    Exp* res= vm_eval(vm,dst->data);
    array_destroy(dst);
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
    case ExpTypeString:
        sprintf(str,"\"%s\"",obj->str);
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
    case ExpTypeForm:
        sprintf(str,"<Form>");
        break;
    case ExpTypeMacro:
        sprintf(str,"<Macro>");
        break;
    case ExpTypeEnv:
        sprintf(str,"<Env>");
        break;
    case ExpTypeChar:
        sprintf(str,"'%c'",obj->character);
        break;
    case ExpTypePointer:
        sprintf(str,"<Pointer>");
        break;
    default:
        break;
    }
}