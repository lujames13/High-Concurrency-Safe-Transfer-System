// 用於監控 Message Queue 狀態的伺服器範例程式  設計server main.c時需參考
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <errno.h>
#include <time.h>
#include "../../include/logger.h" 

// --- 顏色定義 (儀表板用) ---
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_RESET   "\x1b[0m"

// --- 全域變數 ---
volatile sig_atomic_t keep_running = 1;

// --- 1. 訊號處理 ---
void handle_signal(int sig) {
    keep_running = 0; 
}

// --- 2. 監控儀表板 ---
void monitor_queue(int msqid) {
    struct msqid_ds buf;

    // 查詢 Queue 狀態
    if (msgctl(msqid, IPC_STAT, &buf) == -1) {
        return;
    }

    unsigned long count = buf.msg_qnum;            
    unsigned long current_bytes = buf.__msg_cbytes;  
    unsigned long max_bytes = buf.msg_qbytes;      
    double load = (double)current_bytes / max_bytes * 100.0;

    // 取得時間
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char time_str[20];
    strftime(time_str, sizeof(time_str), "%H:%M:%S", t);

    printf("\r"); // 同一行更新

    if (load < 20.0) {
        printf("[%s] " ANSI_COLOR_CYAN "[監控] 狀態: 空閒 | 堆積: %lu | 負載: %.1f%%    " ANSI_COLOR_RESET, time_str, count, load);
    } else if (load < 70.0) {
        printf("[%s] " ANSI_COLOR_YELLOW "[監控] 狀態: 忙碌 | 堆積: %lu | 負載: %.1f%%    " ANSI_COLOR_RESET, time_str, count, load);
    } else {
        printf("[%s] " ANSI_COLOR_RED "[監控] 狀態: 擁塞 | 堆積: %lu | 負載: %.1f%%    " ANSI_COLOR_RESET, time_str, count, load);
    }
    fflush(stdout); 
}

// --- 主程式 ---
int main() {
    signal(SIGINT, handle_signal);

    printf("=== Server (MQ Verification Mode) Started ===\n");
    
    // 初始化 Message Queue
    int msqid = logger_mq_init();
    if (msqid == -1) {
        fprintf(stderr, "Fatal Error: Cannot init queue.\n");
        exit(1);
    }

    printf("監控儀表板啟動中... (按 Ctrl+C 結束)\n\n");

    while (keep_running) {
        monitor_queue(msqid);
        usleep(100000); // 每 0.1 秒更新一次
    }

    // 清理資源
    printf("\n[系統] 正在關閉...\n");
    logger_mq_cleanup(msqid);

    return 0;
}