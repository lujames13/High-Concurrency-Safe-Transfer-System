#ifndef LOGGER_H
#define LOGGER_H

// ============================================================================
// Auditor API (Implemented by Member 3 in src/common/mq_wrapper.c)
// ============================================================================

// Log Message Structure
typedef struct {
    long mtype;       // Message Type (e.g., 1)
    int cmd_type;     // CMD_TRANSFER, etc.
    int status;       // 0=Success, -1=Fail
    int src_id;
    int dst_id;
    int amount;
    // timestamp will be added by the logger process
} LogMessage;

/**
 * @brief Initialize Message Queue.
 * 
 * Behavior:
 * 1. msgget() to create or access the queue.
 * 
 * @return Message Queue ID (mqid).
 */
int logger_mq_init();

/**
 * @brief Cleanup Message Queue.
 * 
 * Behavior:
 * 1. msgctl(mqid, IPC_RMID, ...) to remove the queue.
 */
void logger_mq_cleanup(int mqid);

/**
 * @brief Send a log message asynchronously (Non-blocking).
 * 
 * Behavior:
 * 1. Construct LogMessage.
 * 2. msgsnd() with IPC_NOWAIT to avoid blocking the worker.
 * 
 * @param mqid Message Queue ID.
 * @param type Command type (e.g., OP_TRANSFER).
 * @param status Result status.
 * @param src Source ID.
 * @param dst Destination ID.
 * @param amt Amount.
 */
void logger_send_async(int mqid, int type, int status, int src, int dst, int amt);

/**
 * @brief Main loop for the Logger Process.
 * 
 * Behavior:
 * 1. Infinite loop.
 * 2. msgrcv() (Blocking) to wait for messages.
 * 3. Write to "transaction.log" with timestamp.
 * 
 * @param mqid Message Queue ID.
 */
void logger_main_loop(int mqid);

#endif // LOGGER_H
