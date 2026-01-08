// 檔案位置: tests/test_monitor.c
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
#include <sys/time.h>   // [新增] 用於計算 TPS 微秒級時間
#include <time.h>
#include <math.h>       // [新增] 用於模擬正弦波流量
#include "logger.h"     // 引用學長的合約

// === 壓力測試設定 ===
#define TEST_COUNT 30000        // [極端測試] 總交易筆數
#define BATCH_UPDATE 500        // 每收幾筆更新一次畫面 (避免 I/O 瓶頸)
#define ENCRYPTION_KEY 0xAB 

// === 顏色定義 ===
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_RESET   "\x1b[0m"

volatile sig_atomic_t keep_running = 1;

void handle_signal(int sig) {
    keep_running = 0; 
}

// [輔助] 取得當前時間 (微秒)
long long current_timestamp() {
    struct timeval te; 
    gettimeofday(&te, NULL);
    return te.tv_sec * 1000000LL + te.tv_usec;
}

// [接收端] 解密並驗證 (僅抽樣檢查，避免拖慢速度)
void decrypt_and_verify(LogMessage *msg, int current_idx) {
    // 只有每 1000 筆詳細檢查一次，或是金額異常大時檢查
    if (current_idx % 1000 != 0 && msg->amount < 1000000) return;

    size_t payload_size = sizeof(LogMessage) - sizeof(long);
    char *ptr = (char *)msg + sizeof(long);
    
    // XOR 解密
    for (size_t i = 0; i < payload_size; i++) {
        ptr[i] ^= ENCRYPTION_KEY;
    }
}

int main() {
    signal(SIGINT, handle_signal);
    srand(time(NULL)); // 設定隨機種子

    printf("=== [Stress Test] 銀行轉帳極端壓力測試 (Target: %d tx) ===\n", TEST_COUNT);
    printf("提示: 系統將模擬 '波動式流量' 與 '髒資料攻擊'...\n\n");

    system("mkdir -p logs");

    // 1. 初始化 Queue
    int msqid = logger_mq_init();
    if (msqid == -1) {
        fprintf(stderr, "[Error] Queue init failed\n");
        return 1;
    }

    // 2. 準備計時
    long long start_time = current_timestamp();

    // 3. 產生分身 (Fork)
    pid_t pid = fork();

    if (pid == 0) {
        // ============================
        // [Sender] 壓力測試產生器 (攻擊方)
        // ============================
        printf("   [Sender] 啟動 30,000 筆極端交易注入...\n");
        
        for (int i = 0; i < TEST_COUNT; i++) {
            // A. [創新] 波動式流量模擬 (Sine Wave Traffic)
            // 模擬真實世界：忽快忽慢
            // 使用 sin 函數讓延遲在 0us 到 500us 之間波動
            double wave = sin(i * 0.01); // 週期變化
            int delay = (int)((wave + 1.0) * 250); // 0 ~ 500 us
            
            // 偶爾來個 "尖峰時刻" (Burst)，完全不等待
            if (rand() % 100 < 5) delay = 0; 

            if (delay > 0) usleep(delay);

            // B. [創新] 極端資料生成 (Fuzzing)
            int cmd_type, src, dst, amt, status;
            
            int scenario = rand() % 100;
            if (scenario < 70) {
                // 70% 正常轉帳
                cmd_type = 0x30; src = 1001; dst = 2001; amt = 100 + (i % 5000); status = 0;
            } else if (scenario < 90) {
                // 20% 高頻登入 (小封包)
                cmd_type = 0x10; src = rand(); dst = 0; amt = 0; status = 0;
            } else {
                // 10% 髒資料攻擊 (測試系統穩定性)
                // 負數金額、超大金額、未定義指令
                cmd_type = 0xFF; // 無效指令
                src = -1; 
                dst = -1; 
                amt = 99999999; // 巨額
                status = 1;     // 失敗狀態
            }

            // 發送 (非阻塞，如果 Queue 滿了可能會丟包，這是符合預期的壓力測試)
            logger_send_async(msqid, cmd_type, status, src, dst, amt);
        }
        
        printf("   [Sender] 30,000 筆發送完畢，任務結束。\n");
        exit(0);
    } 
    else {
        // ============================
        // [Receiver] 監控與效能分析端 (防守方)
        // ============================
        LogMessage msg;
        size_t msg_size = sizeof(LogMessage) - sizeof(long);
        
        int total_received = 0;
        long long batch_start_time = current_timestamp();
        
        // 開啟 Log 檔 (使用 Buffer I/O 減少磁碟寫入次數)
        FILE *fp = fopen("logs/stress_test.log", "w");
        if(fp) setvbuf(fp, NULL, _IOFBF, 8192); // 設定 8KB 緩衝區

        while (keep_running) {
            // 接收 (非阻塞，盡快消化)
            if (msgrcv(msqid, &msg, msg_size, 0, IPC_NOWAIT) != -1) {
                total_received++;
                
                // 解密驗證 (抽樣)
                decrypt_and_verify(&msg, total_received);

                // 寫入檔案 (簡單紀錄)
                if (fp) fprintf(fp, "Recv Log: Type=%d Amt=%d\n", msg.cmd_type, msg.amount);

                // [效能優化] 批次更新儀表板 (每 500 筆更新一次)
                if (total_received % BATCH_UPDATE == 0) {
                    struct msqid_ds buf;
                    msgctl(msqid, IPC_STAT, &buf); // 取得 Queue 狀態

                    // 計算 TPS (Transactions Per Second)
                    long long now = current_timestamp();
                    double time_diff_sec = (now - batch_start_time) / 1000000.0;
                    // 避免除以零
                    if (time_diff_sec < 0.0001) time_diff_sec = 0.0001; 
                    
                    int tps = (int)(BATCH_UPDATE / time_diff_sec);
                    batch_start_time = now; // 重置計時器

                    // 視覺化堆積條
                    int queue_usage = (int)((double)buf.msg_cbytes / buf.msg_qbytes * 20.0); // 0~20 格
                    char bar[21];
                    memset(bar, '#', queue_usage);
                    bar[queue_usage] = '\0';

                    // 根據 TPS 變色
                    char *color = ANSI_COLOR_GREEN;
                    if (tps > 10000) color = ANSI_COLOR_RED;      // 極限
                    else if (tps > 5000) color = ANSI_COLOR_YELLOW; // 高負載

                    printf("\r[Stress] 進度: %5d/%d | TPS: %s%-6d" ANSI_COLOR_RESET " | 堆積: [%-20s] %lu msgs", 
                        total_received, TEST_COUNT, color, tps, bar, buf.msg_qnum);
                    fflush(stdout);
                }

                // 測試結束條件
                if (total_received >= TEST_COUNT) break;

            } else {
                // Queue 空了，稍作休息
                if (errno == ENOMSG) {
                    // 檢查 Sender 是否已經結束且 Queue 真的空了
                    int status;
                    if (waitpid(pid, &status, WNOHANG) > 0) {
                         // 子行程已死，且沒信了 -> 結束
                         break;
                    }
                    usleep(100); // 0.1ms 極短暫休息
                }
            }
        }

        // 結算報告
        long long end_time = current_timestamp();
        double total_time = (end_time - start_time) / 1000000.0;
        
        printf("\n\n=== 壓力測試報告 ===\n");
        printf("1. 總耗時    : %.2f 秒\n", total_time);
        printf("2. 平均 TPS  : %.0f (交易/秒)\n", total_received / total_time);
        printf("3. 處理總量  : %d 筆\n", total_received);
        printf("4. 完整性    : %s\n", (total_received >= TEST_COUNT) ? "完美 (100%)" : "有遺失 (可能是 Queue 溢出)");
        
        if (fp) fclose(fp);
        logger_mq_cleanup(msqid);
    }

    return 0;
}
