#include "logger.h"
#include <stdio.h>

int logger_mq_init() {
    // TODO: Member 3 implements this
    return 0;
}

void logger_mq_cleanup(int mqid) {
    // TODO: Member 3 implements this
}

void logger_send_async(int mqid, int type, int status, int src, int dst, int amt) {
    // TODO: Member 3 implements this
}

void logger_main_loop(int mqid) {
    // TODO: Member 3 implements this
    printf("[Logger] Stub loop running...\n");
    while(1);
}
