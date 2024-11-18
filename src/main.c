
#include "stdio.h"
#include "lisp/lisp.h"
int main(int argc,char**argv){
    VM vm;
    vm_init(&vm);
    vm_eval(&vm,"(define f (lambda (return) (return 2) 3))");
    vm_eval(&vm,"(f +)");
    vm_eval(&vm,"(call/cc f)");
    vm_eval(&vm,"(call/cc f)");
    vm_eval(&vm,"(call/cc f)");
    if(argc>1){
        vm_load(&vm,argv[1]);
    }
    int num = 0;
    char code[512]={0};
    while (1)
    {
        printf(">");
        fgets(code,512,stdin);
        Exp* res = vm_eval(&vm,code);
        if(res){
            char res_buf[512]={0};
            to_string(res,res_buf);
            printf("%s\n",res_buf);
        }
    }
    return 0;
}