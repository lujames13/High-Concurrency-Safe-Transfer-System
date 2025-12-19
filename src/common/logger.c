// 檔案位置: src/server/logger.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>     // getpid
#include "../../include/logger.h" // 引用學長的合約

// [修改重點]
// 將原本的 void start_logger_process() 改成 int main()
// 因為 CMakeLists.txt 把這個檔案定義成 "add_executable"，所以它必須要有 main 才能跑
int main() {
    printf("[Logger Process] Started via fork... (PID: %d)\n", getpid());

    // 1. 初始化 Queue
    // 我們呼叫 mq_wrapper.c 提供的初始化函式
    int mqid = logger_mq_init();
    if (mqid == -1) {
        fprintf(stderr, "[Logger] Error: Queue not initialized!\n");
        exit(1); 
    }

    // 2. 啟動主迴圈
    // === 關鍵改變 ===
    // 原本這裡的 while(1), fopen, fprintf 邏輯，
    // 現在都已經封裝在 mq_wrapper.c 的 logger_main_loop() 裡面了。
    // 我們只要呼叫它，它就會開始：
    //   a. 無限迴圈收信
    //   b. 解密 (XOR)
    //   c. 寫入 logs/transaction.log
    //   d. 自動加上時間戳記
    // ================
    logger_main_loop(mqid);

    // 3. 收尾 (理論上 logger_main_loop 是無限迴圈不會跑出來)
    printf("[Logger Process] Exiting...\n");
    return 0;
}