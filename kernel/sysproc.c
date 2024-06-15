#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}


// print all the processes in states runnable, running, sleeping 
uint64
sys_pstate(void)
{
  struct proc *currentProcPointer;
  struct proc *p = myproc();

  pagetable_t pt = p->pagetable;

  printf("pid   name    state   parent\n");
  printf("____________________________\n");
  
  for (currentProcPointer = pt; currentProcPointer < &pt[NPROC]; currentProcPointer++)
  {
    int pid = currentProcPointer->pid;
    char name[16] = currentProcPointer->name;
    enum procstate state = currentProcPointer->state;
    char pname[16]= currentProcPointer->parent->name;

    printf("%d %s %s %s\n", pid, name, state, pname);
  }
  
  return 0;
}