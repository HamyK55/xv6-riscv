#include "kernel/types.h"
#include "kernel/param.h"
#include "kernel/stat.h"
#include "user/user.h"


// this user program invokes system call pstate
int main(int argc, char *argv[])
{
    struct proc_info proc_user_buf[NPROC];
    struct cpu_info cpu_user_buf[NCPU];
    struct proc_cpu_num proc_cpu_num[2];
    //int num_proc;
    psinfo(proc_user_buf, cpu_user_buf, proc_cpu_num);

    //Print the process information
    printf("pid\tname\tstate\t\tparent\n");
    printf("___________________________________________\n");
    for (int i = 0; i < proc_cpu_num[0].num; i++) {
        printf("%d\t%s\t%s\t%s\n", proc_user_buf[i].pid, proc_user_buf[i].name, proc_user_buf[i].state, proc_user_buf[i].pName);
        printf("___________________________________________\n");
    }


    printf("Total processes: %d\n", proc_cpu_num[0].num);

    for (int i = 0; i < proc_cpu_num[1].num; i++) {
        printf("cpu %d:\t%s\n", cpu_user_buf[i].cpu_num, cpu_user_buf[i].name);
    }

    exit(0);
}