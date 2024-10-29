
#include "stdio.h"
#include "lisp/lisp.h"
int main(int argc,char*argv){
    VM vm;
    vm_init(&vm);
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
        num = vm.exp_num;
        vm_gc(&vm);
        num = num-vm.exp_num;
        printf("%d exp free! %d exp remain.\n",num,vm.exp_num);
    }
    return 0;
}