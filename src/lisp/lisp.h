typedef struct Env Env;
typedef struct Proc Proc;
typedef struct Exp Exp;
typedef struct Array Array;
typedef struct Map Map;
typedef struct VM VM;
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
    ExpTypeCallcc
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
        char* str;
        Array* list;
        double number;
        Callable call;
        Proc proc;
        Env env;
        char character;
        Array* stack;
    };
    char flags;
}Exp;

typedef struct StackFrame{
    Exp* env;
    Exp* proc;
    Exp* args;
}StackFrame;

typedef struct VM{
    Exp* global_env;
    Exp* head;
    Exp* call_cc;
    int exp_num;
    int gc_thre;
    int last_gc_num;
}VM;

void vm_init(VM*);
void vm_gc(VM*);
Exp* vm_eval(VM*,char*);
void to_string(Exp*,char*);
Exp* vm_load(VM*,char*);