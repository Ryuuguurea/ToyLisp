#include "lisp.h"
#include "../collection/map.h"
#include "stdio.h"
#include "../collection/array.h"
#include "string.h"
#include "stdlib.h"
#include "ctype.h"
#include "time.h"
//forward
Exp* exp_new(VM* vm, ExpType type,int size);
Exp* eval(VM* vm,Exp* env,Exp* exp);
Exp* apply_proc(VM* vm,Exp* env,Exp* proc,Exp* args);
Exp* vm_load(VM*,char*);
Exp* vm_apply(VM*);

void add_to_root(Exp* exp){
    exp->flags|=ExpFlagRoot;
}
void remove_from_root(Exp* exp){
    exp->flags&=~ExpFlagRoot;
}
ExpSymbol* exp_new_symbol(VM* vm,ExpString* str){
    ExpSymbol* ret = exp_new(vm,ExpTypeSymbol,sizeof(ExpSymbol));
    ret->string = str;
    return ret;
}
ExpString* exp_new_string(VM* vm,char* str){
    ExpString* ret = exp_new(vm,ExpTypeString,sizeof(ExpString));
    int len = strlen(str)+1;
    ret->data = malloc(len);
    memset(ret->data,0,len);
    memcpy(ret->data,str,len-1);
    return ret;
}
ExpList* exp_new_list(VM* vm){
    ExpList* ret = exp_new(vm,ExpTypeList,sizeof(ExpList));
    ret->list = array_create(sizeof(Exp*));
    return ret;
}
ExpNumber* exp_new_number(VM* vm,double val){
    ExpNumber* ret = exp_new(vm,ExpTypeNum,sizeof(ExpNumber));
    ret->number = val;
    return ret;
}
ExpFunc* exp_new_func(VM* vm,Callable func){
    ExpFunc* ret = exp_new(vm,ExpTypeFunc,sizeof(ExpFunc));
    ret->func = func;
    return ret;
}
ExpProc* exp_new_proc(VM* vm,ExpEnv*env,ExpList*body,ExpList*param){
    ExpProc* ret = exp_new(vm,ExpTypeProc,sizeof(ExpProc));
    ret->env = env;
    ret->body = body;
    ret->param = param;
    return ret;
}
ExpMacro* exp_new_macro(VM* vm,ExpEnv*env,ExpList*body,ExpList*param){
    ExpMacro* ret = exp_new(vm,ExpTypeMacro,sizeof(ExpMacro));
    ret->env = env;
    ret->body = body;
    ret->param = param;
    return ret;
}
ExpForm* exp_new_form(VM* vm,Callable func){
    ExpForm* ret = exp_new(vm,ExpTypeForm,sizeof(ExpForm));
    ret->func = func;
    return ret;
}
ExpEnv* exp_new_env(VM* vm,ExpEnv* outer){
    ExpEnv* ret = exp_new(vm,ExpTypeEnv,sizeof(ExpEnv));
    ret->outer = outer;
    ret->map = map_create(sizeof(Exp*));
    return ret;
}
ExpChar* exp_new_char(VM* vm,char inchar){
    ExpChar* ret = exp_new(vm,ExpTypeChar,sizeof(ExpChar));
    ret->character = inchar;
    return ret;
}
ExpCallcc* exp_new_callcc(VM* vm){
    ExpCallcc* ret = exp_new(vm,ExpTypeCallcc,sizeof(ExpCallcc));
    ret->list = array_create(sizeof(Exp*));
    return ret;
}
ExpContinuation* exp_new_contiunation(VM* vm){
    ExpContinuation* ret = exp_new(vm,ExpTypeContinuation,sizeof(ExpContinuation));
    ret->args = NULL;
    ret->env = NULL;
    ret->proc = NULL;
    return ret;
}

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
        if(str[index]=='('||str[index]==')'||
            str[index]=='['||str[index]==']'||
            str[index]==' '||str[index]=='\''||
            str[index]=='\"'||str[index]=='\n'||
            str[index]=='\r'){
            if(buf->size>0){
                item = malloc((buf->size+1)*sizeof(char));
                item[buf->size] = '\0';
                memcpy(item,buf->data,buf->size*sizeof(char));
                array_push(res,&item);
                array_clear(buf);
            }
            if(str[index]=='('||str[index]==')'||
                str[index]=='['||str[index]==']'||
                str[index]=='\''||str[index]=='\"'){
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
    Exp* res = NULL;
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
        res = exp_new_number(vm, atof(str));
        free(str);
    }else if(str[0]=='#'){
        if(str[1]=='\\'){
            res = exp_new_char(vm,str[2]);
        }else if(str[1]=='t'){
            res = exp_new_number(vm,1);
        }else{
            res = exp_new_number(vm,0);
        }
        free(str);
    }else{
        ExpString* exp_str = exp_new_string(vm,str);
        add_to_root(exp_str);
        res = exp_new_symbol(vm,exp_str);
        remove_from_root(exp_str);
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
        list_value = exp_new_string(vm,*token_ptr);
        free(token_ptr);

        token_ptr = array_shift(tokens);
        free(*token_ptr);
        free(token_ptr);

    }else if(strcmp(*token_ptr,"(")==0||strcmp(*token_ptr,"[")==0){
        free(*token_ptr);
        free(token_ptr);
        list_value = exp_new_list(vm);
        add_to_root(list_value);
        while (strcmp(*(char**)array_get(tokens,0),")")!=0&&
            strcmp(*(char**)array_get(tokens,0),"]")!=0)
        {
            Exp* left_value = read_from_tokens(vm,tokens);
            array_push(((ExpList*)list_value)->list,&left_value); 
        }
        remove_from_root(list_value);
        token_ptr = array_shift(tokens);
        free(*token_ptr);
        free(token_ptr);
    }else if(strcmp(*token_ptr,"'")==0){
        free(*token_ptr);
        free(token_ptr);

        list_value = exp_new_list(vm);
        add_to_root(list_value);

        char quote_expr[] = "quote";
        ExpString* quote_str = exp_new_string(vm,quote_expr);
        ExpSymbol* quote = exp_new_symbol(vm,quote_str);

        array_push(((ExpList*)list_value)->list,&quote);
        Exp* next = read_from_tokens(vm,tokens);
        array_push(((ExpList*)list_value)->list,&next);
        remove_from_root(list_value);

    }else if(strcmp(*token_ptr,")")!=0&&
        strcmp(*token_ptr,"]")!=0){
        list_value = atom(vm,*token_ptr);
        free(token_ptr);
    }
    return list_value;
}

Exp** env_find(ExpEnv* self,ExpSymbol* var){
    Exp** e = map_get(self->map,var->string->data);
    if(!e&&self->outer){
        return env_find(self->outer,var);
    }
    return e;
}
void env_set(ExpEnv*self,ExpSymbol* key,Exp* var){
    map_insert(self->map,key->string->data,&var);
}
Exp* apply_form(VM*vm, Exp* env,Exp* form,Exp* exp){
    return ((ExpFunc*)form)->func(vm,env,exp);
}
Exp* vm_apply(VM* vm){
    ExpContinuation** cont = array_get(((ExpCallcc*)vm->call_cc)->list,((ExpCallcc*)vm->call_cc)->list->size-1);
    if((*cont)->proc->type == ExpTypeForm){
        return ((ExpForm*)(*cont))->func(vm,(*cont)->env,(*cont)->args);
    }else if((*cont)->proc->type == ExpTypeFunc){
        return ((ExpFunc*)(*cont)->proc)->func(vm,(*cont)->env,(*cont)->args);
    }else if((*cont)->proc->type == ExpTypeProc){
        return eval(vm,(*cont)->env,((ExpProc*)(*cont)->proc)->body);
    }
}
Exp* apply_proc(VM* vm,Exp* env,Exp* proc,ExpList* args){
    Exp* ret = NULL;
    ExpContinuation* cont = exp_new_contiunation(vm);
    cont->args = args;
    cont->proc = proc;
    add_to_root(cont);
    if(proc->type == ExpTypeFunc||proc->type==ExpTypeForm){
        cont->env = env;
        array_push(vm->call_cc->list,&cont);
        ret = vm_apply(vm);
    }else{
        add_to_root(args);
        ExpEnv* proc_env = exp_new_env(vm,env);
        add_to_root(proc_env);
        for(int i = 0;i<((ExpProc*)proc)->param->list->size;i++){
            ExpSymbol** key = array_get(((ExpProc*)proc)->param->list,i);
            if(strcmp((*key)->string,".")==0){
                key = array_get(((ExpProc*)proc)->param->list,i+1);
                ExpList* rest = exp_new_list(vm);
                for(int j = i;j<args->list->size;j++){
                    Exp** value = array_get(args->list,j);
                    array_push(rest->list,value);
                }
                env_set(proc_env,*key,rest);
                break;
            }
            Exp** value = array_get(args->list,i);
            env_set(proc_env,*key,*value);
        }
        cont->env = proc_env;
        array_push(vm->call_cc->list,&cont);
        remove_from_root(args);
        remove_from_root(proc_env);
        ret = vm_apply(vm);
        if(proc->type == ExpTypeMacro){
            ret =eval(vm,env,ret);
        }
    }
    array_pop(vm->call_cc->list);
    remove_from_root(cont);
    return ret;
}

Exp* eval(VM* vm,Exp* env,Exp* exp){
    Exp* ret = NULL;
    if(exp->type == ExpTypeSymbol){
        ret = *env_find(env,exp);
    }else if(exp->type == ExpTypeNum||exp->type == ExpTypeString||exp->type == ExpTypeChar){
        ret = exp;
    }else if(((ExpList*)exp)->list->size==0){
        ret = exp;
    }else{
        ExpList* exp_list =(ExpList*)exp;
        Exp** head = array_get(exp_list->list,0);
        Exp* proc = eval(vm,env,*head);
        if(proc->type == ExpTypeForm){
            return apply_form(vm,env,proc,exp);
        }else if(proc->type == ExpTypeCallcc){
            ExpContinuation** outer = array_get(vm->call_cc->list,vm->call_cc->list->size-1);
            array_push(((ExpCallcc*)proc)->list,outer);
            vm->call_cc = proc;
            Exp** second = array_get(exp_list->list,1);
            ret = *second;
        }else{
            ExpFunc* f = (ExpFunc*)proc;
            ExpList* args = exp_new_list(vm);
            add_to_root(args);
            for(int i = 1;i<exp_list->list->size;i++){
                Exp** item = array_get(exp_list->list,i);
                if(proc->type == ExpTypeMacro||proc->type == ExpTypeForm){
                    array_push(args->list,item);
                }else{
                    Exp* argv = eval(vm,env,*item);
                    array_push(args->list,&argv);
                }
            }
            remove_from_root(args);
            ret = apply_proc(vm,env,proc,args);
        }
    }
    return ret;
}

//lib
Exp* build_in_begin(VM* vm,Exp* env,ExpList* body){
    Exp** last = array_get(body->list,body->list->size-1);
    return *last;
}
Exp* build_in_add(VM* vm,Exp* env,ExpList* body){
    ExpNumber* num = exp_new_number(vm,0);
    for(int i =0;i<body->list->size;i++){
        ExpNumber** item = array_get(body->list,i);
        num->number += (*item)->number;
    }
    return num;
}
Exp* build_in_sub(VM* vm,Exp* env,ExpList* body){
    ExpNumber** first = array_get(body->list,0);
    ExpNumber** second = array_get(body->list,1);
    ExpNumber* num =  exp_new_number(vm,0);
    num->number = (*first)->number - (*second) ->number;
    return num;
}
Exp* build_in_mult(VM* vm,Exp* env,ExpList* body){
    ExpNumber* num = exp_new_number(vm,1);
    for(int i =0;i<body->list->size;i++){
        ExpNumber** item = array_get(body->list,i);
        num->number *= (*item)->number;
    }
    return num;
}
Exp* build_in_div(VM* vm,Exp* env,ExpList* body){
    ExpNumber** first = array_get(body->list,0);
    ExpNumber** second = array_get(body->list,1);
    ExpNumber* num = exp_new_number(vm,0);
    num->number = (*first)->number / (*second) ->number;
    return num;
}
Exp* build_in_gt(VM* vm,Exp* env,ExpList* body){
    ExpNumber** first = array_get(body->list,0);
    ExpNumber** second = array_get(body->list,1);
    ExpNumber* num = exp_new_number(vm,0);
    num->number = (*first)->number > (*second) ->number;
    return num;
}
Exp* build_in_lt(VM* vm,Exp* env,ExpList* body){
    ExpNumber** first = array_get(body->list,0);
    ExpNumber** second = array_get(body->list,1);
    ExpNumber* num = exp_new_number(vm,0);
    num->number = (*first)->number < (*second) ->number;
    return num;
}
Exp* build_in_ge(VM* vm,Exp* env,ExpList* body){
    ExpNumber** first = array_get(body->list,0);
    ExpNumber** second = array_get(body->list,1);
    ExpNumber* num = exp_new_number(vm,0);
    num->number = (*first)->number >= (*second) ->number;
    return num;
}
Exp* build_in_le(VM* vm,Exp* env,ExpList* body){
    ExpNumber** first = array_get(body->list,0);
    ExpNumber** second = array_get(body->list,1);
    ExpNumber* num = exp_new_number(vm,0);
    num->number = (*first)->number <= (*second) ->number;
    return num;
}
Exp* build_in_eq(VM* vm,Exp* env,ExpList* body){
    ExpNumber** first = array_get(body->list,0);
    ExpNumber** second = array_get(body->list,1);
    ExpNumber* num =  exp_new_number(vm,0);
    num->number = (*first)->number == (*second) ->number;
    return num;
}
Exp* build_in_not(VM* vm,Exp* env,ExpList* body){
    ExpNumber** first = array_get(body->list,0);
    ExpNumber* num = exp_new_number(vm,0);
    num->number = !(*first)->number;
    return num;
}
Exp* build_in_and(VM* vm,Exp* env,ExpList* body){
    ExpNumber** first = array_get(body->list,0);
    ExpNumber** second = array_get(body->list,1);
    ExpNumber* num = exp_new_number(vm,0);
    num->number = (*first)->number && (*second) ->number;
    return num;
}
Exp* build_in_or(VM* vm,Exp* env,ExpList* body){
    ExpNumber** first = array_get(body->list,0);
    ExpNumber** second = array_get(body->list,1);
    ExpNumber* num = exp_new_number(vm,0);
    num->number = (*first)->number || (*second) ->number;
    return num;
}
Exp* build_in_list(VM* vm,Exp* env,ExpList* body){

    return body;
}

Exp* build_in_apply(VM* vm,Exp* env,ExpList* body){
    Exp* ret;
    Exp** proc = array_get(body->list,0);
    for(int i = 1;i<body->list->size;i++){
        Exp** list_to_apply = array_get(body->list,i);
        ret = apply_proc(vm,env,*proc,*list_to_apply);
    }
    return ret;
}

Exp* build_in_cons(VM* vm,Exp* env,ExpList* body){
    
    ExpList* list = exp_new_list(vm);
    Exp** first = array_get(body->list,0);
    ExpList** second = array_get(body->list,1);

    array_push(list->list,first);
    array_append(list->list,(*second)->list);
    return list;
}
Exp* build_in_car(VM* vm,Exp* env,ExpList* body){
    ExpList** first = array_get(body->list,0);
    Exp** car = array_get((*first)->list,0);
    return *car;
}
Exp* build_in_cdr(VM* vm,Exp* env,ExpList* body){
    ExpList** first = array_get(body->list,0);

    ExpList* list = exp_new_list(vm);
    for(int i =1;i<(*first)->list->size;i++){
        Exp**item = array_get((*first)->list,i);
        array_push(list->list,item);
    }
    return list;
}
Exp* build_in_null(VM* vm,Exp* env,ExpList* body){
    Exp** first = array_get(body->list,0);
    ExpNumber* res= exp_new_number(vm,0);

    if((*first)->type==ExpTypeList&&((ExpList*)(*first))->list->size==0){
        res->number = 1;
    }
    return res;
}
Exp* build_in_length(VM* vm,Exp* env,ExpList* body){
    Exp** first = array_get(body->list,0);
    ExpNumber* res= exp_new_number(vm,0);
    if((*first)->type==ExpTypeList){
        res->number = ((ExpList*)(*first))->list->size;
    }else if((*first)->type==ExpTypeString){
        res->number = ((ExpString*)(*first))->len;
    }
    return res;
}
Exp* build_in_sym_str(VM* vm, Exp* env,ExpList* body){
    ExpSymbol** first = array_get(body->list,0);
    return (*first)->string;
}
Exp* build_in_str_sym(VM* vm,Exp* env,ExpList* body){
    ExpString** first = array_get(body->list,0);
    ExpSymbol* res= exp_new_symbol(vm,*first);
    return res;
}
Exp* build_in_str_append(VM* vm,Exp* env,ExpList* body){
    ExpString* res= exp_new(vm,ExpTypeString,sizeof(ExpString));
    int alloc_size = 1;
    for(int i =0;i<body->list->size;i++){
        ExpString** item = array_get(body->list,i);
        alloc_size+=strlen((*item)->data);
    }
    res->data = malloc(alloc_size);
    memset(res->data,0,alloc_size);
    int offset = 0;
    for(int i =0;i<body->list->size;i++){
        ExpString** item = array_get(body->list,i);
        int copy_size = strlen((*item)->data);
        memcpy(res->data+offset,(*item)->data,copy_size);
        offset+=copy_size;
    }
    return res;
}
Exp* build_in_load(VM* vm,Exp* env,ExpList* body){
    ExpString** first = array_get(body->list,0);
    return vm_load(vm,(*first)->data);
}
Exp* build_in_readchar(VM* vm,Exp* env,ExpList* body){
    Exp* res = exp_new_char(vm,fgetc(stdin));
    return res;
}
Exp* build_in_display(VM* vm,Exp* env,ExpList* body){
    Exp** first = array_get(body->list,0);
    char buffer[1024]={0};
    to_string(*first,buffer);
    printf("%s",buffer);
    return *first;
}
Exp* build_in_char_eq(VM* vm,Exp* env,ExpList* body){
    ExpChar** first = array_get(body->list,0);
    ExpChar** second = array_get(body->list,1);
    ExpNumber* res = exp_new_number(vm,0);
    res->number = (*first)->character == (*second)->character;
    return res;
}
Exp* build_in_random(VM* vm,Exp* env,ExpList* body){
    ExpNumber** first = array_get(body->list,0);
    ExpNumber* res = exp_new_number(vm,0);
    res->number = rand()%((int)(*first)->number);
    return res;
}
Exp* make_fun(VM* vm,Callable func){
    Exp* v = exp_new_func(vm,func);
    return v;
}
Exp* build_in_define(VM* vm,Exp* env,ExpList* exp){
    Exp** first = array_get(exp->list,1);
    Exp** val = array_get(exp->list,2);
    env_set(env,*first,eval(vm,env,*val));
    return *first;
   
}
Exp* build_in_lambda(VM* vm,Exp* env,ExpList* exp){
    Exp** param = array_get(exp->list,1);
    Exp** body = array_get(exp->list,2);
    ExpProc* ret = exp_new_proc(vm,env,*body,*param);
    return ret;
}
Exp* build_in_let(VM* vm,Exp* env,ExpList* exp){
    ExpList** bind = array_get(exp->list,1);
    Exp** body = array_get(exp->list,2);
    ExpEnv* let_env = exp_new_env(vm,env);

    for(int i =0;i<(*bind)->list->size;i++){
        ExpList** kv = array_get((*bind)->list,i);
        Exp** key = array_get((*kv)->list,0);
        Exp** value = array_get((*kv)->list,1);
        env_set(let_env,*key,*value);
    }
    return eval(vm,let_env,*body);
}
Exp* build_in_set(VM* vm,Exp* env,ExpList* exp){
    Exp** key = array_get(exp->list,1);
    Exp** val = array_get(exp->list,2);
    Exp** obj = env_find(env,*key);
    *obj = eval(vm,env,*val);
    return *obj;
}
Exp* build_in_if(VM* vm,Exp* env,ExpList* exp){
    Exp** condition_exp = array_get(exp->list,1);
    Exp** seq = NULL;
    Exp* condition = eval(vm,env,*condition_exp);
    
    if(condition->type == ExpTypeNum&&((ExpNumber*)condition)->number 
    || condition->type == ExpTypeList&&((ExpList*)condition)->list->size){
        seq = array_get(exp->list,2);
    }else{
        seq = array_get(exp->list,3);
    }
    return eval(vm,env,*seq);
}
Exp* build_in_quote(VM* vm,Exp* env,ExpList* exp){
    Exp** quote = array_get(exp->list,1);
    return *quote;
}
Exp* build_in_macro(VM* vm,Exp* env,ExpList* exp){
    ExpSymbol** key = array_get(exp->list,1);
    Exp** param = array_get(exp->list,2);
    Exp** body = array_get(exp->list,3);
    Exp* macro = exp_new_macro(vm,env,*body,*param);
    env_set(env,*key,macro);
    return *key;  
}

Exp* build_in_cond(VM* vm,Exp* env,ExpList* body){
    Exp* else_exp = NULL;
    for(int i =1;i<body->list->size;i++){
        ExpList** cond = array_get(body->list,i);
        Exp** test = array_get((*cond)->list,0);
        Exp** exe = array_get((*cond)->list,1);
        if((*test)->type==ExpTypeSymbol&&strcmp(((ExpSymbol*)(*test))->string->data,"else")==0){
            else_exp = *exe;
        }else{
            ExpNumber* test_res = eval(vm,env,*test);
            if(test_res->number){
                return eval(vm,env,*exe);
            }
        }
    }
    if(else_exp){
        return eval(vm,env,else_exp);
    }
    return NULL;
}
Exp* build_in_callcc(VM* vm,Exp* env,ExpList* body){
    Exp**proc = array_get(body->list,0);
    ExpCallcc* callcc = exp_new_callcc(vm);
    add_to_root(callcc);
    for(int i =0;i<vm->call_cc->list->size;i++){
        ExpContinuation* cont = array_get(vm->call_cc->list,i);
        array_push(callcc->list,cont);
    }
    ExpList* args = exp_new_list(vm);
    remove_from_root(callcc);
    array_push(args->list,&callcc);
    return apply_proc(vm,env,*proc,args);
}
Exp* make_form(VM* vm,Callable func){
    Exp* v = exp_new_form(vm,func);
    return v;
}
void env_top_form(VM* vm, char* str,Callable val){
    ExpString* exp_str = exp_new_string(vm,str);
    add_to_root(exp_str);
    ExpSymbol* sym = exp_new_symbol(vm,exp_str);
    add_to_root(sym);
    ExpForm* form = exp_new_form(vm,val);
    remove_from_root(exp_str);
    remove_from_root(sym);
    env_set(vm->global_env,sym,form);
}
void env_top_func(VM* vm, char* str,Callable val){
    ExpString* exp_str = exp_new_string(vm,str);
    add_to_root(exp_str);
    ExpSymbol* sym = exp_new_symbol(vm,exp_str);
    add_to_root(sym);
    ExpFunc* form = exp_new_func(vm,val);
    remove_from_root(exp_str);
    remove_from_root(sym);
    env_set(vm->global_env,sym,form);
}
Exp* standard_env(VM* vm){
    vm->global_env = exp_new_env(vm,NULL);
    add_to_root(vm->global_env);
    //special form
    env_top_form(vm,"define",build_in_define);
    env_top_form(vm,"lambda",build_in_lambda);
    env_top_form(vm,"let",build_in_let);
    env_top_form(vm,"set!",build_in_set);
    env_top_form(vm,"if",build_in_if);
    env_top_form(vm,"quote",build_in_quote);
    env_top_form(vm,"define-macro",build_in_macro);
    env_top_form(vm,"cond",build_in_cond);
    
    //func
    env_top_func(vm,"call/cc",build_in_callcc);
    env_top_func(vm,"begin",build_in_begin);
    env_top_func(vm,"cons",build_in_cons);
    env_top_func(vm,"car",build_in_car);
    env_top_func(vm,"cdr",build_in_cdr);
    env_top_func(vm,"list",build_in_list);
    env_top_func(vm,"apply",build_in_apply);

    env_top_func(vm,"+",build_in_add);
    env_top_func(vm,"-",build_in_sub);
    env_top_func(vm,"*",build_in_mult);
    env_top_func(vm,"/",build_in_div);
    env_top_func(vm,">",build_in_gt);
    env_top_func(vm,"<",build_in_lt);
    env_top_func(vm,">=",build_in_ge);
    env_top_func(vm,"<=",build_in_le);
    env_top_func(vm,"=",build_in_eq);
    env_top_func(vm,"equal?",build_in_eq);

    env_top_func(vm,"not",build_in_not);
    env_top_func(vm,"and",build_in_and);
    env_top_func(vm,"or",build_in_or);
    env_top_func(vm,"null?",build_in_null);
    env_top_func(vm,"length",build_in_length);
    env_top_func(vm,"symbol->string",build_in_sym_str);
    env_top_func(vm,"string->symbol",build_in_str_sym);
    env_top_func(vm,"string-append",build_in_str_append);
    time_t t;
    srand((unsigned)time(&t));
    env_top_func(vm,"read-char",build_in_readchar);
    env_top_func(vm,"display",build_in_display);
    env_top_func(vm,"char=?",build_in_char_eq);
    env_top_func(vm,"random",build_in_random);
    env_top_func(vm,"load",build_in_load);

}


//memory
Exp* exp_new(VM* vm, ExpType type, int size){
    if(vm->exp_num-vm->last_gc_num > vm->gc_thre){
        vm_gc(vm);
    }
    Exp* exp = malloc(size);
    memset(exp,0,size);
    exp->type = type;
    exp->next = vm->head;
    vm->head = exp;
    vm->exp_num++;
    return exp;
}
void mark(Exp* exp){
    if(exp->flags&ExpFlagMarked)return;
    exp->flags |= ExpFlagMarked;
    if(exp->type == ExpTypeList){
        for(int i =0;i<((ExpList*)exp)->list->size;i++){
            Exp** item = array_get(((ExpList*)exp)->list,i);
            mark(*item);
        }
    }else if(exp->type == ExpTypeProc){
        mark(((ExpProc*)exp)->body);
        mark(((ExpProc*)exp)->env);
        mark(((ExpProc*)exp)->param);
    }else if(exp->type == ExpTypeMacro){
        mark(((ExpMacro*)exp)->body);
        mark(((ExpMacro*)exp)->env);
        mark(((ExpMacro*)exp)->param);
    }else if(exp->type == ExpTypeEnv){
        MapIter iter = map_iter(((ExpEnv*)exp)->map);
        char* key;
        while (key = map_next(((ExpEnv*)exp)->map,&iter))
        {
            Exp** item = map_get(((ExpEnv*)exp)->map,key);
            mark(*item);
        }
    }else if(exp->type == ExpTypeCallcc){
        for(int i =0;i<((ExpCallcc*)exp)->list->size;i++){
            Exp** item = array_get(((ExpCallcc*)exp)->list,i);
            mark(*item);
        }
        
    }else if(exp->type == ExpTypeContinuation){
        mark(((ExpContinuation*)exp)->args);
        mark(((ExpContinuation*)exp)->env);
        mark(((ExpContinuation*)exp)->proc);
    }else if(exp->type==ExpTypeSymbol){
        mark(((ExpSymbol*)exp)->string);
    }
}
void exp_free(Exp* exp){
    if(exp->type == ExpTypeString){
        free(((ExpString*)exp)->data);
    }else if(exp->type == ExpTypeList){
        array_destroy(((ExpList*)exp)->list);
    }else if(exp->type == ExpTypeEnv){
        map_destroy(((ExpEnv*)exp)->map);
    }else if(exp->type==ExpTypeCallcc){
        array_destroy(((ExpCallcc*)exp)->list);
    }
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
    Exp* res = eval(vm,vm->global_env,list_to_eval);
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
    vm->call_cc = exp_new_callcc(vm);
    add_to_root(vm->call_cc);
    vm->gc_thre = 64;
    standard_env(vm);
    remove_from_root(vm->call_cc);

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
    mark(vm->call_cc);

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
        to_string(((ExpSymbol*)obj)->string,str);
        break;
    case ExpTypeNum:
        sprintf(str," %f ",((ExpNumber*)obj)->number);
        break;
    case ExpTypeString:
        sprintf(str,"\"%s\"",((ExpString*)obj)->data);
        break;
    case ExpTypeList:
        int offset = 0;
        sprintf(str+offset,"(");
        offset += 1;
        for(int i =0;i<((ExpList*)obj)->list->size;i++){
            char buf[128]={0};
            Exp** item = array_get(((ExpList*)obj)->list,i);
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
    case ExpTypeCallcc:
        sprintf(str,"<Callcc>");
        break;
    case ExpTypeChar:
        sprintf(str,"'%c'",((ExpChar*)obj)->character);
        break;
    case ExpTypePointer:
        sprintf(str,"<Pointer>");
        break;
    default:
        break;
    }
}