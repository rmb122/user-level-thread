#include <ucontext.h>
   #include <stdio.h>
   #include <stdlib.h>

   static ucontext_t uctx_main, uctx_func1, uctx_func2;

   #define handle_error(msg) \
       do { perror(msg); exit(EXIT_FAILURE); } while (0)

   static void
   func1(void)
   {
       printf("func1: started\n");
       printf("func1: swapcontext(&uctx_func1, &uctx_func2)\n");

       // swap 跳转uctx_func2
       if (swapcontext(&uctx_func1, &uctx_func2) == -1)
           handle_error("swapcontext");
       printf("func1: returning\n");

       // 执行完 跳转uctx_main
   }

   static void
   func2(void)
   {
       printf("func2: started\n");
       printf("func2: swapcontext(&uctx_func2, &uctx_func1)\n");

       // swap 跳转uctx_func1
       if (swapcontext(&uctx_func2, &uctx_func1) == -1)
           handle_error("swapcontext");
       printf("func2: returning\n");

       // 执行完 跳转uctx_func1
   }

   int
   main(int argc, char *argv[])
   {
       char func1_stack[16384];
       char func2_stack[16384];

       if (getcontext(&uctx_func1) == -1)
           handle_error("getcontext");

       // 预分配的栈空间
       uctx_func1.uc_stack.ss_sp = func1_stack;
       uctx_func1.uc_stack.ss_size = sizeof(func1_stack);

       // 后继上下文环境是uctx_main
       uctx_func1.uc_link = &uctx_main;

       // uctx_func1的上下文环境：func1
       makecontext(&uctx_func1, func1, 0);

       if (getcontext(&uctx_func2) == -1)
           handle_error("getcontext");

       // 预分配的栈空间
       uctx_func2.uc_stack.ss_sp = func2_stack;
       uctx_func2.uc_stack.ss_size = sizeof(func2_stack);

       // 后继上下文环境是uctx_func1（argc = 0的时候）
       /* Successor context is f1(), unless argc > 1 */
       uctx_func2.uc_link = (argc > 1) ? NULL : &uctx_func1;

       // uctx_func2的上下文环境：func2
       makecontext(&uctx_func2, func2, 0);

       // swap 跳转uctx_func2
       printf("main: swapcontext(&uctx_main, &uctx_func2)\n");
       if (swapcontext(&uctx_main, &uctx_func2) == -1)
           handle_error("swapcontext");

       printf("main: exiting\n");
       exit(EXIT_SUCCESS);
   }
