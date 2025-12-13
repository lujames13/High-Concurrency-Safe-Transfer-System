#ifndef BANK_H
#define BANK_H

#include <semaphore.h>

#define MAX_ACCOUNTS 100
#define SHM_NAME "/hsts_shm"

typedef struct {
    int id;
    int balance;
    sem_t lock;
    char last_update[32];
} Account;

// Interface
Account* init_bank_shm();
int transfer(Account* accounts, int src, int dst, int amount);

#endif
