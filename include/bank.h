#ifndef BANK_H
#define BANK_H

#include <pthread.h>
#include <semaphore.h>
#include <stdint.h>

#define MAX_ACCOUNTS 100
#define SHM_NAME "/hsts_bank_core"

// [新增] 定義最大並發數 (Semaphore 上限)
// 方便之後在 shm_manager.c 的 sem_init 使用
#define MAX_CONCURRENCY 10 

// Error Codes (給 Server/Client 用)
#define BANK_OK              0
#define BANK_ERR_INTERNAL   -1
#define BANK_ERR_INVALID_ID -2
#define BANK_ERR_SAME_ACCOUNT -3
#define BANK_ERR_INVALID_AMOUNT -4
#define BANK_ERR_INSUFFICIENT -5
#define BANK_ERR_BUSY       -6

// Account Structure (Cache Line Aligned)
// padding 用來避免 False Sharing，這在高併發寫入時非常重要
typedef struct {
    uint32_t id;
    int32_t  balance;
    pthread_mutex_t lock;
    uint64_t last_updated;
    char padding[8]; 
} Account;

// Bank Map Structure
typedef struct {
    uint32_t is_initialized;
    volatile uint64_t total_transactions;
    sem_t limit_sem;
    pthread_rwlock_t bank_lock;
    Account accounts[MAX_ACCOUNTS];
} BankMap;

// Public API
int bank_init();

// [新增] 給 Client (Worker) 使用：只斷開連結，不刪除檔案
int bank_detach(); 

// 給 Server (Master) 使用：斷開並刪除檔案
int bank_destroy();

BankMap* get_bank_map();
int bank_transfer(int src_id, int dst_id, int amount);
int bank_get_balance(int account_id, int *balance);

#endif
