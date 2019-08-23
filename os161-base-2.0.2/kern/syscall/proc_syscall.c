#include <types.h>
#include <syscall.h>
#include <lib.h>
#include <proc.h>
#include <kern/errno.h>
#include <mips/trapframe.h>
#include <thread.h>
#include <addrspace.h>
#include <current.h>
#include <kern/unistd.h>
#include "opt-waitpid.h"
#include "opt-fork.h"


void sys__exit(int status) {
    #if OPT_WAITPID
    struct proc *p = curproc;
    p->p_status = status & 0xff; /* just lower 8 bits returned */
    proc_remthread(curthread);
    lock_acquire(p->p_lockl);
    cv_signal(p->p_cv, p->p_lockl);
    lock_release(p->p_lockl);
    #else
    struct addrspace *as = proc_getas();
    as_destroy(as);
    #endif
    thread_exit();

    panic("thread_exit returned (should not happen)\n");
    (void) status; // TODO: handle status
}

pid_t sys_waitpid(pid_t pid, userptr_t status, int options){
    //TODO: Implement
#if OPT_WAITPID
    struct proc *p = proc_search_pid(pid);
    int s;
    (void)options; /* not handled */
    if (p==NULL) return -1;
    s = proc_wait(p);
    if (status!=NULL) 
    *(int*)status = s;
        return pid;
    #else
    (void)options; /* not handled */
    (void)pid;
    (void)status;
    return -1;
#endif
}


pid_t sys_getpid(void) {
#if OPT_WAITPID
  KASSERT(curproc != NULL);
  return curproc->p_pid;
#else
  return -1;
#endif
}

#if OPT_FORK
static void
call_enter_forked_process(void *tfv, unsigned long dummy) {
  struct trapframe *tf = (struct trapframe *)tfv;
  (void)dummy;
  enter_forked_process(tf); 
 
  panic("enter_forked_process returned (should not happen)\n");
}

int sys_fork(struct trapframe *ctf, pid_t *retval) {
  struct trapframe *tf_child;
  struct proc *newp;
  int result;

  KASSERT(curproc != NULL);

  newp = proc_create_runprogram(curproc->p_name);
  if (newp == NULL) {
    return ENOMEM;
  }

  /* done here as we need to duplicate the address space 
     of thbe current process */
  as_copy(curproc->p_addrspace, &(newp->p_addrspace));
  if(newp->p_addrspace == NULL){
    proc_destroy(newp); 
    return ENOMEM; 
  }

  proc_file_table_copy(newp,curproc);

  /* we need a copy of the parent's trapframe */
  tf_child = kmalloc(sizeof(struct trapframe));
  if(tf_child == NULL){
    proc_destroy(newp);
    return ENOMEM; 
  }
  memcpy(tf_child, ctf, sizeof(struct trapframe));

  /* TO BE DONE: linking parent/child, so that child terminated 
     on parent exit */

  result = thread_fork(
		 curthread->t_name, newp,
		 call_enter_forked_process, 
		 (void *)tf_child, (unsigned long)0/*unused*/);

  if (result){
    proc_destroy(newp);
    kfree(tf_child);
    return ENOMEM;
  }

  *retval = newp->p_pid;

  return 0;
}
#endif