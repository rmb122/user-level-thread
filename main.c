#include "ul_thread.h"
#include "stdio.h"
#include <unistd.h>

void test()
{
    int t = 0;
    printf("Thread TID %d\n", ult_get_tid());
    for (int i = 0; i < 1000000; i++)
    {
        t++;
        if (i % 100000 == 0)
        {
            printf("Thread TID %d start sleep\n", ult_get_tid());
            ult_thread_sleep(1000);
            printf("Thread TID %d wakeup\n", ult_get_tid());
            printf("Count Test: %d\n", t);
        }
    }
    //printf("status: %d\n", ult_thread_join(1));
    printf("Thread TID %d\n", ult_get_tid());
    printf("Count Test: %d\n", t);
}

/*
 * void test_2()
{
    int t = 0;
    printf("Thread TID %d\n", ult_get_tid());
    for (int i = 0; i < 1000000; i++)
    {
        t++;
        if (i % 100000 == 0)
        {
            printf("Thread TID %d start sleep\n", ult_get_tid());
            ult_thread_sleep(1000);
            printf("Thread TID %d wakeup\n", ult_get_tid());
            printf("Count Test: %d\n", t);
        }
    }
    //printf("status: %d\n", ult_thread_join(1));
    printf("Thread TID %d\n", ult_get_tid());
    printf("Count Test: %d\n", t);
}
 */

void sleep_test()
{
    int tid = ult_thread_create(&test, NULL, 2);
    //ult_thread_exit();
    ult_thread_create(&test, NULL, 1);
    ult_thread_create(&test, NULL, 3);
    int count = 0;
    ult_set_priority(3);
    printf("tid: %d\n", tid);
    ult_thread_join(2);
    printf("Target thread complete!\n");
    ult_thread_join(3);
    printf("Target thread complete!\n");

    for (;;)
    {
        //printf("Test\n");
        //count++;
        ult_thread_create(&test, NULL, 2);
        ult_thread_sleep(2000);
    }
}


void producer(ult_mutex *mutex)
{
    printf("mutex addr: %p\n", mutex);
    while (1)
    {
        //signal(SIGVTALRM, SIG_IGN);
        //ult_debug_all_list();
        //signal(SIGVTALRM, ult_scheduler);
        ult_mutex_release(mutex);
        //signal(SIGVTALRM, SIG_IGN);
        //ult_debug_all_list();
        //signal(SIGVTALRM, ult_scheduler);
        printf("produce: count: %d\n", mutex->count);
        ult_thread_sleep(1000);
    }
}

void consumer(ult_mutex *mutex)
{
    printf("mutex addr: %p\n", mutex);
    while (1)
    {
        ult_mutex_acquire(mutex);
        printf("thread %d acquired: count: %d\n", ult_get_tid(), mutex->count);
    }
}

void mutex_test()
{
    ult_mutex *mutex = ult_new_mutex(0);
    ult_thread_create(&consumer, mutex, 1);
    ult_thread_create(&consumer, mutex, 1);
    int tid = ult_thread_create(&producer, mutex, 1);

    ult_mutex *mutex2 = ult_new_mutex(0);
    ult_thread_create(&consumer, mutex2, 1);
    ult_thread_create(&consumer, mutex2, 1);
    ult_thread_create(&consumer, mutex2, 1);
    int tid2 = ult_thread_create(&producer, mutex2, 1);
    ult_thread_join(tid);
}


void printer()
{
    int t = 0;
    while (1)
    {
        int count = 0;
        printf("I am thread %d\n", ult_get_tid());
        for (int i = 0; i < 1000000; i++)
        {
            count++;
        }
        t++;
        if (t == 100)
        {
            ult_set_priority(PRIORITY_MAX);
        }
    }
}

void priority_test()
{
    ult_thread_create(&printer, NULL, 1);
    ult_thread_create(&printer, NULL, 2);
    ult_thread_create(&printer, NULL, 3);
    ult_thread_join(2);
}


void sleeper()
{
    printf("I am thread %d\n", ult_get_tid());
    if (ult_get_tid() == 4)
    {
        printf("Now disable it\n");
        ult_disable_scheduler();
        for (int i = 0; i < 10000000; i++)
        {
            if (i % 10000 == 0)
            {
                printf("I am thread %d\n", ult_get_tid());
            }
        }
        ult_thread_join(2);
        ult_thread_join(3);

        ult_enable_scheduler();
        ult_thread_create(&sleeper, NULL, 1);
        ult_thread_create(&sleeper, NULL, 1);
        printf("Exit...\n");
    } else
    {
        for (int i = 0; i < 10000000; i++)
        {
            if (i % 10000 == 0)
            {
                printf("I am thread %d\n", ult_get_tid());
            }
        }
        printf("Thread %d done\n", ult_get_tid());
    }
}


void scheduler_disable_test()
{
    ult_thread_create(&sleeper, NULL, 1);
    ult_thread_create(&sleeper, NULL, 1);
    ult_thread_create(&sleeper, NULL, 1);
    ult_thread_join(2);
    ult_thread_join(3);
    ult_thread_join(4);
    ult_thread_join(5);
    ult_thread_join(6);
}

int main()
{
    scheduler_disable_test();
    return 0;
}
