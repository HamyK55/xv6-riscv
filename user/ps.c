#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// this user program invokes system call pstate
int main(int argc, char *argv[])
{
    ps();
    exit(0);
}