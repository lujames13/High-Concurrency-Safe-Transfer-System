// 檔案: tests/test_robust_crash.c
#define _DEFAULT_SOURCE  // 解決 usleep 警告
#include "bank.h"      
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s [victim|survivor]\n", argv[0]);
        return 1;
    }

    // 連接 Shared Memory
    BankMap *bank = get_bank_map();
    if (!bank) {
        fprintf(stderr, "Failed to connect to SHM.\n");
        return 1;
    }

    Account *acc = &bank->accounts[0]; // 我們只測試 ID 0 的鎖

    if (strcmp(argv[1], "victim") == 0) {
        printf("[Victim] Trying to lock Account 0...\n");
        pthread_mutex_lock(&acc->lock);
        printf("[Victim] Acquired lock! I will crash in 3 seconds. DO NOT run me again.\n");
        printf("[Victim] While I am sleeping, run: ./bin/test_robust survivor\n");
        sleep(3);
        
        printf("[Victim] Simulating CRASH (kill -9)!\n");
        raise(SIGKILL); // 自殺，鎖不會被釋放
    } 
    else if (strcmp(argv[1], "survivor") == 0) {
        printf("[Survivor] Waiting for lock on Account 0...\n");
        
        // 嘗試獲取鎖
        int r = pthread_mutex_lock(&acc->lock);
        
        if (r == EOWNERDEAD) {
            printf("[Survivor] SUCCESS! Detected EOWNERDEAD.\n");
            pthread_mutex_consistent(&acc->lock); // 修復鎖
            printf("[Survivor] Mutex recovered. I now hold the lock.\n");
            
            // 檢查是否真的能運作
            acc->balance += 100; 
            printf("[Survivor] Modified balance to %d. Unlocking...\n", acc->balance);
            pthread_mutex_unlock(&acc->lock);
        } else if (r == 0) {
            printf("[Survivor] Got lock normally (Victim didn't crash?).\n");
            pthread_mutex_unlock(&acc->lock);
        } else {
            printf("[Survivor] Failed with error: %d\n", r);
        }
    }

    return 0;
}