#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

#ifdef CS333_P2
#include "uproc.h"
#endif

#ifdef CS333_P3
#define statecount NELEM(states)
#endif //P3

static char *states[] = {
[UNUSED]    "unused",
[EMBRYO]    "embryo",
[SLEEPING]  "sleep ",
[RUNNABLE]  "runble",
[RUNNING]   "run   ",
[ZOMBIE]    "zombie"
};

#ifdef CS333_P3
struct ptrs {
  struct proc *head;
  struct proc *tail;
};
#endif //P3


static struct {
  struct spinlock lock;
  struct proc proc[NPROC];
#ifdef CS333_P3
  struct ptrs list[statecount];
#endif //P3
#ifdef CS333_P4
  struct ptrs ready[MAXPRIO+1];
  uint PromoteAtTime;
#endif //P4
} ptable;

static struct proc *initproc;

uint nextpid = 1;
extern void forkret(void);
extern void trapret(void);
static void wakeup1(void* chan);

#ifdef CS333_P3
static void initProcessLists(void);
static void initFreeList(void);
static void stateListAdd(struct ptrs*, struct proc*);
static int  stateListRemove(struct ptrs*, struct proc* p);
static void assertState(struct proc*, enum procstate);
#endif //P3

#ifdef CS333_P3
static void
stateListAdd(struct ptrs* list, struct proc* p)
{
  if((*list).head == NULL){
    (*list).head = p;
    (*list).tail = p;
    p->next = NULL;
  } else{
    ((*list).tail)->next = p;
    (*list).tail = ((*list).tail)->next;
    ((*list).tail)->next = NULL;
  } 
}
static int
stateListRemove(struct ptrs* list, struct proc* p)
{
  if((*list).head == NULL || (*list).tail == NULL || p == NULL){
    return -1;
}
  struct proc* current = (*list).head;
  struct proc* previous = 0;
  if(current == p){
    (*list).head = ((*list).head)->next;
    // prevent tail remaining assigned when we’ve removed the only item
    // on the list
    if((*list).tail == p){
      (*list).tail = NULL;
    }
    return 0; 
  }
  while(current){
    if(current == p){
      break; 
    }
    previous = current;
    current = current->next;
  }
  // Process not found, hit eject.
  if(current == NULL){
    return -1; 
  }
  // Process found. Set the appropriate next pointer.
  if(current == (*list).tail){
    (*list).tail = previous;
    ((*list).tail)->next = NULL;
  } else{
    previous->next = current->next;
  }
  // Make sure p->next doesn’t point into the list.
  p->next = NULL;
  return 0; 
}
static void
initProcessLists()
{
  int i;
  for (i = UNUSED; i <= ZOMBIE; i++) {
    ptable.list[i].head = NULL;
    ptable.list[i].tail = NULL;
  }
  #ifdef CS333_P4
  for (i = 0; i <= MAXPRIO; i++) {
    ptable.ready[i].head = NULL;
    ptable.ready[i].tail = NULL;
  }
  #endif //P4
}
static void
initFreeList(void)
{
  struct proc* p;
  for(p = ptable.proc; p < ptable.proc + NPROC; ++p){
    p->state = UNUSED;
    stateListAdd(&ptable.list[UNUSED], p);
  } 
}
static void
assertState(struct proc* p, enum procstate state)
{
  if(p->state == state){
  }
  else
  {
    cprintf("expected %s, got %s\n",states[state],states[p->state]);
    panic("state mismatch!");
  }
}

void
controlR(void)
{
#ifdef CS333_P4
  cprintf("Ready List Processes:\n");
  for(int i=MAXPRIO; i >=0;--i)
  {
    cprintf("%d: ",i);
    struct proc *p = ptable.ready[i].head;
    if(!p){
      cprintf("Nothing !!!\n");
    }
    while(p)
    {
      if(p->next ==0)
	cprintf("(%d,%d)\n", p -> pid, p-> budget);
      else{
	cprintf("(%d,%d)->", p -> pid, p-> budget);
      }
      p = p ->next;
    }
  }
#else
  cprintf("Ready List Processes:\n");
  struct proc *p = ptable.list[RUNNABLE].head;
  if(!p){
    cprintf("Nothing !!!\n");
  }
  while(p)
  {
    if(p->next ==0)
      cprintf("%d\n", p -> pid);
    else{
      cprintf("%d->", p -> pid);
    }
    p = p ->next;
  }
#endif
}
void
controlF(void)
{
  cprintf("Free List Size: ");
  struct proc *p = ptable.list[UNUSED].head;
  int count = 1;
   if (!p)
   {
      cprintf(" 0 processes\n");
      return;
   }
   while (p -> next)
   {
      count ++;
      p = p->next;
   }
   cprintf(" %d processes\n", count);
}
void
controlS(void)
{
  cprintf("Sleep List Processes:\n");
  struct proc *p = ptable.list[SLEEPING].head;
  if (!p)
   {
      cprintf("Nothing !!!\n");
      return;
   }
   while (p)
   {
      if(p -> next ==0)
         cprintf("%d\n", p -> pid);
      else
         cprintf("%d -> ", p -> pid);
      p = p->next;
   }
}
void
controlZ(void)
{
  cprintf("Zombie List Processes:\n");
  struct proc *p = ptable.list[ZOMBIE].head;
  if (!p){
      cprintf("Nothing !!!\n");
   }
   while (p){
      if(p -> next ==0)
         cprintf("(%d, %d)\n", p -> pid, p ->parent-> pid); 
      else
         cprintf("(%d, %d)->", p -> pid, p ->parent-> pid);
      p = p->next;
   }
}
#endif //P3

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;

  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");

  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid) {
      return &cpus[i];
    }
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)  //OK P3
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);
  int found = 0;
#ifdef CS333_P3
  p = ptable.list[UNUSED].head;
  if(p){
    found = 1;
  }
#else
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++) //finding first unused process.
    if(p->state == UNUSED) {
      found = 1;
      break;
    }
#endif //P3
  if (!found) {
    release(&ptable.lock);
    return 0;
  }
#ifdef CS333_P3
    stateListRemove(&ptable.list[p->state], p);
    assertState(p, UNUSED);   
    p->state = EMBRYO;
    stateListAdd(&ptable.list[p->state], p);
  //add embryo list ; remove from unused
#else
  p->state = EMBRYO;
#endif //P3
  p->pid = nextpid++;
  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
#ifdef CS333_P3
    acquire(&ptable.lock);
    stateListRemove(&ptable.list[p->state], p);
    assertState(p, EMBRYO);  
    p->state = UNUSED;
    stateListAdd(&ptable.list[p->state], p);
    release(&ptable.lock);
    return 0;
#else
    p->state = UNUSED;
    return 0;
#endif //P3
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;
#ifdef CS333_P1
  p->start_ticks = ticks;  // for ticks initialization
#endif
#ifdef CS333_P2
  p->cpu_ticks_total = 0;
  p->cpu_ticks_in = 0;
#endif
#ifdef CS333_P4
  p->priority = MAXPRIO;
  p->budget = BUDGET;
#endif

  return p;
}

// Set up first user process.
void
userinit(void)  //OK P3
{
#ifdef CS333_P4
  acquire(&ptable.lock);
  initProcessLists();
  initFreeList();
  ptable.PromoteAtTime = ticks + TICKS_TO_PROMOTE;
  release(&ptable.lock);

#elif CS333_P3
  initProcessLists();
  initFreeList();
#endif //P3

  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];
  p = allocproc();

  initproc = p;

#ifdef CS333_P2
  initproc->parent = initproc;
  initproc->uid = 0;
  initproc->gid = 0;
#endif
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);
#ifdef CS333_P4
    stateListRemove(&ptable.list[p->state], p); //??
    assertState(p, EMBRYO);  
    p->state = RUNNABLE;
    stateListAdd(&ptable.ready[MAXPRIO], p);

#elif CS333_P3
    stateListRemove(&ptable.list[p->state], p);
    assertState(p, EMBRYO);  
    p->state = RUNNABLE;
    stateListAdd(&ptable.list[p->state], p);
#else
  p->state = RUNNABLE;
#endif //P3
  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)  //OK P3
{
  int i;
  uint pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
#ifdef CS333_P3
    acquire(&ptable.lock);
    stateListRemove(&ptable.list[np->state], np);
    assertState(np, EMBRYO);  
    np->state = UNUSED;
    stateListAdd(&ptable.list[np->state], np);
    release(&ptable.lock);
#else
    np->state = UNUSED;
#endif //P3
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

#ifdef CS333_P2
  np->uid = myproc()->uid;
  np->gid = myproc()->gid;
#endif

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;
  
  acquire(&ptable.lock);
#ifdef CS333_P4
    stateListRemove(&ptable.list[np->state], np);
    assertState(np, EMBRYO);  
    np->state = RUNNABLE;
    stateListAdd(&ptable.ready[MAXPRIO], np);
  
#elif CS333_P3
    stateListRemove(&ptable.list[np->state], np);
    assertState(np, EMBRYO);  
    np->state = RUNNABLE;
    stateListAdd(&ptable.list[np->state], np);
#else
  np->state = RUNNABLE;
#endif //P3
  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
#ifdef CS333_P3
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);
#ifdef CS333_P4
  for(int x = 0; x <= MAXPRIO; x++)
  {
    p = ptable.ready[x].head;
    while(p){
      if(p->parent == curproc){
	p->parent = initproc;
      }
    p = p->next;
    }
  }
#endif //P4
  // Pass abandoned children to init.
  for(int x = EMBRYO; x <= ZOMBIE; x++) //here
  {
    p = ptable.list[x].head;
    while(p){
      if(p->parent == curproc){
	p->parent = initproc;
	if(p->state == ZOMBIE)
	  wakeup1(initproc);
      }
    p = p->next;
    }
  }
  // Jump into the scheduler, never to return.
  //****************
  stateListRemove(&ptable.list[curproc->state], curproc);
  assertState(curproc, RUNNING);  
  curproc->state = ZOMBIE;
  stateListAdd(&ptable.list[curproc->state], curproc);
  //****************
   
  sched();
  panic("zombie exit");
}
#else
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}
#endif //P3

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
#ifdef CS333_P3
int
wait(void)
{
  struct proc *p;
  int havekids;
  uint pid;
  struct proc *curproc = myproc();

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
#ifdef CS333_P4
    for(int x = 0; x <= MAXPRIO; x++)
    {
      p = ptable.ready[x].head;
      while(p){
	if(p->parent == curproc){
	  havekids = 1;
	}
	p = p->next;
      }
    }
#endif //P4
    for(int x=EMBRYO; x <= ZOMBIE; x++) //here
    {
      p = ptable.list[x].head; 
      while(p){ 

	if(p->parent == curproc){
	  havekids = 1;
	  if(p->state == ZOMBIE){
	    // Found one.
	    pid = p->pid;
	    kfree(p->kstack);
	    p->kstack = 0;
	    freevm(p->pgdir);
	    p->pid = 0;
	    p->parent = 0;
	    p->name[0] = 0;
	    p->killed = 0;

	    //***********
	    stateListRemove(&ptable.list[p->state], p);
	    assertState(p, ZOMBIE);  
	    p->state = UNUSED;
	    stateListAdd(&ptable.list[p->state], p);
	    //***************

	    release(&ptable.lock);
	    return pid;
	  }
	}
	p = p->next;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}
#else
int
wait(void)
{
  struct proc *p;
  int havekids;
  uint pid;
  struct proc *curproc = myproc();

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}
#endif //P3

// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
#ifdef CS333_P3
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
#ifdef PDX_XV6
  int idle;  // for checking if processor is idle
#endif // PDX_XV6

  for(;;){
    // Enable interrupts on this processor.
    sti();

#ifdef PDX_XV6
    idle = 1;  // assume idle unless we schedule a process
#endif // PDX_XV6
    // Loop over process table looking for process to run.
    acquire(&ptable.lock);

#ifdef CS333_P4
    if(ticks >=ptable.PromoteAtTime){
      for(int i =MAXPRIO-1; i >= 0; i--){
	p = ptable.ready[i].head;
	struct proc* hold = p;
	while(p){
	  p->budget = BUDGET;
	  hold = hold ->next;
	  stateListRemove(&ptable.ready[p->priority], p);
	  p->priority +=1;
	  stateListAdd(&ptable.ready[p->priority], p);
	  p = hold;
	}
      }

      p = ptable.list[RUNNING].head;
      while(p){
	if(p->priority == MAXPRIO){
	  //  
	}else{
	  p ->priority += 1;
	  p ->budget = BUDGET;
	}
	p = p->next;
      }

      p = ptable.list[SLEEPING].head;
      while(p){
	if(p->priority == MAXPRIO){
	  //
	}else{
	  p ->priority += 1;
	  p ->budget = BUDGET;
	}
	p = p->next;
      }

      ptable.PromoteAtTime = ticks + TICKS_TO_PROMOTE;
    }	    
#endif

//***************
#ifdef CS333_P4
  int i;
  for(i = MAXPRIO; i>=0; i--)
  {
    p = ptable.ready[i].head;
    if(p){
      break;
    }
  }
#else
    p = ptable.list[RUNNABLE].head;
#endif
      if(p){
//***************
      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
#ifdef PDX_XV6
      idle = 0;  // not idle this timeslice
#endif // PDX_XV6

      c->proc = p;
      switchuvm(p);
//***************
#ifdef CS333_P4
      stateListRemove(&ptable.ready[i], p); //MAXPRIO to p->priority
      assertState(p, RUNNABLE);  
      p->state = RUNNING;
      stateListAdd(&ptable.list[p->state], p); //?? 
#else
      stateListRemove(&ptable.list[p->state], p);
      assertState(p, RUNNABLE);  
      p->state = RUNNING;
      stateListAdd(&ptable.list[p->state], p);
#endif //P4
//****************

      swtch(&(c->scheduler), p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }
    release(&ptable.lock);
#ifdef PDX_XV6
    // if idle, wait for next interrupt
    if (idle) {
      sti();
      hlt();
    }
#endif // PDX_XV6
  }
}
#else
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
#ifdef PDX_XV6
  int idle;  // for checking if processor is idle
#endif // PDX_XV6

  for(;;){
    // Enable interrupts on this processor.
    sti();

#ifdef PDX_XV6
    idle = 1;  // assume idle unless we schedule a process
#endif // PDX_XV6
    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
#ifdef PDX_XV6
      idle = 0;  // not idle this timeslice
#endif // PDX_XV6
      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;
#ifdef CS333_P2
      p->cpu_ticks_in = ticks;
#endif
      swtch(&(c->scheduler), p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }
    release(&ptable.lock);
#ifdef PDX_XV6
    // if idle, wait for next interrupt
    if (idle) {
      sti();
      hlt();
    }
#endif // PDX_XV6
  }
}
#endif //P3

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  
#ifdef CS333_P2
  p->cpu_ticks_total += ticks - p->cpu_ticks_in;
#endif  
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
#ifdef CS333_P3
void
yield(void)
{
  struct proc *curproc = myproc();

  acquire(&ptable.lock);  //DOC: yieldlock
  //*************
#ifdef CS333_P4
  stateListRemove(&ptable.list[curproc->state], curproc);
  assertState(curproc, RUNNING);  
  curproc->state = RUNNABLE;
  
  curproc->budget = curproc->budget - (ticks - curproc->cpu_ticks_in);
  if((curproc->budget <= 0)){
    if(curproc->priority > 0 && MAXPRIO != 0){
//      curproc->priority = curproc->priority -1;
    }
    curproc->budget = BUDGET;
  }
  
  stateListAdd(&ptable.ready[curproc->priority], curproc);
#else
  stateListRemove(&ptable.list[curproc->state], curproc);
  assertState(curproc, RUNNING);  
  curproc->state = RUNNABLE;
  stateListAdd(&ptable.list[curproc->state], curproc);
#endif //P4
  //*************
  sched();
  release(&ptable.lock);
}
#else
void
yield(void)
{
  struct proc *curproc = myproc(); 

  acquire(&ptable.lock);  //DOC: yieldlock
  curproc->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}
#endif //P3

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they prioritysleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
#ifdef CS333_P3
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();

  if(p == 0)
    panic("sleep");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    if (lk) release(lk);
  }
  // Go to sleep.
  //*******************
  p->chan = chan;
  stateListRemove(&ptable.list[p->state], p);
  assertState(p, RUNNING);  
#ifdef CS333_P4
//  if(MAXPRIO){
    p->budget = p->budget - (ticks - p->cpu_ticks_in);
    if(p->budget <= 0){
      if(p->priority > 0 && MAXPRIO != 0){
//	p->priority = p->priority -1;
      }
      p->budget = BUDGET;
    }
//  }
#endif
  p->state = SLEEPING;
  stateListAdd(&ptable.list[p->state], p);
  //*******************

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    if (lk) acquire(lk);
  }
}
#else
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();

  if(p == 0)
    panic("sleep");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    if (lk) release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    if (lk) acquire(lk);
  }
}
#endif //P3

// Wake up all processes sleeping on chan.
// The ptable lock must be held.
#ifdef CS333_P3
static void
wakeup1(void *chan)
{
  struct proc *p;

  p = ptable.list[SLEEPING].head;
  //*******************
  while(p)
  {
    if(p->chan == chan)
    {
#ifdef CS333_P4
      stateListRemove(&ptable.list[p->state], p); //??
      assertState(p, SLEEPING);  
      p->state = RUNNABLE;
      stateListAdd(&ptable.ready[p->priority], p);  //all MAXPRIO change to p->priority
#else
      stateListRemove(&ptable.list[p->state], p);
      assertState(p, SLEEPING);  
      p->state = RUNNABLE;
      stateListAdd(&ptable.list[p->state], p);
#endif //P4
    }
    p = p->next;
  }
  //*******************

}
#else
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}
#endif //P3

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
#ifdef CS333_P3
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);

#ifdef CS333_P4
      for(int i = MAXPRIO; i >= 0; i--)
      {
	p = ptable.ready[i].head;
	while(p){
	  if(p->pid == pid){
	    p ->killed = 1;
	    release(&ptable.lock);
	    return 0;
	  }
	p = p->next;
	}
      }
#endif //P4
  
  for(int x = EMBRYO; x <=ZOMBIE; x++)
  { 
    p = ptable.list[x].head;
  
    while(p){
      if(p->pid == pid){
	p->killed = 1;
      // Wake process from sleep if necessary.
	if(p->state == SLEEPING){
#ifdef CS333_P4
	  stateListRemove(&ptable.list[p->state], p);
	  assertState(p, SLEEPING);  
	  p->state = RUNNABLE;
	  stateListAdd(&ptable.ready[p->priority], p);
#else
	  stateListRemove(&ptable.list[p->state], p);
	  assertState(p, SLEEPING);  
	  p->state = RUNNABLE;
	  stateListAdd(&ptable.list[p->state], p);
#endif //P4
	}
	release(&ptable.lock);
	return 0;
      }
      p = p->next;
    }
  }
  release(&ptable.lock);
  return -1;
}
#else
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}
#endif //P3

#ifdef CS333_P2
int
getuid(void)
{
  acquire(&ptable.lock);
  int m = myproc()->uid;
  release(&ptable.lock);
  return m;
}

int
getgid(void)
{
  acquire(&ptable.lock);
  int m = myproc()->gid;
  release(&ptable.lock);
  return m;
}

int
getppid(void)
{
  acquire(&ptable.lock);
  int m;
  if(myproc()->parent)
  {
    m = myproc()->parent->pid;
  }
  else
    m = myproc()->pid;
  release(&ptable.lock);
  return m;
}
int
setuid(int uid)
{
  acquire(&ptable.lock);
  myproc()->uid = uid;
  release(&ptable.lock);
  return 0;
}
int
setgid(int gid)
{
  acquire(&ptable.lock);
  myproc()->gid = gid;
  release(&ptable.lock);
  return 0;

}

int getprocs(uint max, struct uproc *u)
{ 
  struct proc *p;
  int count = 0;
  acquire(&ptable.lock);
  for(p = ptable.proc; p <&ptable.proc[NPROC] && p < &ptable.proc[max]; p++)
  {
    if(p->state == UNUSED || p->state == EMBRYO)
      continue;
    u->pid = p->pid;
    u->uid = p->uid;
    u->gid = p->gid;
    if(p->pid ==1)
      u->ppid = 1;
    else
      u->ppid = p->parent->pid;
    u->elapsed_ticks = ticks - p->start_ticks;
    u->CPU_total_ticks = p->cpu_ticks_total;
    safestrcpy(u->state, states[p->state], sizeof(u->state)/sizeof(char));
    u->size = p->sz;
    safestrcpy(u->name, p->name, sizeof(u->name)/sizeof(char));
    u++;
    count++;
  }
  release(&ptable.lock);
  return count;
}
#endif //CS333_P2

#ifdef CS333_P4
int
setpriority(int pid, int priority)
{
  struct proc * p;
    if(pid < 0)
        return -1;
    if(priority < 0 || priority > MAXPRIO)
        return -1;
    acquire(&ptable.lock);
    for(int i =0; i < MAXPRIO; ++i)
    {
      p = ptable.ready[i].head;
      while(p)
      {
	if(p-> pid ==pid)
	{	
	  stateListRemove(&ptable.ready[p->priority], p);
	  p -> budget = BUDGET;
	  p -> priority = priority;
	  stateListAdd(&ptable.ready[p->priority], p);
	  release(&ptable.lock);
	  return 0;
	}
	p = p->next;
      }
    }
    p = ptable.list[RUNNING].head;
    while(p){
      if(p->pid == pid){
	p->priority = priority;
        p->budget = BUDGET;
        release(&ptable.lock);
	return 0;
      }
      p = p->next;
    }
    p = ptable.list[SLEEPING].head;
    while(p){
      if(p->pid == pid){
	p->priority = priority;
        p->budget = BUDGET;
        release(&ptable.lock);
	return 0;
      }
      p = p->next;
    }
    release(&ptable.lock);
    return -1;
}

int
getpriority(int pid)
{
  acquire(&ptable.lock);
  struct proc* p = ptable.list[RUNNING].head;

  while(p){
    if(p->pid ==pid){
      release(&ptable.lock);
      return p->priority;
    }
    p = p->next;
  }
  for(int i = MAXPRIO; i>=0; --i){
    p = ptable.ready[i].head;
    while(p){
      if(p->pid ==pid){
	release(&ptable.lock);
	return p->priority;
      }
    p = p->next;
    }
  }
  p = ptable.list[SLEEPING].head;
    while(p){
      if(p->pid ==pid){
	release(&ptable.lock);
	return p->priority;
    }
    p = p->next;
  }
  release(&ptable.lock);
  return -1;
}
#endif
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.

#ifdef CS333_P1
void //function by caculate elapsed time CS333_P1
elapsed_caculate(struct proc *p)
{
  cprintf("\t%d.%d\t", (ticks - p->start_ticks)/1000, (ticks - p->start_ticks)%1000);
}

void //CS333_P1
procdumpP1(struct proc *p, char *state)
{
  cprintf("%d\t%s", p->pid, p->name);
  elapsed_caculate(p);
  cprintf("%s\t%d\t", state, p->sz);
}
#endif
#ifdef CS333_P2
void
cpu_caculate(struct proc *p)
{
  cprintf("%d.%d\t", (p->cpu_ticks_total)/1000, (p->cpu_ticks_total)%1000);
}
void
procdumpP2(struct proc *p, char *state)
{
  
  cprintf("%d\t%s\t%d\t%d\t%d", p->pid, p->name, p->uid, p->gid, 1); //fix PPID
  elapsed_caculate(p);
  cpu_caculate(p);
  cprintf("%s\t%d\t", state, p->sz);

}
#endif
#ifdef CS333_P4
void
procdumpP3P4(struct proc *p, char *state)
{
  
  cprintf("%d\t%s\t%d\t%d\t%d\t%d", p->pid, p->name, p->uid, p->gid, 0, p->priority); //fix PPID
  elapsed_caculate(p);
  cpu_caculate(p);
  cprintf("%s\t%d\t", state, p->sz);

}
#endif

void
procdump(void)
{
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

#if defined(CS333_P4)
#define HEADER "\nPID\tName    UID\tGID\tPPID\tPrio\tElapsed\tCPU\tState\tSize\t PCs\n"
#elif defined(CS333_P2)
#define HEADER "\nPID\tName    UID\tGID\tPPID\tElapsed\tCPU\tState\tSize\t PCs\n"
#elif defined(CS333_P1)
#define HEADER "\nPID\tName    Elapsed\tState\tSize\t PCs\n"
#else
#define HEADER "\n"
#endif

  cprintf(HEADER);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";

#if defined(CS333_P4)
    procdumpP3P4(p, state);
#elif defined(CS333_P2)
    procdumpP2(p, state);
#elif defined(CS333_P1)
    procdumpP1(p,state);
#else
    cprintf("%d\t%s\t%s\t", p->pid, p->name, state);
#endif

    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}

