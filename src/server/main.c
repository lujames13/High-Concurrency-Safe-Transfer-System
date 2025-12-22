// 修改 1: 改用 _GNU_SOURCE 以支援 usleep 和 msg_cbytes
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
// 新增: MQ 與 Time 相關 Header
#include <sys/ipc.h>
#include <sys/msg.h>
#include <time.h>

#include "bank.h"
#include "logger.h"
#include "protocol.h"

#define PORT 8080
#define WORKER_COUNT 4

// --- 顏色定義 (儀表板用) ---
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_RESET   "\x1b[0m"

// ============================================================================
// Global Resources (for Signal Handler Cleanup)
// ============================================================================
static int mq_id = -1;
static int server_fd = -1;
static volatile sig_atomic_t keep_running = 1;

// ============================================================================
// Signal Handler: Graceful Shutdown
// ============================================================================
void handle_signal(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        keep_running = 0;
        printf("\n[Server] Caught signal %d. Initiating shutdown...\n", sig);
        
        // Cleanup Logger Resources (Message Queue)
        if (mq_id != -1) {
            logger_mq_cleanup(mq_id);
            printf("[Server] Logger MQ cleaned up.\n");
        }
        
        // Cleanup Bank Resources (Shared Memory)
        bank_destroy(); // Master process destroys SHM
        printf("[Server] Bank SHM destroyed.\n");
        
        // Close Server Socket
        if (server_fd != -1) {
            close(server_fd);
            printf("[Server] Server socket closed.\n");
        }
        
        // Kill all children in process group
        kill(0, SIGTERM);
        
        exit(0);
    }
}

// ============================================================================
// MQ Monitor Function (儀表板核心)
// ============================================================================
void monitor_queue(int msqid) {
    struct msqid_ds buf;

    // 查詢 Queue 狀態
    if (msgctl(msqid, IPC_STAT, &buf) == -1) {
        return;
    }

    unsigned long count = buf.msg_qnum;            
    unsigned long current_bytes = buf.__msg_cbytes; // GNU模式下通常用 __msg_cbytes
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

// ============================================================================
// Network: Create Listening Socket
// ============================================================================
int network_create_listener(int port) {
    int fd;
    struct sockaddr_in addr;
    int opt = 1;

    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("[Network] socket failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("[Network] setsockopt");
        close(fd);
        exit(EXIT_FAILURE);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("[Network] bind failed");
        close(fd);
        exit(EXIT_FAILURE);
    }

    if (listen(fd, 10) < 0) {
        perror("[Network] listen");
        close(fd);
        exit(EXIT_FAILURE);
    }

    return fd;
}

// ============================================================================
// Worker: Process Client Requests
// ============================================================================
void worker_process_loop(int server_socket, int mqid) {
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);
    
    // Worker must attach to existing SHM (not create)
    if (bank_init() != 0) {
        fprintf(stderr, "[Worker %d] Failed to attach to Bank SHM\n", getpid());
        exit(1);
    }
    
    BankMap* bank = get_bank_map();
    if (!bank) {
        fprintf(stderr, "[Worker %d] Failed to get Bank Map\n", getpid());
        exit(1);
    }

    printf("[Worker %d] Ready to accept connections.\n", getpid());

    while (keep_running) {
        int client_fd = accept(server_socket, (struct sockaddr *)&address, &addrlen);
        if (client_fd < 0) {
            if (errno == EINTR) continue; 
            perror("[Worker] accept");
            continue;
        }

        // 為了讓畫面乾淨，這裡我把 Worker 的 Log 註解掉，讓您專心看儀表板
        // printf("[Worker %d] Client connected...\n", getpid());

        PacketHeader header;
        void* body = NULL;
        if (protocol_read_packet(client_fd, &header, &body) < 0) {
            close(client_fd);
            continue;
        }

        int ret_code = 0;
        switch (header.op_code) {
            case OP_LOGIN: {
                ret_code = 0; 
                logger_send_async(mqid, OP_LOGIN, ret_code, 0, 0, 0);
                break;
            }
            case OP_BALANCE: {
                if (header.body_len != sizeof(int)) {
                    ret_code = BANK_ERR_INTERNAL;
                    break;
                }
                int account_id = ntohl(*(int*)body);
                int balance = 0;
                ret_code = bank_get_balance(account_id, &balance);
                if (ret_code == BANK_OK) ret_code = balance;
                logger_send_async(mqid, OP_BALANCE, ret_code, account_id, 0, 0);
                break;
            }
            case OP_TRANSFER: {
                if (header.body_len != sizeof(TransferBody)) {
                    ret_code = BANK_ERR_INTERNAL;
                    break;
                }
                TransferBody* tf = (TransferBody*)body;
                int src_id = ntohl(tf->src_id);
                int dst_id = ntohl(tf->dst_id);
                int amount = ntohl(tf->amount);
                
                ret_code = bank_transfer(src_id, dst_id, amount);
                logger_send_async(mqid, OP_TRANSFER, ret_code, src_id, dst_id, amount);
                break;
            }
            default:
                ret_code = BANK_ERR_INTERNAL;
                break;
        }

        protocol_send_response(client_fd, header.op_code, ret_code);
        if (body) free(body);
        close(client_fd);
    }
    bank_detach();
}

// ============================================================================
// Main: Master Process
// ============================================================================
int main() {
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    signal(SIGCHLD, SIG_IGN);

    printf("=== High-Concurrency Safe Transfer System (HSTS) ===\n");
    printf("[Server] Master process starting (PID: %d)...\n", getpid());

    // 1. Initialize Bank Core
    if (bank_init() != 0) {
        fprintf(stderr, "[Server] FATAL: Failed to initialize Bank SHM\n");
        exit(EXIT_FAILURE);
    }
    printf("[Server] ✓ Bank SHM initialized\n");

    // 2. Initialize Logger
    mq_id = logger_mq_init();
    if (mq_id < 0) {
        fprintf(stderr, "[Server] FATAL: Failed to initialize Logger MQ\n");
        bank_destroy();
        exit(EXIT_FAILURE);
    }
    printf("[Server] ✓ Logger MQ initialized (ID: %d)\n", mq_id);

    // 3. Create Server Socket
    server_fd = network_create_listener(PORT);
    printf("[Server] ✓ Listening on 0.0.0.0:%d\n", PORT);

    // 4. Fork Logger Process
    pid_t logger_pid = fork();
    if (logger_pid == 0) {
        close(server_fd);
        logger_main_loop(mq_id);
        exit(0);
    }
    printf("[Server] ✓ Logger process started (PID: %d)\n", logger_pid);

    // 5. Fork Worker Pool
    for (int i = 0; i < WORKER_COUNT; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            worker_process_loop(server_fd, mq_id);
            exit(0);
        }
        printf("[Server] ✓ Worker %d started (PID: %d)\n", i + 1, pid);
    }

    // ========================================================================
    // 6. [新增] Fork Monitor Process (高速監控版)
    // ========================================================================
    pid_t monitor_pid = fork();
    if (monitor_pid == 0) {
        close(server_fd);
        printf("\n[Monitor] 高速儀表板啟動 (取樣間隔 1ms)\n");
        sleep(1); 
        
        while (keep_running) {
            monitor_queue(mq_id); 
            // 修改 2: 將這裡的等待時間改短
            usleep(1000); // 1000 us = 0.001 秒 (極速監控)
        }
        exit(0);
    }
    // ========================================================================

    printf("\n[Server] System Ready. Press Ctrl+C to shutdown.\n");
    printf("========================================\n\n");

    // 7. Master Wait Loop
    int status;
    while (keep_running) {
        waitpid(-1, &status, 0);
    }

    printf("[Server] Master process exiting.\n");
    return 0;
}