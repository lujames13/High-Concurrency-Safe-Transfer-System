#ifndef BANK_H
#define BANK_H

#include <semaphore.h>
#include <stdint.h>

#define MAX_ACCOUNTS 100
#define SHM_NAME "/hsts_shm"

typedef struct {
    int id;
    int balance;
    sem_t lock;
    char last_update[32];
} Account;

// ============================================================================
// Bank Core API (Implemented in src/common/shm_wrapper.c)
// ============================================================================

/**
 * @brief Initialize Shared Memory and Semaphores.
 * 
 * Behavior:
 * 1. shm_open() with O_CREAT | O_RDWR.
 * 2. ftruncate() to set size.
 * 3. mmap() to map memory.
 * 4. If created (O_CREAT), initialize all accounts (balance=1000) and semaphores.
 * 5. If exists, just attach.
 * 
 * @return Pointer to the Account table in Shared Memory.
 */
Account* bank_shm_init();

/**
 * @brief Cleanup Shared Memory resources.
 * 
 * Behavior:
 * 1. munmap() the memory.
 * 2. shm_unlink() to remove the object.
 * 3. sem_close() and sem_unlink() for all semaphores.
 * CRITICAL: Must be called on server shutdown (SIGINT).
 */
void bank_shm_cleanup();

/**
 * @brief Execute a safe transfer between accounts.
 * 
 * Behavior:
 * 1. Validate IDs.
 * 2. Deadlock Prevention: Lock mutexes in ID order (min_id -> max_id).
 * 3. Check balance (src >= amount).
 * 4. Update balances.
 * 5. Unlock mutexes.
 * 
 * @param table Pointer to SHM account table.
 * @param src_id Source Account ID.
 * @param dst_id Destination Account ID.
 * @param amount Amount to transfer.
 * @return 0 on success, -1 if insufficient funds, -2 if invalid ID.
 */
int bank_transfer(Account* table, int src_id, int dst_id, int amount);

/**
 * @brief Get the balance of an account safely.
 * 
 * Behavior:
 * 1. Lock the specific account.
 * 2. Read balance.
 * 3. Unlock.
 * 
 * @param table Pointer to SHM account table.
 * @param id Account ID.
 * @return Current balance or -1 if invalid ID.
 */
int bank_get_balance(Account* table, int id);

#endif // BANK_H
