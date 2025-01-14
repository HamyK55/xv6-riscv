#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

struct cpu cpus[NCPU];

struct proc proc[NPROC];

struct proc *initproc;

int nextpid = 1;
struct spinlock pid_lock;

extern void forkret(void);
static void freeproc(struct proc *p);

extern char trampoline[]; // trampoline.S

// helps ensure that wakeups of wait()ing
// parents are not lost. helps obey the
// memory model when using p->parent.
// must be acquired before any p->lock.
struct spinlock wait_lock;

// Allocate a page for each process's kernel stack.
// Map it high in memory, followed by an invalid
// guard page.
void
proc_mapstacks(pagetable_t kpgtbl)
{
  struct proc *p;
  
  for(p = proc; p < &proc[NPROC]; p++) {
    char *pa = kalloc();
    if(pa == 0)
      panic("kalloc");
    uint64 va = KSTACK((int) (p - proc));
    kvmmap(kpgtbl, va, (uint64)pa, PGSIZE, PTE_R | PTE_W);
  }
}

// initialize the proc table.
void
procinit(void)
{
  struct proc *p;
  
  initlock(&pid_lock, "nextpid");
  initlock(&wait_lock, "wait_lock");
  for(p = proc; p < &proc[NPROC]; p++) {
      initlock(&p->lock, "proc");
      p->state = UNUSED;
      p->kstack = KSTACK((int) (p - proc));
  }
}

// Must be called with interrupts disabled,
// to prevent race with process being moved
// to a different CPU.
int
cpuid()
{
  int id = r_tp();
  return id;
}

// Return this CPU's cpu struct.
// Interrupts must be disabled.
struct cpu*
mycpu(void)
{
  int id = cpuid();
  struct cpu *c = &cpus[id];
  return c;
}

// Return the current struct proc *, or zero if none.
struct proc*
myproc(void)
{
  push_off();
  struct cpu *c = mycpu();
  struct proc *p = c->proc;
  pop_off();
  return p;
}

int
allocpid()
{
  int pid;
  
  acquire(&pid_lock);
  pid = nextpid;
  nextpid = nextpid + 1;
  release(&pid_lock);

  return pid;
}

// Look in the process table for an UNUSED proc.
// If found, initialize state required to run in the kernel,
// and return with p->lock held.
// If there are no free procs, or a memory allocation fails, return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->state == UNUSED) {
      goto found;
    } else {
      release(&p->lock);
    }
  }
  return 0;

found:
  p->priority = 0;
  p->pid = allocpid();
  p->state = USED;

  // Allocate a trapframe page.
  if((p->trapframe = (struct trapframe *)kalloc()) == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // An empty user page table.
  p->pagetable = proc_pagetable(p);
  if(p->pagetable == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // Set up new context to start executing at forkret,
  // which returns to user space.
  memset(&p->context, 0, sizeof(p->context));
  p->context.ra = (uint64)forkret;
  p->context.sp = p->kstack + PGSIZE;

  return p;
}

// free a proc structure and the data hanging from it,
// including user pages.
// p->lock must be held.
static void
freeproc(struct proc *p)
{
  if(p->trapframe)
    kfree((void*)p->trapframe);
  p->trapframe = 0;
  if(p->pagetable)
    proc_freepagetable(p->pagetable, p->sz);
  p->pagetable = 0;
  p->sz = 0;
  p->pid = 0;
  p->parent = 0;
  p->name[0] = 0;
  p->chan = 0;
  p->killed = 0;
  p->xstate = 0;
  p->priority = 0;
  p->state = UNUSED;
}

// Create a user page table for a given process, with no user memory,
// but with trampoline and trapframe pages.
pagetable_t
proc_pagetable(struct proc *p)
{
  pagetable_t pagetable;

  // An empty page table.
  pagetable = uvmcreate();
  if(pagetable == 0)
    return 0;

  // map the trampoline code (for system call return)
  // at the highest user virtual address.
  // only the supervisor uses it, on the way
  // to/from user space, so not PTE_U.
  if(mappages(pagetable, TRAMPOLINE, PGSIZE,
              (uint64)trampoline, PTE_R | PTE_X) < 0){
    uvmfree(pagetable, 0);
    return 0;
  }

  // map the trapframe page just below the trampoline page, for
  // trampoline.S.
  if(mappages(pagetable, TRAPFRAME, PGSIZE,
              (uint64)(p->trapframe), PTE_R | PTE_W) < 0){
    uvmunmap(pagetable, TRAMPOLINE, 1, 0);
    uvmfree(pagetable, 0);
    return 0;
  }

  return pagetable;
}

// Free a process's page table, and free the
// physical memory it refers to.
void
proc_freepagetable(pagetable_t pagetable, uint64 sz)
{
  uvmunmap(pagetable, TRAMPOLINE, 1, 0);
  uvmunmap(pagetable, TRAPFRAME, 1, 0);
  uvmfree(pagetable, sz);
}

// a user program that calls exec("/init")
// assembled from ../user/initcode.S
// od -t xC ../user/initcode
uchar initcode[] = {
  0x17, 0x05, 0x00, 0x00, 0x13, 0x05, 0x45, 0x02,
  0x97, 0x05, 0x00, 0x00, 0x93, 0x85, 0x35, 0x02,
  0x93, 0x08, 0x70, 0x00, 0x73, 0x00, 0x00, 0x00,
  0x93, 0x08, 0x20, 0x00, 0x73, 0x00, 0x00, 0x00,
  0xef, 0xf0, 0x9f, 0xff, 0x2f, 0x69, 0x6e, 0x69,
  0x74, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00
};

// Set up first user process.
void
userinit(void)
{
  struct proc *p;

  p = allocproc();
  initproc = p;
  
  // allocate one user page and copy initcode's instructions
  // and data into it.
  uvmfirst(p->pagetable, initcode, sizeof(initcode));
  p->sz = PGSIZE;

  // prepare for the very first "return" from kernel to user.
  p->trapframe->epc = 0;      // user program counter
  p->trapframe->sp = PGSIZE;  // user stack pointer

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  p->state = RUNNABLE;

  release(&p->lock);
}

// Grow or shrink user memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint64 sz;
  struct proc *p = myproc();

  sz = p->sz;
  if(n > 0){
    if((sz = uvmalloc(p->pagetable, sz, sz + n, PTE_W)) == 0) {
      return -1;
    }
  } else if(n < 0){
    sz = uvmdealloc(p->pagetable, sz, sz + n);
  }
  p->sz = sz;
  return 0;
}

// Create a new process, copying the parent.
// Sets up child kernel stack to return as if from fork() system call.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *p = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy user memory from parent to child.
  if(uvmcopy(p->pagetable, np->pagetable, p->sz) < 0){
    freeproc(np);
    release(&np->lock);
    return -1;
  }
  np->sz = p->sz;

  // copy saved user registers.
  *(np->trapframe) = *(p->trapframe);

  // Cause fork to return 0 in the child.
  np->trapframe->a0 = 0;

  // increment reference counts on open file descriptors.
  for(i = 0; i < NOFILE; i++)
    if(p->ofile[i])
      np->ofile[i] = filedup(p->ofile[i]);
  np->cwd = idup(p->cwd);

  safestrcpy(np->name, p->name, sizeof(p->name));

  pid = np->pid;

  release(&np->lock);

  acquire(&wait_lock);
  np->parent = p;
  release(&wait_lock);

  acquire(&np->lock);
  np->state = RUNNABLE;
  release(&np->lock);

  return pid;
}

// Pass p's abandoned children to init.
// Caller must hold wait_lock.
void
reparent(struct proc *p)
{
  struct proc *pp;

  for(pp = proc; pp < &proc[NPROC]; pp++){
    if(pp->parent == p){
      pp->parent = initproc;
      wakeup(initproc);
    }
  }
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait().
void
exit(int status)
{
  struct proc *p = myproc();

  if(p == initproc)
    panic("init exiting");

  // Close all open files.
  for(int fd = 0; fd < NOFILE; fd++){
    if(p->ofile[fd]){
      struct file *f = p->ofile[fd];
      fileclose(f);
      p->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(p->cwd);
  end_op();
  p->cwd = 0;

  acquire(&wait_lock);

  // Give any children to init.
  reparent(p);

  // Parent might be sleeping in wait().
  wakeup(p->parent);
  
  acquire(&p->lock);

  // update pcb with param status, and zombie state
  p->xstate = status;
  p->state = ZOMBIE;

  release(&wait_lock);

  // Jump into the scheduler, never to return.
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(uint64 addr)
{
  struct proc *pp;
  int havekids, pid;
  struct proc *p = myproc();

  acquire(&wait_lock);

  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(pp = proc; pp < &proc[NPROC]; pp++){
      if(pp->parent == p){
        // make sure the child isn't still in exit() or swtch().
        acquire(&pp->lock);

        havekids = 1;
        if(pp->state == ZOMBIE){
          // Found one. do memory cleanup

          
          pid = pp->pid;

          // rmbr: each page table to unique to a process? virtual memory is not the same as physical storage
          if(addr != 0 && copyout(p->pagetable, addr, (char *)&pp->xstate,
                                  sizeof(pp->xstate)) < 0) {
            release(&pp->lock);
            release(&wait_lock);
            return -1;
          }
          freeproc(pp);
          release(&pp->lock);
          release(&wait_lock);
          return pid;
        }
        // not in zombie state, 
        release(&pp->lock);
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || killed(p)){
      release(&wait_lock);
      return -1;
    }
    
    // Wait for a child to exit.
    sleep(p, &wait_lock);  //DOC: wait-sleep
  }
}

// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run.
//  - swtch to start running that process.
//  - eventually that process transfers control
//    via swtch back to the scheduler.
void scheduler(void) {
    struct proc *p;
    struct proc *high_prio_proc;
    struct cpu *c = mycpu();
    c->proc = 0;

    for(;;){
        // Avoid deadlock by ensuring that devices can interrupt.
        intr_on();

        high_prio_proc = 0;

        for(p = proc; p < &proc[NPROC]; p++) {
            acquire(&p->lock);
            if(p->state == RUNNABLE) {
                // If we haven't selected a process yet, or this process has a higher priority
                if(high_prio_proc == 0 || p->priority < high_prio_proc->priority) {
                    if (high_prio_proc != 0) { // if high prio has been initilized already release the lock before updating
                        release(&high_prio_proc->lock);
                    }  
                    high_prio_proc = p; // if high prio proc hasnt been initialized or p has higher priority then update
                } 
                else {
                    release(&p->lock);
                }
            } else {
                release(&p->lock);
            }
        }

        if (high_prio_proc != 0) {
            // Switch to the highest priority process.
            high_prio_proc->state = RUNNING;
            c->proc = high_prio_proc;
            swtch(&c->context, &high_prio_proc->context);

            // Process is done running for now.
            // It should have changed its p->state before coming back.
            c->proc = 0;
            release(&high_prio_proc->lock);
        }
    }
}

// Switch to scheduler.  Must hold only p->lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->noff, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc(); // pcb

  if(!holding(&p->lock))
    panic("sched p->lock");
  if(mycpu()->noff != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(intr_get())
    panic("sched interruptible");

  intena = mycpu()->intena;
  swtch(&p->context, &mycpu()->context);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  struct proc *p = myproc();
  acquire(&p->lock);
  p->state = RUNNABLE;
  sched();
  release(&p->lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch to forkret.
void
forkret(void)
{
  static int first = 1;

  // Still holding p->lock from scheduler.
  release(&myproc()->lock);

  if (first) {
    // File system initialization must be run in the context of a
    // regular process (e.g., because it calls sleep), and thus cannot
    // be run from main().
    first = 0;
    fsinit(ROOTDEV);
  }

  usertrapret();
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  // Must acquire p->lock in order to
  // change p->state and then call sched.
  // Once we hold p->lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup locks p->lock),
  // so it's okay to release lk.

  acquire(&p->lock);  //DOC: sleeplock1
  release(lk);

  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  release(&p->lock);
  acquire(lk);
}

// Wake up all processes sleeping on chan.
// Must be called without any p->lock.
void
wakeup(void *chan)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    if(p != myproc()){
      acquire(&p->lock);
      if(p->state == SLEEPING && p->chan == chan) {
        p->state = RUNNABLE;
      }
      release(&p->lock);
    }
  }
}

// Kill the process with the given pid.
// The victim won't exit until it tries to return
// to user space (see usertrap() in trap.c).
int
kill(int pid)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++){
    acquire(&p->lock);
    if(p->pid == pid){
      p->killed = 1;
      if(p->state == SLEEPING){
        // Wake process from sleep().
        p->state = RUNNABLE;
      }
      release(&p->lock);
      return 0;
    }
    release(&p->lock);
  }
  return -1;
}

void
setkilled(struct proc *p)
{
  acquire(&p->lock);
  p->killed = 1;
  release(&p->lock);
}

int
killed(struct proc *p)
{
  int k;
  
  acquire(&p->lock);
  k = p->killed;
  release(&p->lock);
  return k;
}

// Copy to either a user address, or kernel address,
// depending on usr_dst.
// Returns 0 on success, -1 on error.
int
either_copyout(int user_dst, uint64 dst, void *src, uint64 len)
{
  struct proc *p = myproc();
  if(user_dst){
    return copyout(p->pagetable, dst, src, len);
  } else {
    memmove((char *)dst, src, len);
    return 0;
  }
}

// Copy from either a user address, or kernel address,
// depending on usr_src.
// Returns 0 on success, -1 on error.
int
either_copyin(void *dst, int user_src, uint64 src, uint64 len)
{
  struct proc *p = myproc();
  if(user_src){
    return copyin(p->pagetable, dst, src, len);
  } else {
    memmove(dst, (char*)src, len);
    return 0;
  }
}

// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [USED]      "used",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  struct proc *p;
  char *state;

  printf("\n");
  for(p = proc; p < &proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    printf("%d %s %s", p->pid, state, p->name);
    printf("\n");
  }
}
// Prints relevent data about all the proccesses in the page table
void
pstate(void){
  struct proc *currentProcPointer;
  int proc_num = 0;
  char *state = "\0";
  struct cpu *current_cpu;
  int cpu_num = 0;

  printf("pid\tname\tstate\t\tparent\t\n");
  printf("___________________________________________\n");
  
  // go through page table and print info 
  for (currentProcPointer = proc; (currentProcPointer < &proc[NPROC]) && currentProcPointer->pid > 0; currentProcPointer++)
  {
    proc_num++;

    // get the state into readable text
    switch (currentProcPointer->state)
    {
    case SLEEPING:
      state = "SLEEPING";
      break;
    case RUNNABLE:
      state = "RUNNABLE";
      break;
    case RUNNING:
      state = "RUNNING ";
      break;
    
    default:
      break;
    }

    // deal with init having no parent
    char *pname = "\0";
    if (currentProcPointer->pid == 1){
      pname = "(init)";
    }
    else{
      pname = currentProcPointer->parent->name;
    }

    printf("%d\t%s\t%s\t%s\n",currentProcPointer->pid, currentProcPointer->name, state, pname);
  }
  printf("Total processes: %d\n", proc_num);


  // print all cpus with a process running on it with relevent data
  for (current_cpu = cpus; current_cpu < &cpus[NCPU]; current_cpu++)
  {
    cpu_num++;
    if (current_cpu->proc){ // check if process exists on cpu
      printf("cpu %d: %s\n", cpu_num, current_cpu->proc->name);
    }
  }
}


// Prints pid, name, state, and priority of all the processes on the system
void
ps(void){
  struct proc *currentProcPointer;
  char *state = "\0";

  printf("pid\tname\tstate\t\tpriority\t\n");
  printf("___________________________________________\n");
  
  // go through page table and print info 
  for (currentProcPointer = proc; (currentProcPointer < &proc[NPROC]) && currentProcPointer->pid > 0; currentProcPointer++)
  {

    // get the state into readable text
    switch (currentProcPointer->state)
    {
    case SLEEPING:
      state = "SLEEPING";
      break;
    case RUNNABLE:
      state = "RUNNABLE";
      break;
    case RUNNING:
      state = "RUNNING ";
      break;
    
    default:
      break;
    }

    printf("%d\t%s\t%s\t%d\n",currentProcPointer->pid, currentProcPointer->name, state, currentProcPointer->priority);
  }
}

void
set(int pid, int priority){
  struct proc *currentProcPointer;

  // go through page table and find proc with pid
  for (currentProcPointer = proc; (currentProcPointer < &proc[NPROC]) && currentProcPointer->pid > 0; currentProcPointer++)
  {
    if (currentProcPointer->pid == pid){
      currentProcPointer->priority = priority;
    }
  }
}


// sends relevent data about all the proccesses in the page table to userspace
int
psinfo(struct proc_info *user_buf, struct cpu_info *cpu_user_buf, struct proc_cpu_num *user_num_buf){
  struct proc *currentProcPointer;
  int proc_num = 0;
  char *state = "\0";
  struct cpu *current_cpu;
  int cpu_num = 0;

  struct proc_cpu_num kernel_num_buf[2];

  struct proc_info kernel_proc_buf[NPROC];
  struct cpu_info kernel_cpu_buf[NCPU];
  
  // go through page table and print info 
  for (currentProcPointer = proc; (currentProcPointer < &proc[NPROC]) && currentProcPointer->pid > 0; currentProcPointer++)
  {
    state = "\0";
    // get the state into readable text
    switch (currentProcPointer->state)
    {
    case SLEEPING:
      state = "SLEEPING";
      break;
    case RUNNABLE:
      state = "RUNNABLE";
      break;
    case RUNNING:
      state = "RUNNING\t";
      break;
    
    default:
      break;
    }

    // deal with init having no parent
    char *pname = "\0";
    if (currentProcPointer->pid == 1){
      pname = "(init)";
    }
    else{
      pname = currentProcPointer->parent->name;
    }

    
    // store proc info into my kernel_proc_buffer
    //printf("%d\t%s\t%s\t%s\n",currentProcPointer->pid, currentProcPointer->name, state, pname);

    kernel_proc_buf[proc_num].pid = currentProcPointer->pid;
    strncpy(kernel_proc_buf[proc_num].name, currentProcPointer->name, sizeof(proc->name));
    strncpy(kernel_proc_buf[proc_num].pName, pname, sizeof(proc->name));
    strncpy(kernel_proc_buf[proc_num].state, state, sizeof(state));

    proc_num++;
  }
  kernel_num_buf[0].num = proc_num;

  


  // print all cpus with a process running on it with relevent data
  for (current_cpu = cpus; current_cpu < &cpus[NCPU]; current_cpu++)
  {
    if (current_cpu->proc){ // check if process exists on cpu
      //printf("cpu %d: %s\n", cpu_num, current_cpu->proc->name);
      kernel_cpu_buf[cpu_num].cpu_num = cpu_num;
      strncpy(kernel_cpu_buf[cpu_num].name, current_cpu->proc->name, sizeof(proc->name));
      cpu_num++;
    }
  }
  kernel_num_buf[1].num = cpu_num; 

  // copy kernel data to userspace
  struct proc *p = myproc(); // Get the current process
  pagetable_t pagetable = p->pagetable; // Access the page table of the process

  if (copyout(pagetable, (uint64)user_buf, (char *)kernel_proc_buf, proc_num * sizeof(struct proc_info)) != 0) {
      printf("err on copyout");
  }

  if (copyout(pagetable, (uint64)cpu_user_buf, (char *)kernel_cpu_buf, cpu_num * sizeof(struct cpu_info)) != 0) {
      printf("err on copyout");
  }

  if (copyout(pagetable, (uint64)user_num_buf, (char *)kernel_num_buf, 2 * sizeof(struct proc_cpu_num)) != 0) {
      printf("err on copyout");
  }

  return proc_num;
  
}