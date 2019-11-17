#include <stdio.h>
#include <ucontext.h>
#include <signal.h>
#include <sys/time.h>
#include <malloc.h>
#include <memory.h>

void add_to_waiting(ucontext_t *context, int tid);
struct linked_ctx* pop_from_waiting();

struct linked_ctx {
    struct linked_ctx *prev;
    ucontext_t *context;
    int tid;
    int status;
    struct linked_ctx *next;
};

int STATUS_WAITING = 0;
int STATUS_RUNNING = 1;
int STATUS_DEAD = 2;

ucontext_t main_context;
ucontext_t *curr_running_ctx = &main_context;
int curr_status = 0;

struct linked_ctx *waiting_thread = NULL;

int curr_tid = 0;
int stack_size = 10240;

void handler() {
    signal(SIGALRM, SIG_IGN);

    if (curr_status != STATUS_DEAD) {
        ucontext_t *context = curr_running_ctx;
        add_to_waiting(context, curr_tid);
        struct linked_ctx* running_ctx = pop_from_waiting();
        curr_tid = running_ctx->tid;
        curr_running_ctx = running_ctx->context;
        curr_status = running_ctx->status;
        swapcontext(context, curr_running_ctx);
    } else {
        struct linked_ctx* running_ctx = pop_from_waiting();
        curr_tid = running_ctx->tid;
        curr_running_ctx = running_ctx->context;
        curr_status = running_ctx->status;
        setcontext(curr_running_ctx);
    }
    signal(SIGALRM, handler);
}

void thread_exit() {
    curr_status = STATUS_DEAD;
    handler();
}

void add_to_waiting(ucontext_t *context, int tid) {
    if (waiting_thread == NULL) {
        waiting_thread = malloc(sizeof(struct linked_ctx));
        waiting_thread->next = NULL;
        waiting_thread->prev = NULL;
        waiting_thread->context = context;
        waiting_thread->tid = tid;
        waiting_thread->status = STATUS_WAITING;
    }
}

struct linked_ctx* pop_from_waiting() {
    struct linked_ctx* ret = NULL;
    if (waiting_thread == NULL) {
        printf("[FATAL ERROR] waiting_thread == NULL\n");
    } else {
        ret = waiting_thread;
        waiting_thread = waiting_thread->next;
    }
    return ret;
}

void thread_init(int tid, void (*func)(void)) {
    char *stack_bottom = malloc(stack_size);
    memset(stack_bottom, 0, stack_size);

    ucontext_t *context = malloc(sizeof(ucontext_t));
    getcontext(context);

    context->uc_stack.ss_sp = stack_bottom;
    context->uc_stack.ss_size = stack_size;

    thread_exit_ctx.uc_stack.ss_sp = stack_bottom;
    thread_exit_ctx.uc_stack.ss_size = stack_size;
    //context->uc_link = &thread_exit_ctx;

    makecontext(context, func, 0);

    setcontext(context);
    add_to_waiting(context, tid);
}

void set_timer() {
    struct itimerval value;
    signal(SIGALRM, handler);
    value.it_value.tv_sec = 0;
    value.it_value.tv_usec = 10000;
    value.it_interval.tv_sec = 0;
    value.it_interval.tv_usec = 10000;
    setitimer(ITIMER_REAL, &value, NULL);
}

void test(void) {
    printf("Test\n");
}

int main() {
    set_timer();

    thread_init(1, &test);
    while (1) {
        printf("test2\n");
    }
    return 0;
}