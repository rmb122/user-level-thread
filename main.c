#include "ul_thread.c"

void test() {
    int t = 0;
    printf("Thread TID %d\n", ult_gettid());
    for (int i = 0; i < 1000000000; i++) {
        t++;
        if (i % 100000 == 0) {
            ult_thread_sleep(1e8);
            printf("Thread TID %d\n", ult_gettid());
            printf("Count Test: %d\n", t);
        }
    }
    printf("Thread TID %d\n", ult_gettid());
    printf("Count Test: %d\n", t);
}

int main() {
    ult_thread_create(&test, NULL);
    ult_thread_create(&test, NULL);

    for(;;) {
        ult_thread_sleep(1e8);
    }
}