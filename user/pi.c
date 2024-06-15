#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// this is a program to run in the background, a tester program for pstate sys_call
int main(int argc, char *argv[])
{
    int x = 0;
    while (1)
    {
        x++;
    }  
    exit(0);
}