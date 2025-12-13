#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <string.h>

#include "bank.h"
#include "logger.h"
#include "protocol.h"

#define PORT 8080
#define WORKER_COUNT 4

// Global variables for cleanup in signal handler
Account* account_table = NULL;
int mq_id = -1;
int server_fd = -1;

void handle_signal(int sig) {
    if (sig == SIGINT) {
        printf("\n[Server] Caught SIGINT. Shutting down...\n");
        
        // 1. Cleanup Bank Resources (SHM, Semaphores)
        if (account_table) {
            bank_shm_cleanup();
            printf("[Server] Bank SHM cleaned up.\n");
        }

        // 2. Cleanup Logger Resources (Message Queue)
        if (mq_id != -1) {
            logger_mq_cleanup(mq_id);
            printf("[Server] Logger MQ cleaned up.\n");
        }

        // 3. Close Server Socket
        if (server_fd != -1) {
            close(server_fd);
        }

        // 4. Kill all children (Workers + Logger)
        // In a real app, we might want to wait for them to finish gracefully.
        // For now, we rely on them exiting or being killed by the OS if we exit.
        // But strictly, we should kill the process group.
        kill(0, SIGTERM);

        exit(0);
    }
}

int network_create_listener(int port) {
    int fd;
    struct sockaddr_in addr;
    int opt = 1;

    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(fd, 10) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    return fd;
}

void worker_process_loop(int server_socket) {
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    printf("[Worker %d] Ready to accept connections.\n", getpid());

    while (1) {
        // Accept connection (Blocking, but OS handles thundering herd or round-robin)
        int client_fd = accept(server_socket, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        if (client_fd < 0) {
            if (errno == EINTR) continue; // Retry on signal
            perror("accept");
            continue;
        }

        // Read Packet
        PacketHeader header;
        void* body = NULL;
        if (protocol_read_packet(client_fd, &header, &body) < 0) {
            close(client_fd);
            continue;
        }

        int ret_code = 0;

        // Dispatch based on OpCode
        switch (header.op_code) {
            case OP_TRANSFER: {
                if (header.body_len != sizeof(TransferBody)) {
                    ret_code = -1; // Invalid body size
                    break;
                }
                TransferBody* tf = (TransferBody*)body;
                
                // 1. Call Bank Core
                ret_code = bank_transfer(account_table, tf->src_id, tf->dst_id, tf->amount);

                // 2. Call Auditor (Async)
                logger_send_async(mq_id, OP_TRANSFER, ret_code, tf->src_id, tf->dst_id, tf->amount);
                break;
            }
            case OP_BALANCE: {
                // Assuming body contains just the int id, or we define a BalanceBody
                // For simplicity, let's assume body is just int id
                if (header.body_len != sizeof(int)) {
                    ret_code = -1;
                    break;
                }
                int* id_ptr = (int*)body;
                int balance = bank_get_balance(account_table, *id_ptr);
                // We need to send balance back. 
                // protocol_send_response might need to be more flexible to send data back.
                // For now, let's just use ret_code as balance if >= 0, or error code.
                // But typically response has a body.
                // The user's prompt didn't specify response format details, so I'll stick to simple ret_code for now.
                ret_code = balance; 
                break;
            }
            default:
                ret_code = -1; // Unknown OpCode
                break;
        }

        // Send Response
        protocol_send_response(client_fd, header.op_code, ret_code);

        // Cleanup
        if (body) free(body);
        close(client_fd);
    }
}

int main() {
    // 0. Setup Signal Handling
    signal(SIGINT, handle_signal);

    printf("[Server] Starting up...\n");

    // 1. Initialize Resources
    // Member 2: Bank Core
    account_table = bank_shm_init();
    if (!account_table) {
        fprintf(stderr, "Failed to initialize Bank SHM\n");
        exit(1);
    }
    printf("[Server] Bank SHM initialized.\n");

    // Member 3: Auditor
    mq_id = logger_mq_init();
    if (mq_id < 0) {
        fprintf(stderr, "Failed to initialize Logger MQ\n");
        bank_shm_cleanup(); // Cleanup previous
        exit(1);
    }
    printf("[Server] Logger MQ initialized.\n");

    // 2. Create Server Socket
    server_fd = network_create_listener(PORT);
    printf("[Server] Listening on port %d\n", PORT);

    // 3. Fork Logger Process (Member 3)
    pid_t logger_pid = fork();
    if (logger_pid == 0) {
        // Child: Logger
        // Close unused file descriptors if necessary
        logger_main_loop(mq_id);
        exit(0);
    }
    printf("[Server] Logger process started (PID: %d)\n", logger_pid);

    // 4. Prefork Worker Pool
    for (int i = 0; i < WORKER_COUNT; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            // Child: Worker
            worker_process_loop(server_fd);
            exit(0);
        }
    }
    printf("[Server] %d Worker processes started.\n", WORKER_COUNT);

    // 5. Master Wait Loop
    // Wait for children. If one dies, we could restart it (Supervisor mode).
    // For this assignment, we just wait.
    while (wait(NULL) > 0);

    return 0;
}
