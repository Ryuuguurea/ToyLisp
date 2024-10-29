typedef struct Env Env;
typedef struct Proc Proc;
typedef struct Exp Exp;
typedef struct Array Array;
typedef struct Map Map;
typedef struct VM VM;
typedef enum{
    List,
    Symbol,
    Number,
    Function,
    Procedure,
    Environment
}Type;

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
    char marked;
    Exp* next;
    union{
        char* symbol;
        Array* list;
        double number;
        Callable call;
        Proc proc;
        Env env;
    };
}Exp;

typedef struct VM{
    Exp* global_env;
    Exp* head;
    int exp_num;
}VM;

void vm_init(VM*);
void vm_gc(VM*);
Exp* vm_eval(VM*,char*);
void to_string(Exp*,char*);