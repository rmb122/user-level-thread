//
// Created by rmb122 on 2019/11/17.
//

#include <stdio.h>
#include <ucontext.h>
#include <signal.h>
#include <sys/time.h>
#include <malloc.h>
#include <memory.h>
#include <time.h>

#include "ul_thread.h"

#define DEBUG 1

#ifdef DEBUG
#define DEBUG_PRINT(fmt, args...) fprintf(stderr, "DEBUG: %s:%d:%s(): " fmt, __FILE__, __LINE__, __func__, ##args)
#else
#define DEBUG_PRINT(...)
#endif

typedef struct {
    int count;
} ult_mutex;

typedef struct __ult_tcb { // tcb 定义
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

enum {
    ULT_STATUS_RUNNING = 0,
    ULT_STATUS_READY = 1,
    ULT_STATUS_SLEEP = 2,
    ULT_STATUS_DEAD = 3,
    ULT_STATUS_JOINED = 4,
    ULT_STATUS_BLOCKED = 5,
};

static int SCHEDULER_TICK = 10000; // 大概一秒调度 100 次
static int ULT_STACK_SIZE = 10240;
static int ult_thread_count = 1;
static ucontext_t *ult_main_context = NULL;

static ult_tcb *ult_curr_tcb;

static ult_tcb *ult_ready_tcb;
static ult_tcb *ult_dead_tcb;
static ult_tcb *ult_sleep_tcb;
static ult_tcb *ult_joined_tcb;
static ult_tcb *ult_blocked_tcb;

static ult_tcb *__ult_temp_tcb_a = NULL;
static ult_tcb *__ult_temp_tcb_b = NULL;
static ult_tcb *__ult_temp_curr_tcb = NULL;

struct timespec ult_last_sleep_schedule = {0, 0};

void ult_scheduler() {
    signal(SIGVTALRM, SIG_IGN);
    __ult_temp_tcb_a = NULL;
    __ult_temp_tcb_b = NULL;
    __ult_temp_curr_tcb = NULL;

    if (ult_curr_tcb->status != ULT_STATUS_SLEEP) {
        ult_schedule_sleep_thread();
    }
    switch (ult_curr_tcb->status) {
        case ULT_STATUS_READY:
        case ULT_STATUS_RUNNING: // 在运行状态下, 至少会有一个线程在运行, 不用确认 ready 是否为空
            if (ult_curr_tcb->status == ULT_STATUS_RUNNING) {
                ult_curr_tcb->vpriority -= 1;
                if (ult_curr_tcb->vpriority == 0) {
                    ult_curr_tcb->vpriority = ult_curr_tcb->priority;
                }
            }

            ult_curr_tcb->status = ULT_STATUS_READY;
            ult_tcb_add_to_list(ult_curr_tcb, ult_ready_tcb);
            __ult_temp_tcb_a = ult_pop_from_list(ult_ready_tcb);
            __ult_temp_tcb_a ->status = ULT_STATUS_RUNNING;

            if (__ult_temp_tcb_a != ult_curr_tcb) {
                __ult_temp_tcb_b = ult_curr_tcb;
                ult_curr_tcb = __ult_temp_tcb_a;
                signal(SIGVTALRM, ult_scheduler);
                swapcontext(__ult_temp_tcb_b->context, ult_curr_tcb->context);
            }
            signal(SIGVTALRM, ult_scheduler);
            break;

        case ULT_STATUS_DEAD:
            ult_clean_dead_thread();
            ult_tcb_add_to_list(ult_curr_tcb, ult_dead_tcb);

            __ult_temp_tcb_a = ult_joined_tcb->next;
            while (__ult_temp_tcb_a != NULL) {
                __ult_temp_tcb_b = __ult_temp_tcb_a->next;
                if (__ult_temp_tcb_a->joined_tid == ult_curr_tcb->tid) {
                    __ult_temp_tcb_a->status = ULT_STATUS_READY;
                    __ult_temp_tcb_a->joined_tid = -1;
                    __ult_temp_tcb_a->prev->next = __ult_temp_tcb_a->next;
                    if (__ult_temp_tcb_a->next != NULL) {
                        __ult_temp_tcb_a->next->prev = __ult_temp_tcb_a->prev;
                    }
                    ult_tcb_add_to_list(__ult_temp_tcb_a, ult_ready_tcb);
                }
                __ult_temp_tcb_a = __ult_temp_tcb_b;
            }

            __ult_temp_curr_tcb = ult_pop_from_list(ult_ready_tcb);
            while (__ult_temp_curr_tcb == NULL) {
                ult_scheduler_ticksleep();
                ult_schedule_sleep_thread();
                __ult_temp_curr_tcb = ult_pop_from_list(ult_ready_tcb);
            }
            ult_curr_tcb = __ult_temp_curr_tcb;
            ult_curr_tcb->status = ULT_STATUS_RUNNING;
            signal(SIGVTALRM, ult_scheduler);
            setcontext(ult_curr_tcb->context);
            break;

        case ULT_STATUS_SLEEP: // 进入这里的分支一定是 ULT_STATUS_SLEEP, 所以不用额外再设置一遍
            __ult_temp_tcb_a = ult_curr_tcb;
            ult_tcb_add_to_list(ult_curr_tcb, ult_sleep_tcb);
            __ult_temp_curr_tcb = ult_pop_from_list(ult_ready_tcb);

            while (__ult_temp_curr_tcb == NULL) {
                ult_scheduler_ticksleep();
                ult_schedule_sleep_thread();
                __ult_temp_curr_tcb = ult_pop_from_list(ult_ready_tcb);
            }
            ult_curr_tcb = __ult_temp_curr_tcb;
            ult_curr_tcb->status = ULT_STATUS_RUNNING;
            signal(SIGVTALRM, ult_scheduler);
            swapcontext(__ult_temp_tcb_a->context, ult_curr_tcb->context);
            break;

        case ULT_STATUS_JOINED:
            __ult_temp_tcb_a = ult_curr_tcb;
            ult_tcb_add_to_list(ult_curr_tcb, ult_joined_tcb);
            __ult_temp_curr_tcb = ult_pop_from_list(ult_ready_tcb);

            while (__ult_temp_curr_tcb == NULL) {
                ult_scheduler_ticksleep();
                ult_schedule_sleep_thread();
                __ult_temp_curr_tcb = ult_pop_from_list(ult_ready_tcb);
            }

            ult_curr_tcb = __ult_temp_curr_tcb;
            ult_curr_tcb->status = ULT_STATUS_RUNNING;
            signal(SIGVTALRM, ult_scheduler);
            swapcontext(__ult_temp_tcb_a->context, ult_curr_tcb->context);
            break;

        case ULT_STATUS_BLOCKED:
            __ult_temp_tcb_a = ult_curr_tcb;
            ult_tcb_add_to_list(ult_curr_tcb, ult_blocked_tcb);
            __ult_temp_curr_tcb = ult_pop_from_list(ult_ready_tcb);

            while (__ult_temp_curr_tcb == NULL) {
                ult_scheduler_ticksleep();
                ult_schedule_sleep_thread();
                __ult_temp_curr_tcb = ult_pop_from_list(ult_ready_tcb);
            }

            ult_curr_tcb = __ult_temp_curr_tcb;
            ult_curr_tcb->status = ULT_STATUS_RUNNING;
            signal(SIGVTALRM, ult_scheduler);
            swapcontext(__ult_temp_tcb_a->context, ult_curr_tcb->context);
            break;

        default:
            printf("[FATAL] Unexpected thread status %d\n", ult_curr_tcb->status);
    }
}

void ult_scheduler_ticksleep() {
    struct timespec ts = {
            0,
            SCHEDULER_TICK * 1000 // 转换 microsecond to nanosecond
    };
    while (nanosleep(&ts, &ts) == -1) {
    }
}

void ult_schedule_sleep_thread() {
    struct timespec curr_time = {0, 0};
    clock_gettime(CLOCK_REALTIME, &curr_time); // 睡眠按照实际时间来, 中断按照进程运行时间来
    long long int time_diff = ((long long int)curr_time.tv_sec * 1000000000 + (long long int)curr_time.tv_nsec) -
            ((long long int)ult_last_sleep_schedule.tv_sec * 1000000000 + (long long int)ult_last_sleep_schedule.tv_nsec);
    ult_tcb *curr = ult_sleep_tcb->next;
    ult_tcb *tmp = NULL;

    while (curr != NULL) {
        curr->sleep_remain -= time_diff;
        tmp = curr->next;
        if (curr->sleep_remain <= 0) {
            curr->sleep_remain = 0;
            curr->prev->next = curr->next;
            if (curr->next != NULL) {
                curr->next->prev = curr->prev;
            }
            curr->status = ULT_STATUS_READY;
            ult_tcb_add_to_list(curr, ult_ready_tcb);
        }
        curr = tmp;
    }
    ult_last_sleep_schedule = curr_time;
}

void ult_settimer() {
    signal(SIGVTALRM, ult_scheduler);
    struct itimerval value;
    value.it_value.tv_sec = 0;
    value.it_value.tv_usec = 1;
    value.it_interval.tv_sec = 0;
    value.it_interval.tv_usec = SCHEDULER_TICK;
    setitimer(ITIMER_VIRTUAL, &value, NULL);
}

ucontext_t* ult_make_context(void (*func)(), ucontext_t *link_context, void *arg) {
    ucontext_t *context = malloc(sizeof(ucontext_t));
    void *stack = malloc(ULT_STACK_SIZE);
    getcontext(context);

    context->uc_stack.ss_sp = stack;
    context->uc_stack.ss_size = ULT_STACK_SIZE;
    context->uc_link = link_context;

    if (arg == NULL) {
        makecontext(context, func, 0);
    } else {
        makecontext(context, func, 1, arg);
    }
    return context;
}

void ult_thread_exit() {
    signal(SIGVTALRM, SIG_IGN);
    ult_curr_tcb->status = ULT_STATUS_DEAD;
    ult_scheduler();
}

void ult_tcb_add_to_list(ult_tcb *tcb, ult_tcb *tcb_list) {
    while (tcb_list->next != NULL && tcb_list->next->vpriority >= tcb->vpriority) { // 在一个优先度比自己低的线程前面插入, 同时在这个优先度里面是最后一个
        tcb_list = tcb_list->next;
    }
    tcb->prev = tcb_list;
    tcb->next = tcb_list->next;
    if (tcb_list->next != NULL) {
        tcb_list->next->prev = tcb;
    }
    tcb_list->next = tcb;
}

ult_tcb *ult_pop_from_list(ult_tcb *tcb_list) {
    ult_tcb *ret = tcb_list->next;
    if (ret != NULL) {
        tcb_list->next = tcb_list->next->next;

        if (tcb_list->next != NULL) {
            tcb_list->next->prev = tcb_list;
        }
        ret->prev = NULL;
        ret->next = NULL;
    }
    return ret;
}

void ult_debug_print_list(ult_tcb *tcb_list, char *name) {
    printf("tid: %d | name: %s", ult_get_tid(), name);
    if (tcb_list->tid != -1 || tcb_list->status != 0) {
        printf("[FATAL] tcb_list->tid != -1 || tcb_list->status != 0");
    }
    tcb_list = tcb_list->next;
    while (tcb_list != NULL) {
        printf(" | tid: %d, status: %d, priority: %d, vpriority: %d, joined %d,",
                tcb_list->tid, tcb_list->status, tcb_list->priority, tcb_list->vpriority, tcb_list->joined_tid);
        tcb_list = tcb_list->next;
    }
    printf("\n");
}

void ult_debug_all_list() {
    ult_debug_print_list(ult_sleep_tcb, " sleep ");
    ult_debug_print_list(ult_ready_tcb, " ready ");
    ult_debug_print_list(ult_dead_tcb, "  dead ");
    ult_debug_print_list(ult_joined_tcb, " joined");
    ult_debug_print_list(ult_blocked_tcb, "blocked");
    printf("=====================\n");
}

void ult_clean_dead_thread() {
    ult_tcb *curr = ult_dead_tcb->next;
    ult_tcb *next;

    while (curr != NULL) {
        next = curr->next;
        free(curr->exit_stack_ptr);
        free(curr->context->uc_link);
        free(curr->stack_ptr);
        free(curr->context);
        free(curr);
        curr = next;
    }
    ult_dead_tcb->next = NULL;
}

void ult_init_main_context() {
    if (ult_main_context == NULL) {
        ult_main_context = malloc(sizeof(ucontext_t));
        getcontext(ult_main_context);
        ult_curr_tcb = malloc(sizeof(ult_tcb));
        ult_curr_tcb->context = ult_main_context;
        ult_curr_tcb->status = ULT_STATUS_RUNNING;
        ult_curr_tcb->next = NULL;
        ult_curr_tcb->prev = NULL;
        ult_curr_tcb->mutex_blocked = NULL;
        ult_curr_tcb->tid = ult_thread_count;
        ult_curr_tcb->priority = 1;
        ult_curr_tcb->vpriority = 1;
        ult_curr_tcb->joined_tid = -1;
        ult_thread_count++;

        ult_ready_tcb = malloc(sizeof(ult_tcb));
        ult_ready_tcb->tid = -1;
        ult_dead_tcb = malloc(sizeof(ult_tcb));
        ult_dead_tcb->tid = -1;
        ult_sleep_tcb = malloc(sizeof(ult_tcb));
        ult_sleep_tcb->tid = -1;
        ult_joined_tcb = malloc(sizeof(ult_tcb));
        ult_joined_tcb->tid = -1;
        ult_blocked_tcb = malloc(sizeof(ult_tcb));
        ult_blocked_tcb->tid = -1;

        clock_gettime(CLOCK_REALTIME, &ult_last_sleep_schedule); // 定时信号, 按实际时间来
        ult_settimer();
    }
}

int ult_thread_create(void (*func)(), void *arg, unsigned short priority) {
    signal(SIGVTALRM, SIG_IGN);

    ult_init_main_context();
    ucontext_t *exit_ucontext = ult_make_context(&ult_thread_exit, NULL, NULL);
    ucontext_t *ucontext = ult_make_context(func, exit_ucontext, arg);
    ult_tcb *tcb = malloc(sizeof(ult_tcb));
    tcb->stack_ptr = ucontext->uc_stack.ss_sp;
    tcb->exit_stack_ptr = exit_ucontext->uc_stack.ss_sp;
    tcb->status = ULT_STATUS_READY;
    tcb->tid = ult_thread_count;
    tcb->context = ucontext;
    tcb->sleep_remain = 0;
    tcb->priority = priority;
    tcb->vpriority = priority;
    tcb->joined_tid = -1;
    tcb->mutex_blocked = NULL;
    ult_thread_count++;

    ult_tcb_add_to_list(tcb, ult_ready_tcb);
    ult_scheduler();
    return tcb->tid;
}

void ult_thread_sleep(long int millisecond) {
    signal(SIGVTALRM, SIG_IGN);
    ult_curr_tcb->status = ULT_STATUS_SLEEP;
    ult_curr_tcb->sleep_remain = ((long long int) millisecond) * 1000000;
    ult_scheduler();
}

ult_tcb* ult_find_tcb_in_list(ult_tcb *tcb_list, int tid) {
    while (tcb_list->next != NULL) {
        if (tcb_list->next->tid == tid) {
            return tcb_list->next;
        }
        tcb_list = tcb_list->next;
    }
    return NULL;
}

ult_tcb* ult_thread_find(int tid) {
    ult_tcb *target;
    target = ult_find_tcb_in_list(ult_ready_tcb, tid);
    if (target != NULL) {
        return target;
    }

    target = ult_find_tcb_in_list(ult_sleep_tcb, tid);
    if (target != NULL) {
        return target;
    }

    target = ult_find_tcb_in_list(ult_joined_tcb, tid);
    if (target != NULL) {
        return target;
    }

    target = ult_find_tcb_in_list(ult_blocked_tcb, tid);
    if (target != NULL) {
        return target;
    }
    return NULL;
}

int ult_thread_join(int tid) {
    signal(SIGVTALRM, SIG_IGN);
    ult_tcb *tmp;
    ult_tcb *target;

    if (tid == ult_curr_tcb->tid) {
        signal(SIGVTALRM, ult_scheduler);
        return 0; // 不能 join 自己
    }

    target = ult_thread_find(tid);

    if (target == NULL) { // 不能 join 一个不存在的线程
        signal(SIGVTALRM, ult_scheduler);
        return 0;
    }

    tmp = target;
    while (tmp != NULL) {
        if (tmp->joined_tid == ult_curr_tcb->tid) {
            signal(SIGVTALRM, ult_scheduler);
            return 0;
        }
        tmp = ult_thread_find(tmp->joined_tid); // 不能循环 join, 否则会死锁
    }

    ult_curr_tcb->joined_tid = tid;
    ult_curr_tcb->status = ULT_STATUS_JOINED;
    ult_scheduler();
    return 1;
}

ult_mutex *ult_new_mutex(int count) {
    ult_mutex *mutex = malloc(sizeof(ult_mutex));
    mutex->count = count;
    return mutex;
}

void ult_mutex_acquire(ult_mutex *mutex) {
    signal(SIGVTALRM, SIG_IGN);
    mutex->count -= 1;
    if (mutex->count < 0) {
        ult_curr_tcb->mutex_blocked = mutex;
        ult_curr_tcb->status = ULT_STATUS_BLOCKED;
        ult_scheduler();
    } else {
        signal(SIGVTALRM, ult_scheduler);
    }
}

void ult_mutex_release(ult_mutex *mutex) {
    signal(SIGVTALRM, SIG_IGN);
    mutex->count += 1;
    if (mutex->count <= 0) {
        ult_tcb *curr = ult_blocked_tcb;
        ult_tcb *temp;

        while (curr->next != NULL) {
            if (curr->next->mutex_blocked == mutex) {
                temp = curr->next;
                temp->status = ULT_STATUS_READY;
                temp->mutex_blocked = NULL;

                temp->prev->next = temp->next;
                if (temp->next != NULL) {
                    temp->next->prev = temp->prev;
                }

                ult_tcb_add_to_list(temp, ult_ready_tcb);
                signal(SIGVTALRM, ult_scheduler);
                return;
            }
            curr = curr->next;
        }
        printf("[FATAL ERROR] Can't found a thread that acquire this lock\n");
    }
    signal(SIGVTALRM, ult_scheduler);
}

unsigned short ult_get_priority() {
    return ult_curr_tcb->priority;
}

void ult_set_priority(unsigned short priority) {
    ult_curr_tcb->priority = priority;
}

int ult_get_tid() {
    return ult_curr_tcb->tid;
}
