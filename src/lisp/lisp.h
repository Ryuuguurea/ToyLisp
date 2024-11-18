typedef struct Exp Exp;
typedef struct Array Array;
typedef struct Map Map;
typedef struct VM VM;
typedef struct ExpList ExpList;
typedef enum{
    ExpTypeNone,
    ExpTypeList,
    ExpTypeSymbol,
    ExpTypeNum,
    ExpTypeChar,
    ExpTypePointer,
    ExpTypeString,
    ExpTypeFunc,
    ExpTypeProc,
    ExpTypeForm,
    ExpTypeMacro,
    ExpTypeEnv,
    ExpTypeCallcc,
    ExpTypeContinuation,
}ExpType;

typedef enum ExpFlags{
    ExpFlagNone = 0,
    ExpFlagMarked = 1<<0,
    ExpFlagRoot = 1<<1,
}ExpFlags;

typedef Exp* (*Callable)(VM*,Exp*,ExpList*);

typedef struct Exp{
    ExpType type;
    Exp* next;
    char flags;
}Exp;

typedef struct ExpString{
    Exp super;
    char* data;
    int len;
}ExpString;

typedef struct ExpSymbol{
    Exp super;
    ExpString* string;
}ExpSymbol;

typedef struct ExpList{
    Exp super;
    Array* list;
}ExpList;
typedef struct ExpNumber{
    Exp super;
    double number;
}ExpNumber;
typedef struct ExpFunc{
    Exp super;
    Callable func;
}ExpFunc;
typedef struct ExpEnv ExpEnv;
typedef struct ExpEnv{
    Exp super;
    ExpEnv* outer;
    Map* map;
}ExpEnv;
typedef struct ExpProc{
    Exp super;
    ExpEnv* env;
    ExpList* body;
    ExpList* param;
}ExpProc;

typedef struct ExpChar{
    Exp super;
    char character;
}ExpChar;
typedef struct ExpCallcc{
    Exp super;
    Array* list;
}ExpCallcc;
typedef struct ExpContinuation{
    Exp super;
    ExpEnv* env;
    Exp* proc;
    ExpList* args;
}ExpContinuation;
typedef struct ExpMacro{
    Exp super;
    ExpEnv* env;
    ExpList* body;
    ExpList* param;
}ExpMacro;
typedef struct ExpForm{
    Exp super;
    Callable func;
}ExpForm;

typedef struct VM{
    Exp* global_env;
    Exp* head;
    ExpCallcc* call_cc;
    int exp_num;
    int gc_thre;
    int last_gc_num;
}VM;

void add_to_root(Exp*);
void remove_from_root(Exp*);
ExpSymbol* exp_new_symbol(VM*,ExpString*);
ExpString* exp_new_string(VM*,char*);
ExpList* exp_new_list(VM*);
ExpNumber* exp_new_number(VM*,double);
ExpFunc* exp_new_func(VM*,Callable);
ExpProc* exp_new_proc(VM*,ExpEnv*env,ExpList*body,ExpList*param);
ExpEnv* exp_new_env(VM*,ExpEnv*);
ExpChar* exp_new_char(VM*,char);
ExpCallcc* exp_new_callcc(VM*);
ExpContinuation* exp_new_contiunation(VM*);
ExpMacro* exp_new_macro(VM*,ExpEnv*env,ExpList*body,ExpList*param);
ExpForm* exp_new_form(VM*,Callable func);

void vm_init(VM*);
void vm_gc(VM*);
Exp* vm_eval(VM*,char*);
void to_string(Exp*,char*);
Exp* vm_load(VM*,char*);