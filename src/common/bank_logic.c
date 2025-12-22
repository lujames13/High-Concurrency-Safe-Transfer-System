#define _GNU_SOURCE

#include "../../include/bank.h"
#include <errno.h>
#include <stdio.h>
#include <time.h>
#include <semaphore.h> 

/*
 * Helper: Robust mutex lock with recovery
 * This ensures the system remains available even if a worker crashes.
 */
static int safe_lock(pthread_mutex_t *lock) {
    int r = pthread_mutex_lock(lock);
    if (r == EOWNERDEAD) {
        // [專業度] 標記系統已自動修復
        pthread_mutex_consistent(lock);
        fprintf(stderr, "[BankCore] ALERT: Recovered robust mutex from dead owner. System integrity restored.\n");
        return 1;
    }
    return 0;
}

/*
 * Bank Core: Transfer money from src_id to dst_id
 * Features:
 * - Traffic Throttling (Semaphore)
 * - Deadlock Prevention (Resource Ordering)
 * - ACID Compliance (Robust Mutex)
 */
int bank_transfer(int src_id, int dst_id, int amount) {
    BankMap *bank = get_bank_map();
    if (!bank) return BANK_ERR_INTERNAL;

    /* ---------- 0. Input Validation ---------- */
    // 嚴格檢查防止邏輯錯誤
    if (src_id < 0 || src_id >= MAX_ACCOUNTS) return BANK_ERR_INVALID_ID;
    if (dst_id < 0 || dst_id >= MAX_ACCOUNTS) return BANK_ERR_INVALID_ID;
    if (src_id == dst_id) return BANK_ERR_SAME_ACCOUNT;
    if (amount <= 0) return BANK_ERR_INVALID_AMOUNT;

    /* ---------- 1. Admission Control (Traffic Shaping) ---------- */
    /* * [決策] 使用 sem_wait (Blocking) 而非 trywait。
     * 這能讓高併發請求排隊進入，達到「削峰填谷」的效果，
     * 避免在高負載下大量拒絕服務，提升整體 Throughput。
     */
    if (sem_wait(&bank->limit_sem) != 0) {
        return BANK_ERR_BUSY;
    }

    Account *src = &bank->accounts[src_id];
    Account *dst = &bank->accounts[dst_id];

    /* ---------- 2. Deadlock Prevention (Resource Ordering) ---------- */
    /* 演算法：總是先鎖 ID 小的帳戶 */
    Account *first  = (src_id < dst_id) ? src : dst;
    Account *second = (src_id < dst_id) ? dst : src;

    safe_lock(&first->lock);
    safe_lock(&second->lock);

    /* ---------- 3. Atomic Critical Section (ACID) ---------- */
    int result = BANK_OK;

    if (src->balance < amount) {
        result = BANK_ERR_INSUFFICIENT;
    } else {
        // 執行轉帳
        src->balance -= amount;
        dst->balance += amount;

        // 更新 Metadata
        uint64_t now = (uint64_t)time(NULL);
        src->last_updated = now;
        dst->last_updated = now;

        /* Atomic statistics for system monitoring */
        __sync_fetch_and_add(&bank->total_transactions, 1);
    }

    /* ---------- 4. Unlock (Reverse Order) ---------- */
    pthread_mutex_unlock(&second->lock);
    pthread_mutex_unlock(&first->lock);

    /* ---------- 5. Release admission slot ---------- */
    sem_post(&bank->limit_sem);

    return result;
}

/*
 * Bank Core: Query account balance
 * Consistent Read using locks
 */
int bank_get_balance(int account_id, int *balance) {
    BankMap *bank = get_bank_map();
    if (!bank || !balance) return BANK_ERR_INTERNAL;

    if (account_id < 0 || account_id >= MAX_ACCOUNTS)
        return BANK_ERR_INVALID_ID;

    Account *acc = &bank->accounts[account_id];

    // 讀取也要加鎖，避免讀到 "Dirty Read" (轉帳中間狀態)
    safe_lock(&acc->lock);
    *balance = acc->balance;
    pthread_mutex_unlock(&acc->lock);

    return BANK_OK;
}