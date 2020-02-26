//
// Created by rmb122 on 2019/11/17.
//

#include <ucontext.h>

#ifndef ULT_UL_THREAD_H
#define ULT_UL_THREAD_H

typedef struct
{
    int count;
} ult_mutex;

typedef struct __ult_tcb
{ // tcb 定义
    struct __ult_tcb *prev;
    struct __ult_tcb *next;

    ucontext_t *context;
    void *stack_ptr;
    void *exit_stack_ptr;

    int tid;
    int status;
    int joined_tid;
    ult_mutex *mutex_blocked;

    unsigned short priority;
    unsigned short vpriority;
    long long int sleep_remain;
} ult_tcb;

enum
{
    ULT_STATUS_RUNNING = 0,
    ULT_STATUS_READY = 1,
    ULT_STATUS_SLEEP = 2,
    ULT_STATUS_DEAD = 3,
    ULT_STATUS_JOINED = 4,
    ULT_STATUS_BLOCKED = 5,
};

const unsigned short PRIORITY_MAX = 65535;

void ult_tcb_add_to_list(ult_tcb *tcb, ult_tcb *tcb_list);

ult_tcb *ult_pop_from_list(ult_tcb *tcb_list);

void ult_clean_dead_thread();

void ult_debug_print_list(ult_tcb *tcb_list, char *name);

void ult_scheduler_ticksleep();

void ult_debug_all_list();

void ult_schedule_sleep_thread();

int ult_get_tid();

unsigned short ult_get_priority();

void ult_thread_exit();

int ult_thread_create(void (*func)(), void *arg, unsigned short priority);

void ult_thread_sleep(long int millisecond);

int ult_thread_join(int tid);

void ult_mutex_acquire(ult_mutex *mutex);

void ult_mutex_release(ult_mutex *mutex);

ult_mutex *ult_new_mutex(int count);

void ult_set_priority(unsigned short priority);

void ult_enable_scheduler();

void ult_disable_scheduler();

#endif //ULT_UL_THREAD_H
