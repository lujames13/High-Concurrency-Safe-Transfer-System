// 檔案位置: src/common/mq_wrapper.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <errno.h>
#include <time.h>       // 用於 logger_main_loop 寫時間
#include <unistd.h>     // usleep
#include "../../include/logger.h" // 引用學長的合約

// ==========================================
// [安全性設定] XOR 加密模組
// ==========================================
#define ENCRYPTION_KEY 0xAB 

// [修改] 為了適應 struct，我們改成對 "記憶體位元組" 進行加密
// void* data: 可以接受任何型態的指標
// size_t len: 要加密的長度 (bytes)
void apply_xor_cipher(void *data, size_t len) {
    char *ptr = (char *)data; // 強制轉型成 byte 指標
    for (size_t i = 0; i < len; i++) {
        ptr[i] ^= ENCRYPTION_KEY;
    }
}
// ==========================================

// 這些定義如果 logger.h 沒寫，我們這裡補上，確保能運作
#ifndef MQ_KEY_FILE
#define MQ_KEY_FILE "."
#endif
#ifndef MQ_KEY_ID
#define MQ_KEY_ID 65
#endif

// ----------------------------------------------------------------------------
// 1. 初始化 Message Queue
// ----------------------------------------------------------------------------
int logger_mq_init() {
    key_t key = ftok(MQ_KEY_FILE, MQ_KEY_ID);
    if (key == -1) {
        // 如果 ftok 失敗， fallback 用固定 Key (方便測試)
        key = 5678; 
    }

    // 0666 | IPC_CREAT: 允許讀寫，不存在則建立
    int mqid = msgget(key, 0666 | IPC_CREAT);
    if (mqid == -1) {
        perror("[MQ Wrapper] msgget failed");
        return -1;
    }

    printf("[MQ Wrapper] Queue initialized successfully. ID: %d\n", mqid);
    return mqid;
}

// ----------------------------------------------------------------------------
// 2. 清理資源 (對應 logger.h 的 logger_mq_cleanup)
// ----------------------------------------------------------------------------
void logger_mq_cleanup(int mqid) {
    if (mqid != -1) {
        if (msgctl(mqid, IPC_RMID, NULL) == -1) {
            perror("[MQ Wrapper] Cleanup failed");
        } else {
            printf("[MQ Wrapper] Queue (%d) removed successfully.\n", mqid);
        }
    }
}

// ----------------------------------------------------------------------------
// 3. 非阻塞發送 (對應 logger.h 的 logger_send_async)
// ----------------------------------------------------------------------------
void logger_send_async(int mqid, int type, int status, int src, int dst, int amt) {
    LogMessage msg;
    
    // 1. 填寫資料
    msg.mtype = 1;         // 必須 > 0
    msg.cmd_type = type;
    msg.status = status;
    msg.src_id = src;
    msg.dst_id = dst;
    msg.amount = amt;

    // 2. 計算 payload 大小 (只要加密資料部分，不要加密 mtype)
    // msgsnd 的大小定義是不包含 mtype 的
    size_t payload_size = sizeof(LogMessage) - sizeof(long);

    // 3. [加密] 取得資料部分的指標
    // 這裡我們跳過前面的 mtype，直接把後面的 int 欄位當成一坨 byte 來加密
    void *payload_ptr = (void *)((char *)&msg + sizeof(long));
    apply_xor_cipher(payload_ptr, payload_size);

    // 4. 發送 (使用 IPC_NOWAIT 避免卡住 Server)
    if (msgsnd(mqid, &msg, payload_size, IPC_NOWAIT) == -1) {
        // 如果 Queue 滿了 (EAGAIN)，為了不卡住 Server，我們選擇丟棄或印個 error
        // 這裡只印 error，不做處理
        if (errno != EAGAIN) {
            perror("[MQ Wrapper] Async send failed");
        } else {
            fprintf(stderr, "[MQ Wrapper] Queue full, log dropped!\n");
        }
    }
}

// ----------------------------------------------------------------------------
// 4. Logger 主迴圈 (對應 logger.h 的 logger_main_loop)
// ----------------------------------------------------------------------------
void logger_main_loop(int mqid) {
    LogMessage msg;
    size_t payload_size = sizeof(LogMessage) - sizeof(long);

    printf("[Logger Process] Started monitoring queue ID: %d...\n", mqid);
    printf("[Logger Process] Writing logs to logs/transaction.log\n");

    while (1) {
        // 1. 接收 (使用 0 = 阻塞模式，沒信就睡覺，節省 CPU)
        ssize_t result = msgrcv(mqid, &msg, payload_size, 1, 0);

        if (result == -1) {
            if (errno == EINTR) continue; // 被訊號中斷，繼續
            perror("[Logger Process] msgrcv failed");
            break; // 嚴重錯誤，退出
        }

        // 2. [解密] 收到的資料是亂碼，必須解密才能讀
        void *payload_ptr = (void *)((char *)&msg + sizeof(long));
        apply_xor_cipher(payload_ptr, payload_size);

        // 3. 準備寫檔
        // 先建立 logs 資料夾，確保不會寫檔失敗
        system("mkdir -p logs"); 
        
        FILE *fp = fopen("logs/transaction.log", "a");
        if (fp == NULL) {
            perror("[Logger Process] Cannot open log file");
            continue;
        }

        // 4. 取得時間
        time_t now = time(NULL);
        struct tm *t = localtime(&now);
        char time_str[30];
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", t);

        // 5. 格式化輸出
        // 將 int status 轉成易讀的文字
        char status_str[10];
        if (msg.status == 0) strcpy(status_str, "SUCCESS");
        else strcpy(status_str, "FAILED");

        // 這裡將數字代號轉為文字 (假設 OP code 定義)
        char op_str[20];
        if (msg.cmd_type == 0x10) strcpy(op_str, "LOGIN");
        else if (msg.cmd_type == 0x20) strcpy(op_str, "BALANCE");
        else if (msg.cmd_type == 0x30) strcpy(op_str, "TRANSFER");
        else sprintf(op_str, "OP_%d", msg.cmd_type);

        // 寫入檔案
        fprintf(fp, "[%s] CMD:%-10s | Status:%-8s | Src:%d -> Dst:%d | Amt:$%d\n",
                time_str, op_str, status_str, msg.src_id, msg.dst_id, msg.amount);

        fclose(fp);
        
        // 簡單顯示在螢幕上方便除錯
        // printf("[Logger] Recorded: %s\n", op_str);
    }
}