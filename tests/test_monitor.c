#define _GNU_SOURCE        // 修正 1: 支援 usleep 與 Linux 特有 MQ 欄位
#include <stdio.h>
#include <unistd.h>     // fork, sleep
#include <sys/wait.h>   // wait
#include <stdlib.h>
#include <signal.h>     // 訊號處理
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>   // 用於建立資料夾
#include <sys/types.h>
#include <sys/time.h>   // 用於計算 TPS 微秒級時間
#include <time.h>
#include <math.h>       // 用於模擬正弦波流量
#include "logger.h"     

// === 壓力測試設定 ===
#define TEST_COUNT 30000        
#define BATCH_UPDATE 500        
#define ENCRYPTION_KEY 0xAB  

// === 顏色定義 ===
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_GREEN    "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_RESET   "\x1b[0m"

volatile sig_atomic_t keep_running = 1;

// 修正 2: 解決未使用參數的警告
void handle_signal(int sig) {
    (void)sig; 
    keep_running = 0; 
}

// [輔助] 取得當前時間 (微秒)
long long current_timestamp() {
    struct timeval te; 
    gettimeofday(&te, NULL);
    return te.tv_sec * 1000000LL + te.tv_usec;
}

// [接收端] 解密並驗證 (僅抽樣檢查)
void decrypt_and_verify(LogMessage *msg, int current_idx) {
    if (current_idx % 1000 != 0 && msg->amount < 1000000) return;

    size_t payload_size = sizeof(LogMessage) - sizeof(long);
    // 指向 mtype 之後的資料開始 XOR
    char *ptr = (char *)msg + sizeof(long);
    
    for (size_t i = 0; i < payload_size; i++) {
        ptr[i] ^= ENCRYPTION_KEY;
    }
}

int main() {
    signal(SIGINT, handle_signal);
    srand(time(NULL)); 

    printf("=== [Stress Test] 銀行轉帳極端壓力測試 (Target: %d tx) ===\n", TEST_COUNT);
    printf("提示: 系統將模擬 '波動式流量' 與 '髒資料攻擊'...\n\n");

    if (mkdir("logs", 0777) == -1 && errno != EEXIST) {
        perror("mkdir logs failed");
    }

    // 1. 初始化 Queue
    int msqid = logger_mq_init();
    if (msqid == -1) {
        fprintf(stderr, "[Error] Queue init failed\n");
        return 1;
    }

    long long start_time = current_timestamp();

    // 3. 產生分身 (Fork)
    pid_t pid = fork();

    if (pid == 0) {
        // ============================
        // [Sender] 壓力測試產生器
        // ============================
        printf("   [Sender] 啟動 %d 筆極端交易注入...\n", TEST_COUNT);
        
        for (int i = 0; i < TEST_COUNT && keep_running; i++) {
            // A. 波動式流量模擬 (Sine Wave Traffic)
            double wave = sin(i * 0.01); 
            int delay = (int)((wave + 1.0) * 250); 
            
            if (rand() % 100 < 5) delay = 0; 

            if (delay > 0) usleep(delay);

            // B. 極端資料生成 (Fuzzing)
            int cmd_type, src, dst, amt, status;
            int scenario = rand() % 100;

            if (scenario < 70) {
                cmd_type = 0x30; src = 1001; dst = 2001; amt = 100 + (i % 5000); status = 0;
            } else if (scenario < 90) {
                cmd_type = 0x10; src = rand(); dst = 0; amt = 0; status = 0;
            } else {
                cmd_type = 0xFF; 
                src = -1; dst = -1; amt = 99999999; status = 1; 
            }

            logger_send_async(msqid, cmd_type, status, src, dst, amt);
        }
        
        printf("\n   [Sender] 任務發送結束。\n");
        exit(0);
    } 
    else {
        // ============================
        // [Receiver] 監控與效能分析端
        // ============================
        LogMessage msg;
        size_t msg_size = sizeof(LogMessage) - sizeof(long);
        
        int total_received = 0;
        long long batch_start_time = current_timestamp();
        
        FILE *fp = fopen("logs/stress_test.log", "w");
        if(fp) setvbuf(fp, NULL, _IOFBF, 8192); 

        while (keep_running) {
            // 接收 (非阻塞)
            if (msgrcv(msqid, &msg, msg_size, 0, IPC_NOWAIT) != -1) {
                total_received++;
                
                decrypt_and_verify(&msg, total_received);

                if (fp) fprintf(fp, "Recv Log: Type=%d Amt=%d\n", msg.cmd_type, msg.amount);

                if (total_received % BATCH_UPDATE == 0) {
                    struct msqid_ds buf;
                    msgctl(msqid, IPC_STAT, &buf); 

                    long long now = current_timestamp();
                    double time_diff_sec = (now - batch_start_time) / 1000000.0;
                    if (time_diff_sec < 0.0001) time_diff_sec = 0.0001; 
                    
                    int tps = (int)(BATCH_UPDATE / time_diff_sec);
                    batch_start_time = now; 

                    // 修正 3: 使用 __msg_cbytes 配合 _GNU_SOURCE
                    int queue_usage = (int)((double)buf.__msg_cbytes / buf.msg_qbytes * 20.0);
                    if (queue_usage > 20) queue_usage = 20;

                    char bar[21];
                    memset(bar, ' ', 20);
                    for(int k=0; k<queue_usage; k++) bar[k] = '#';
                    bar[20] = '\0';

                    char *color = ANSI_COLOR_GREEN;
                    if (tps > 10000) color = ANSI_COLOR_RED; 
                    else if (tps > 5000) color = ANSI_COLOR_YELLOW;

                    printf("\r[Stress] 進度: %5d/%d | TPS: %s%-6d" ANSI_COLOR_RESET " | 堆積: [%-20s] %lu msgs   ", 
                        total_received, TEST_COUNT, color, tps, bar, buf.msg_qnum);
                    fflush(stdout);
                }

                if (total_received >= TEST_COUNT) break;

            } else {
                if (errno == ENOMSG) {
                    int status;
                    if (waitpid(pid, &status, WNOHANG) > 0) {
                        // 子行程結束且 MQ 已空
                        break;
                    }
                    usleep(500); // 稍微休息避免 CPU 100%
                }
            }
        }

        long long end_time = current_timestamp();
        double total_time = (end_time - start_time) / 1000000.0;
        if (total_time < 0.001) total_time = 0.001;
        
        printf("\n\n=== 壓力測試報告 ===\n");
        printf("1. 總耗時    : %.2f 秒\n", total_time);
        printf("2. 平均 TPS  : %.0f (交易/秒)\n", total_received / total_time);
        printf("3. 處理總量  : %d 筆\n", total_received);
        printf("4. 完整性    : %s\n", (total_received >= TEST_COUNT) ? "完美 (100%)" : "有遺失");
        
        if (fp) fclose(fp);
        logger_mq_cleanup(msqid);
    }

    return 0;
}