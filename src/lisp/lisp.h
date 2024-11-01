typedef struct Env Env;
typedef struct Proc Proc;
typedef struct Exp Exp;
typedef struct Array Array;
typedef struct Map Map;
typedef struct VM VM;
typedef enum{
    ExpTypeList,
    ExpTypeSymbol,
    ExpTypeNum,
    ExpTypeFunc,
    ExpTypeProc,
    ExpTypeEnv
}Type;

typedef enum ExpFlags{
    ExpFlagNone = 0,
    ExpFlagMarked = 1<<0,
    ExpFlagRoot = 1<<1,
}ExpFlags;

typedef Exp* (*Callable)(Exp*,Exp*);
typedef struct Proc{
    Exp* env;
    Exp* body;
    Exp* param;
}Proc;
typedef struct Env{
    Exp* outer;
    Map* map;
    VM* vm;
}Env;
typedef struct Exp{
    Type type;
    Exp* next;
    union{
        char* symbol;
        Array* list;
        double number;
        Callable call;
        Proc proc;
        Env env;
    };
    char flags;
}Exp;

typedef struct CallStack{
    Exp* env;
    Exp* proc;
    Exp* args;
}CallStack;

typedef struct VM{
    Exp* global_env;
    Exp* head;
    Array* call_stack;
    int exp_num;
    int gc_thre;
    int last_gc_num;
}VM;

void vm_init(VM*);
void vm_gc(VM*);
Exp* vm_eval(VM*,char*);
void to_string(Exp*,char*);