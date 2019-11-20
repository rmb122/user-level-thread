//
// Created by rmb122 on 2019/11/17.
//

#ifndef ULT_UL_THREAD_H
#define ULT_UL_THREAD_H

typedef struct __ult_tcb ult_tcb;
void ult_tcb_add_to_list(ult_tcb *tcb, ult_tcb *tcb_list);
ult_tcb *ult_pop_from_list(ult_tcb *tcb_list);
void ult_clean_dead_thread();
void ult_debug_print_list(ult_tcb *tcb_list, char *name);
void ult_scheduler_ticksleep();
void ult_debug_all_list();
void ult_schedule_sleep_thread();
int ult_get_tid();
#endif //ULT_UL_THREAD_H
