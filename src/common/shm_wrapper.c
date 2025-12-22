#define _GNU_SOURCE

#include "../../include/bank.h"
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <semaphore.h>
#include <pthread.h>

static BankMap *shm_ptr = NULL;

/*
 * bank_init
 * - Creator (Master): Creates and initializes SHM (O_CREAT | O_EXCL)
 * - Attacher (Worker): Waits for initialization and attaches
 */
int bank_init() {
    int shm_fd;
    int is_creator = 0; // [修正] 用來標記我是不是第一個創建的人

    /* ---------- 1. Open / Create POSIX Shared Memory ---------- */
    // [修正] 加入 O_EXCL，確保只有一個人能創建成功
    shm_fd = shm_open(SHM_NAME, O_RDWR | O_CREAT | O_EXCL, 0666);
    
    if (shm_fd >= 0) {
        is_creator = 1; // 我是 Master
        printf("[BankCore] Master process detected. Initializing SHM...\n");
    } else if (errno == EEXIST) {
        // 檔案已存在，我是 Worker，直接開啟
        shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
        if (shm_fd < 0) {
            perror("[BankCore] Worker failed to open existing shm");
            return -1;
        }
    } else {
        perror("[BankCore] shm_open failed");
        return -1;
    }

    /* ---------- 2. Resize SHM (Only Creator needs to do this) ---------- */
    if (is_creator) {
        if (ftruncate(shm_fd, sizeof(BankMap)) == -1) {
            perror("[BankCore] ftruncate failed");
            close(shm_fd);
            shm_unlink(SHM_NAME); // 創建失敗要刪掉
            return -1;
        }
    }

    /* ---------- 3. mmap ---------- */
    shm_ptr = mmap(NULL,
                   sizeof(BankMap),
                   PROT_READ | PROT_WRITE,
                   MAP_SHARED,
                   shm_fd,
                   0);

    if (shm_ptr == MAP_FAILED) {
        perror("[BankCore] mmap failed");
        close(shm_fd);
        if (is_creator) shm_unlink(SHM_NAME);
        shm_ptr = NULL;
        return -1;
    }
    
    // Mapping 建立後，fd 就不需要了，可以關閉
    close(shm_fd);

    /* ---------- 4. Worker Wait Loop (等待 Master 初始化完成) ---------- */
    if (!is_creator) {
        int retries = 0;
        // [修正] Worker 必須等待 Magic Number 出現
        while (shm_ptr->is_initialized != 0xBEEF && retries < 200) {
            usleep(10000); // 等待 10ms
            retries++;
        }
        if (shm_ptr->is_initialized != 0xBEEF) {
            fprintf(stderr, "[BankCore] Error: Timeout waiting for Master init.\n");
            return -1;
        }
        return 0; // Worker Ready
    }

    /* ==========================================================
     * Master-only initialization section
     * ========================================================== */

    memset(shm_ptr, 0, sizeof(BankMap));

    /* Initialize RWLock */
    pthread_rwlockattr_t rw_attr;
    pthread_rwlockattr_init(&rw_attr);
    pthread_rwlockattr_setpshared(&rw_attr, PTHREAD_PROCESS_SHARED);
    if (pthread_rwlock_init(&shm_ptr->bank_lock, &rw_attr) != 0) {
        perror("rwlock init failed");
        return -1;
    }

    /* Initialize Mutex (Robust) */
    pthread_mutexattr_t mtx_attr;
    pthread_mutexattr_init(&mtx_attr);
    pthread_mutexattr_setpshared(&mtx_attr, PTHREAD_PROCESS_SHARED);
    pthread_mutexattr_setrobust(&mtx_attr, PTHREAD_MUTEX_ROBUST);

    /* Initialize Accounts */
    for (int i = 0; i < MAX_ACCOUNTS; i++) {
        shm_ptr->accounts[i].id = i;
        shm_ptr->accounts[i].balance = 10000;
        shm_ptr->accounts[i].last_updated = 0;
        pthread_mutex_init(&shm_ptr->accounts[i].lock, &mtx_attr);
    }

    /* Initialize Semaphore */
    // 使用 bank.h 定義的常數，方便未來調整並發量
    if (sem_init(&shm_ptr->limit_sem, 1, MAX_CONCURRENCY) != 0) { 
        perror("[BankCore] sem_init failed");
        return -1;
    }

    shm_ptr->total_transactions = 0;

    /* Mark initialization complete */
    shm_ptr->is_initialized = 0xBEEF;
    printf("[BankCore] Init Complete. Magic=0xBEEF\n");

    return 0;
}

BankMap* get_bank_map() {
    if (shm_ptr == NULL) {
        if (bank_init() != 0) return NULL;
    }
    return shm_ptr;
}

/* [新增] bank_detach: Client 離開時呼叫，不刪除檔案 */
int bank_detach() {
    if (shm_ptr) {
        munmap(shm_ptr, sizeof(BankMap));
        shm_ptr = NULL;
    }
    return 0;
}

/* [修正] bank_destroy: 只有 Server 關機時呼叫，會刪除檔案 */
int bank_destroy() {
    bank_detach(); // 先斷開
    shm_unlink(SHM_NAME); // 再刪除
    printf("[BankCore] SHM Unlinked (Destroyed).\n");
    return 0;
}