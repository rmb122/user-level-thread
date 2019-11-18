#include "ul_thread.c"

void test() {
    int t = 0;
    printf("Thread TID %d\n", ult_gettid());
    for (int i = 0; i < 1000000; i++) {
        t++;
        if (i % 100000 == 0) {
            printf("Thread TID %d start sleep\n", ult_gettid());
            ult_thread_sleep(1000);
            printf("Thread TID %d wakeup\n", ult_gettid());
            printf("Count Test: %d\n", t);
        }
    }
    printf("Thread TID %d\n", ult_gettid());
    printf("Count Test: %d\n", t);
}

int main() {
    ult_thread_create(&test, NULL);
    ult_thread_create(&test, NULL);
    ult_thread_create(&test, NULL);
    int count = 0;
    for(;;) {
        printf("Test\n");
        count++;
        ult_thread_create(&test, NULL);
        ult_thread_sleep(1000);
    }
}
