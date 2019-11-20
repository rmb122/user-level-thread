#include "ul_thread.c"

void test() {
    int t = 0;
    printf("Thread TID %d\n", ult_get_tid());
    for (int i = 0; i < 1000000; i++) {
        t++;
        if (i % 100000 == 0) {
            printf("Thread TID %d start sleep\n", ult_get_tid());
            ult_thread_sleep(1000);
            printf("Thread TID %d wakeup\n", ult_get_tid());
            printf("Count Test: %d\n", t);
        }
    }
    printf("status: %d\n", ult_thread_join(1));
    printf("Thread TID %d\n", ult_get_tid());
    printf("Count Test: %d\n", t);
}

int main() {
    int tid = ult_thread_create(&test, NULL, 2);
    //ult_thread_exit();
    ult_thread_sleep(644);
    ult_thread_create(&test, NULL, 1);
    ult_thread_sleep(322);
    ult_thread_create(&test, NULL, 3);
    int count = 0;
    ult_set_priority(3);
    printf("tid: %d\n", tid);
    ult_thread_join(2);
    printf("Target thread complete!\n");
    ult_thread_join(3);
    printf("Target thread complete!\n");

    for(;;) {
        printf("Test\n");
        count++;
        ult_thread_create(&test, NULL, 2);
        ult_thread_sleep(20000);
        return 0;
    }
}
