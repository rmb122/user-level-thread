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

typedef struct __ult_tcb {
    struct __ult_tcb *prev;
    struct __ult_tcb *next;

    ucontext_t *context;
    void *stack_ptr;
    void *exit_stack_ptr;
    int tid;
    int status;
    int priority;
    long long int sleep_remain;
} ult_tcb;

enum {
    ULT_STATUS_RUNNING = 0,
    ULT_STATUS_READY = 1,
    ULT_STATUS_SLEEP = 2,
    ULT_STATUS_DEAD = 3
};

static int SCHEDULER_TICK = 10000; // 大概一秒调度 100 次
static int ULT_STACK_SIZE = 10240;
static int ult_thread_count = 0;
static ucontext_t *ult_main_context = NULL;

static ult_tcb *ult_curr_tcb;

static ult_tcb *ult_ready_tcb;
static ult_tcb *ult_dead_tcb;
static ult_tcb *ult_sleep_tcb;

static ult_tcb *__ult_temp_tcb_a = NULL;
static ult_tcb *__ult_temp_tcb_b = NULL;
static ult_tcb *__ult_temp_curr_tcb = NULL;

struct timespec ult_last_sleep_schedule = {0, 0};

void ult_scheduler() {
    signal(SIGALRM, SIG_IGN);
    __ult_temp_tcb_a = NULL;
    __ult_temp_tcb_b = NULL;
    __ult_temp_curr_tcb = NULL;

    if (ult_curr_tcb->status != ULT_STATUS_SLEEP) {
        ult_schedule_sleep_thread();
    }
    switch (ult_curr_tcb->status) {
        case ULT_STATUS_RUNNING: // 在运行状态下, 至少会有一个线程在运行, 不用确认 ready 是否为空
            ult_tcb_add_to_list(ult_curr_tcb, ult_ready_tcb);
            __ult_temp_tcb_a = ult_pop_from_list(ult_ready_tcb);
            __ult_temp_tcb_b = ult_curr_tcb;
            if (__ult_temp_tcb_a != __ult_temp_tcb_b) {
                __ult_temp_tcb_b->status = ULT_STATUS_READY;
                ult_curr_tcb = __ult_temp_tcb_a;
                ult_curr_tcb->status = ULT_STATUS_RUNNING;
                signal(SIGALRM, ult_scheduler);
                swapcontext(__ult_temp_tcb_b->context, ult_curr_tcb->context);
            }
            signal(SIGALRM, ult_scheduler);
            break;
        case ULT_STATUS_DEAD:
            ult_clean_dead_thread();
            ult_tcb_add_to_list(ult_curr_tcb, ult_dead_tcb);

            __ult_temp_curr_tcb = ult_pop_from_list(ult_ready_tcb);
            while (__ult_temp_curr_tcb == NULL) {
                ult_scheduler_ticksleep();
                ult_schedule_sleep_thread();
                __ult_temp_curr_tcb = ult_pop_from_list(ult_ready_tcb);
            }
            ult_curr_tcb = __ult_temp_curr_tcb;
            ult_curr_tcb->status = ULT_STATUS_RUNNING;
            signal(SIGALRM, ult_scheduler);
            setcontext(ult_curr_tcb->context);
            break;
        case ULT_STATUS_SLEEP: // 进入这里的分支, 还没保存线程的 context, 所以不能先添加到
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
            signal(SIGALRM, ult_scheduler);
            swapcontext(__ult_temp_tcb_a->context, ult_curr_tcb->context);
            break;
    }
}

void ult_scheduler_ticksleep() {
    struct timespec ts = {
            0,
            SCHEDULER_TICK * 1000 // microsecond to nanosecond
    };
    while (nanosleep(&ts, &ts) == -1) {
    }
}

void ult_schedule_sleep_thread() {
    struct timespec curr_time = {0, 0};
    clock_gettime(CLOCK_REALTIME, &curr_time); // 睡眠按照实际时间来
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
    signal(SIGALRM, ult_scheduler);
    struct itimerval value;
    value.it_value.tv_sec = 0;
    value.it_value.tv_usec = 1;
    value.it_interval.tv_sec = 0;
    value.it_interval.tv_usec = SCHEDULER_TICK;
    setitimer(ITIMER_REAL, &value, NULL);
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
    signal(SIGALRM, SIG_IGN);
    ult_curr_tcb->status = ULT_STATUS_DEAD;
    ult_scheduler();
}

void ult_tcb_add_to_list(ult_tcb *tcb, ult_tcb *tcb_list) {
    while (tcb_list->next != NULL) {
        tcb_list = tcb_list->next;
    }
    tcb->prev = tcb_list;
    tcb->next = NULL;
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
    printf("tid: %d | name: %s", ult_gettid(), name);
    if (tcb_list->tid != -1 || tcb_list->status != 0) {
        printf("[FATAL] tcb_list->tid != -1 || tcb_list->status != 0");
    }
    tcb_list = tcb_list->next;
    while (tcb_list != NULL) {
        printf(" | tid: %d, status: %d", tcb_list->tid, tcb_list->status);
        tcb_list = tcb_list->next;
    }
    printf("\n");
}

void ult_debug_all_list() {
    ult_debug_print_list(ult_sleep_tcb, "sleep");
    ult_debug_print_list(ult_ready_tcb, "ready");
    ult_debug_print_list(ult_dead_tcb, " dead");
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
        ult_curr_tcb->tid = ult_thread_count;
        ult_thread_count++;

        ult_ready_tcb = malloc(sizeof(ult_tcb));
        ult_ready_tcb->tid = -1;
        ult_dead_tcb = malloc(sizeof(ult_tcb));
        ult_dead_tcb->tid = -1;
        ult_sleep_tcb = malloc(sizeof(ult_tcb));
        ult_sleep_tcb->tid = -1;
        clock_gettime(CLOCK_REALTIME, &ult_last_sleep_schedule); // 定时信号, 按实际时间来
        ult_settimer();
    }
}

void ult_thread_create(void (*func)(), void *arg) {
    signal(SIGALRM, SIG_IGN);

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
    ult_thread_count++;

    ult_tcb_add_to_list(tcb, ult_ready_tcb);
    signal(SIGALRM, ult_scheduler);
}

void ult_thread_sleep(long int milltime) {
    signal(SIGALRM, SIG_IGN);
    ult_curr_tcb->status = ULT_STATUS_SLEEP;
    ult_curr_tcb->sleep_remain = ((long long int) milltime) * 1000000;
    ult_scheduler();
}

int ult_gettid() {
    return ult_curr_tcb->tid;
}
