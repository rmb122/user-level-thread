#include "ul_thread.c"

void test() {
    int t = 0;
    printf("Thread TID %d\n", ult_get_tid());
    for (int i = 0; i < 1000000000000; i++) {
        t++;
        if (i % 100000 == 0 && ult_get_tid() == 2) {
            printf("Thread TID %d start sleep\n", ult_get_tid());
            //ult_thread_sleep(1000);
            printf("Thread TID %d wakeup\n", ult_get_tid());
            printf("Count Test: %d\n", t);
        }
    }
    printf("Thread TID %d\n", ult_get_tid());
    printf("Count Test: %d\n", t);
}

int main() {
    ult_thread_create(&test, NULL, 1);
    ult_thread_sleep(644);
    ult_thread_create(&test, NULL, 1);
    ult_thread_sleep(322);
    ult_thread_create(&test, NULL, 100);
    int count = 0;
    ult_set_priority(100);
    for(;;) {
        printf("Test\n");
        //count++;
        //ult_thread_create(&test, NULL, 2);
        //ult_thread_sleep(10000000);
    }
}
