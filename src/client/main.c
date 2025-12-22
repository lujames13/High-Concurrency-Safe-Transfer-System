#define _DEFAULT_SOURCE // 解決 usleep 被隱藏的問題   // 確保有這個標頭檔
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <time.h>

#include "protocol.h"

// ============================================================================
// Configuration
// ============================================================================
#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080
#define DEFAULT_STRESS_THREADS 100
#define STRESS_TRANSACTIONS_PER_THREAD 10

// ============================================================================
// Global Statistics (for Stress Test)
// ============================================================================
typedef struct {
    pthread_mutex_t lock;
    int total_requests;
    int success_count;
    int failure_count;
    long total_latency_ms; // Total response time
} StressStats;

static StressStats g_stats = {
    .lock = PTHREAD_MUTEX_INITIALIZER,
    .total_requests = 0,
    .success_count = 0,
    .failure_count = 0,
    .total_latency_ms = 0
};

// ============================================================================
// Helper: Connect to Server
// ============================================================================
int connect_to_server() {
    int sock;
    struct sockaddr_in serv_addr;

    // 1. Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("[Client] Socket creation error");
        return -1;
    }

    // 2. Configure server address
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);

    // 3. Convert IP address
    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        fprintf(stderr, "[Client] Invalid address: %s\n", SERVER_IP);
        close(sock);
        return -1;
    }

    // 4. Connect to server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("[Client] Connection failed");
        close(sock);
        return -1;
    }

    return sock;
}

// ============================================================================
// Helper: Send Packet and Receive Response
// ============================================================================
int send_and_receive(int sock, uint8_t op_code, const void* body, uint32_t body_len, int* ret_code) {
    PacketHeader send_header, recv_header;
    void* recv_body = NULL;

    // --- Step 1: Prepare and Send Request ---
    send_header.magic = PROTOCOL_MAGIC;
    send_header.op_code = op_code;
    send_header.body_len = htonl(body_len);

    // Calculate checksum (if body exists)
    if (body_len > 0) {
        uint16_t checksum = 0;
        const uint8_t* ptr = (const uint8_t*)body;
        for (uint32_t i = 0; i < body_len; i++) {
            checksum ^= ptr[i];
        }
        send_header.checksum = htons(checksum);
    } else {
        send_header.checksum = 0;
    }

    // Send Header
    if (write(sock, &send_header, sizeof(PacketHeader)) != sizeof(PacketHeader)) {
        perror("[Client] Failed to send header");
        return -1;
    }

    // Send Body (if exists)
    if (body_len > 0) {
        if (write(sock, body, body_len) != (ssize_t)body_len) {
            perror("[Client] Failed to send body");
            return -1;
        }
    }

    // --- Step 2: Receive Response ---
    if (protocol_read_packet(sock, &recv_header, &recv_body) < 0) {
        fprintf(stderr, "[Client] Failed to read response\n");
        return -1;
    }

    // --- Step 3: Parse Response ---
    if (recv_body && recv_header.body_len >= sizeof(int)) {
        *ret_code = ntohl(*(int*)recv_body);
        free(recv_body);
    } else {
        *ret_code = -1;
    }

    return 0;
}

// ============================================================================
// Interactive Mode: Login
// ============================================================================
void interactive_login() {
    int sock = connect_to_server();
    if (sock < 0) {
        fprintf(stderr, "[Client] Cannot connect to server\n");
        return;
    }

    printf("\n=== Login ===\n");
    printf("Enter Account ID (0-99): ");
    int account_id;
    if (scanf("%d", &account_id) != 1) {
        fprintf(stderr, "Invalid input\n");
        close(sock);
        return;
    }

    // Convert to network byte order
    int account_id_net = htonl(account_id);

    int ret_code = 0;
    if (send_and_receive(sock, OP_LOGIN, &account_id_net, sizeof(int), &ret_code) < 0) {
        fprintf(stderr, "[Client] Login failed\n");
    } else {
        if (ret_code == 0) {
            printf("✓ Login successful! Account ID: %d\n", account_id);
        } else {
            printf("✗ Login failed. Error code: %d\n", ret_code);
        }
    }

    close(sock);
}

// ============================================================================
// Interactive Mode: Check Balance
// ============================================================================
void interactive_balance() {
    int sock = connect_to_server();
    if (sock < 0) {
        fprintf(stderr, "[Client] Cannot connect to server\n");
        return;
    }

    printf("\n=== Check Balance ===\n");
    printf("Enter Account ID (0-99): ");
    int account_id;
    if (scanf("%d", &account_id) != 1) {
        fprintf(stderr, "Invalid input\n");
        close(sock);
        return;
    }

    // Convert to network byte order
    int account_id_net = htonl(account_id);

    int balance = 0;
    if (send_and_receive(sock, OP_BALANCE, &account_id_net, sizeof(int), &balance) < 0) {
        fprintf(stderr, "[Client] Query failed\n");
    } else {
        if (balance >= 0) {
            printf("✓ Account %d Balance: $%d\n", account_id, balance);
        } else {
            printf("✗ Query failed. Error code: %d\n", balance);
        }
    }

    close(sock);
}

// ============================================================================
// Interactive Mode: Transfer
// ============================================================================
void interactive_transfer() {
    int sock = connect_to_server();
    if (sock < 0) {
        fprintf(stderr, "[Client] Cannot connect to server\n");
        return;
    }

    printf("\n=== Transfer ===\n");
    printf("Source Account ID: ");
    int src_id;
    if (scanf("%d", &src_id) != 1) {
        fprintf(stderr, "Invalid input\n");
        close(sock);
        return;
    }

    printf("Destination Account ID: ");
    int dst_id;
    if (scanf("%d", &dst_id) != 1) {
        fprintf(stderr, "Invalid input\n");
        close(sock);
        return;
    }

    printf("Amount: ");
    int amount;
    if (scanf("%d", &amount) != 1) {
        fprintf(stderr, "Invalid input\n");
        close(sock);
        return;
    }

    // Prepare Transfer Body
    TransferBody tf;
    tf.src_id = htonl(src_id);
    tf.dst_id = htonl(dst_id);
    tf.amount = htonl(amount);

    int ret_code = 0;
    if (send_and_receive(sock, OP_TRANSFER, &tf, sizeof(TransferBody), &ret_code) < 0) {
        fprintf(stderr, "[Client] Transfer failed\n");
    } else {
        if (ret_code == 0) {
            printf("✓ Transfer successful: %d -> %d, Amount: $%d\n", src_id, dst_id, amount);
        } else {
            printf("✗ Transfer failed. Error code: %d\n", ret_code);
            switch (ret_code) {
                case -2: printf("  Reason: Invalid Account ID\n"); break;
                case -3: printf("  Reason: Same Account\n"); break;
                case -4: printf("  Reason: Invalid Amount\n"); break;
                case -5: printf("  Reason: Insufficient Balance\n"); break;
                default: printf("  Reason: Unknown Error\n"); break;
            }
        }
    }

    close(sock);
}

// ============================================================================
// Stress Test: Worker Thread
// ============================================================================
void* stress_worker(void* arg) {
    int thread_id = *(int*)arg;
    free(arg);

    for (int i = 0; i < STRESS_TRANSACTIONS_PER_THREAD; i++) {
        // Connect to server for each transaction (simulate real clients)
        int sock = connect_to_server();
        if (sock < 0) {
            pthread_mutex_lock(&g_stats.lock);
            g_stats.failure_count++;
            pthread_mutex_unlock(&g_stats.lock);
            continue;
        }

        // Random transfer: Pick random source and destination
        int src_id = rand() % 100;
        int dst_id = rand() % 100;
        while (dst_id == src_id) {
            dst_id = rand() % 100;
        }
        int amount = (rand() % 100) + 1; // 1-100

        // Prepare Transfer Body
        TransferBody tf;
        tf.src_id = htonl(src_id);
        tf.dst_id = htonl(dst_id);
        tf.amount = htonl(amount);

        // Measure latency
        struct timespec start, end;
        clock_gettime(CLOCK_MONOTONIC, &start);

        int ret_code = 0;
        int result = send_and_receive(sock, OP_TRANSFER, &tf, sizeof(TransferBody), &ret_code);

        clock_gettime(CLOCK_MONOTONIC, &end);
        long latency_ms = (end.tv_sec - start.tv_sec) * 1000 + 
                          (end.tv_nsec - start.tv_nsec) / 1000000;

        // Update statistics
        pthread_mutex_lock(&g_stats.lock);
        g_stats.total_requests++;
        g_stats.total_latency_ms += latency_ms;
        if (result == 0 && ret_code == 0) {
            g_stats.success_count++;
        } else {
            g_stats.failure_count++;
        }
        pthread_mutex_unlock(&g_stats.lock);

        close(sock);

        // Random sleep to simulate think time (10-50ms)
        usleep((rand() % 40 + 10) * 1000);
    }

    printf("[Thread %d] Completed %d transactions\n", thread_id, STRESS_TRANSACTIONS_PER_THREAD);
    return NULL;
}

// ============================================================================
// Stress Test Mode
// ============================================================================
void run_stress_test(int num_threads) {
    printf("\n=== Stress Test Mode ===\n");
    printf("Threads: %d\n", num_threads);
    printf("Transactions per thread: %d\n", STRESS_TRANSACTIONS_PER_THREAD);
    printf("Total expected transactions: %d\n\n", num_threads * STRESS_TRANSACTIONS_PER_THREAD);

    pthread_t* threads = malloc(sizeof(pthread_t) * num_threads);
    if (!threads) {
        perror("malloc failed");
        return;
    }

    // Seed random number generator
    srand(time(NULL));

    // Reset statistics
    memset(&g_stats, 0, sizeof(StressStats));
    pthread_mutex_init(&g_stats.lock, NULL);

    struct timespec test_start, test_end;
    clock_gettime(CLOCK_MONOTONIC, &test_start);

    // Launch threads
    for (int i = 0; i < num_threads; i++) {
        int* id = malloc(sizeof(int));
        *id = i;
        if (pthread_create(&threads[i], NULL, stress_worker, id) != 0) {
            perror("pthread_create failed");
            free(id);
        }
    }

    // Wait for all threads
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    clock_gettime(CLOCK_MONOTONIC, &test_end);
    long total_time_ms = (test_end.tv_sec - test_start.tv_sec) * 1000 +
                         (test_end.tv_nsec - test_start.tv_nsec) / 1000000;

    // Print statistics
    printf("\n=== Test Results ===\n");
    printf("Total Duration: %ld ms\n", total_time_ms);
    printf("Total Requests: %d\n", g_stats.total_requests);
    printf("Success: %d (%.2f%%)\n", g_stats.success_count, 
           (g_stats.success_count * 100.0) / g_stats.total_requests);
    printf("Failure: %d (%.2f%%)\n", g_stats.failure_count,
           (g_stats.failure_count * 100.0) / g_stats.total_requests);
    
    if (g_stats.total_requests > 0) {
        printf("Avg Latency: %.2f ms\n", 
               (double)g_stats.total_latency_ms / g_stats.total_requests);
        printf("Throughput: %.2f req/s\n", 
               (g_stats.total_requests * 1000.0) / total_time_ms);
    }

    free(threads);
    pthread_mutex_destroy(&g_stats.lock);
}

// ============================================================================
// Interactive Mode: Main Menu
// ============================================================================
void interactive_mode() {
    int choice;

    while (1) {
        printf("\n========================================\n");
        printf("   High-Concurrency Transfer Client\n");
        printf("========================================\n");
        printf("1. Login\n");
        printf("2. Check Balance\n");
        printf("3. Transfer Money\n");
        printf("4. Exit\n");
        printf("========================================\n");
        printf("Enter your choice: ");

        if (scanf("%d", &choice) != 1) {
            // Clear input buffer
            int c;
            while ((c = getchar()) != '\n' && c != EOF);
            printf("Invalid input. Please enter a number.\n");
            continue;
        }

        switch (choice) {
            case 1:
                interactive_login();
                break;
            case 2:
                interactive_balance();
                break;
            case 3:
                interactive_transfer();
                break;
            case 4:
                printf("\nGoodbye!\n");
                return;
            default:
                printf("Invalid choice. Please try again.\n");
                break;
        }
    }
}

// ============================================================================
// Main Entry Point
// ============================================================================
int main(int argc, char *argv[]) {
    printf("=== HSTS Client ===\n");
    printf("Server: %s:%d\n\n", SERVER_IP, SERVER_PORT);

    // Parse command-line arguments
    if (argc == 2 && strcmp(argv[1], "--stress") == 0) {
        // Stress test mode with default threads
        run_stress_test(DEFAULT_STRESS_THREADS);
    } else if (argc == 3 && strcmp(argv[1], "--stress") == 0) {
        // Stress test mode with custom thread count
        int num_threads = atoi(argv[2]);
        if (num_threads <= 0 || num_threads > 1000) {
            fprintf(stderr, "Invalid thread count. Must be between 1 and 1000.\n");
            return 1;
        }
        run_stress_test(num_threads);
    } else if (argc == 1) {
        // Interactive mode
        interactive_mode();
    } else {
        // Show usage
        printf("Usage:\n");
        printf("  %s                    - Interactive mode\n", argv[0]);
        printf("  %s --stress          - Stress test with 100 threads\n", argv[0]);
        printf("  %s --stress <N>      - Stress test with N threads\n", argv[0]);
        return 1;
    }

    return 0;
}