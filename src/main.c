
#include "stdio.h"
#include "lisp/lisp.h"
int main(int argc,char*argv){
    VM vm;
    vm_init(&vm);
    int num = 0;
    char code[512]={0};
    vm_eval(&vm,"(define fib (lambda (n) (if (< n 2) 1 (+ (fib (- n 1)) (fib (- n 2))))))");
    while (1)
    {
        printf(">");
        //fgets(code,512,stdin);
        Exp* res = vm_eval(&vm,"(fib 2))");
        if(res){
            char res_buf[512]={0};
            to_string(res,res_buf);
            printf("%s\n",res_buf);
        }
        num = vm.exp_num;
        vm_gc(&vm);
        num = num-vm.exp_num;
        printf("%d exp free! %d exp remain.\n",num,vm.exp_num);

        Exp* next=vm.head;
        num = 0;
        while (next)
        {
            char res_buf[512]={0};
            to_string(next,res_buf);
            printf("%d:%s\n",num++,res_buf);
            next = next->next;
        }
    }
    return 0;
}