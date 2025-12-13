# Product Requirement Document (PRD) - HSTS

## 1. System Specifications

### 1.1 Core Constraints

- **Max Concurrent Connections:** 10 (Throttled by Semaphore)
- **Max Accounts:** 100 (ID: 0-99)
- **Initial Balance:** 1000 per account
- **IPC Keys:**
  - SHM Path: `/hsts_shm`
  - Sem Name: `/hsts_sem_acc_{id}`
  - MQ Key: `0x1234` (ftok path: `.`)

### 1.2 Development Standards

- **Language:** C99 Standard
- **Error Handling:** All system calls (`shm_open`, `sem_wait`, `send`, `recv`) must check return values and handle `EINTR`
- **Endianness:** Network Byte Order (Big Endian) for all packet headers

---

## 2. Protocol Design (The "Contract")

### 2.1 Packet Structure

Every request and response follows this strict binary format.

**Header (8 Bytes):**

```c
struct PacketHeader {
    uint8_t  magic;    // 0x90 (Validation Byte)
    uint8_t  op_code;  // Operation Type
    uint16_t checksum; // CRC16 of Body (or simple XOR sum)
    uint32_t body_len; // Length of the Body payload
} __attribute__((packed));
```

### 2.2 Operation Codes (OpCodes)

| Hex | Mnemonic | Request Payload | Response Payload | Description |
|-----|----------|-----------------|------------------|-------------|
| `0x10` | `LOGIN` | UserID (4B) | Status (1B) | Auth check. Status: 1=OK, 0=Fail |
| `0x20` | `BALANCE` | (Empty) | Balance (4B) | Get current balance |
| `0x30` | `TRANSFER` | TargetID (4B), Amount (4B) | Status (1B), NewBalance (4B) | Transaction. Status: 1=Success, 0=Fail |
| `0xEE` | `ERROR` | (N/A) | ErrCode (4B) | Server error response |

---

## 3. Data Structures (Shared Memory)

Defined in `include/common.h`.

```c
typedef struct {
    int id;
    int balance;
    // Semaphores are handled externally via named semaphores or separate process-shared structs
    char last_update[32];
} Account;
```

---

## 4. Critical Algorithms

### 4.1 Deadlock Prevention (Resource Ordering)

**Owner:** Bank Core (Role 2)

To prevent deadlock when Account A transfers to B while B transfers to A:

1. Identify `id1` (Source) and `id2` (Target)
2. Determine `first_lock` = `min(id1, id2)` and `second_lock` = `max(id1, id2)`
3. **ALWAYS** acquire `first_lock` first
4. Then acquire `second_lock`
5. Perform operation
6. Release in reverse order

### 4.2 Async Logging

**Owner:** Auditor (Role 3)

1. Worker pushes `LogStruct` to Message Queue (Non-blocking)
2. Logger Process pops `LogStruct` (Blocking)
3. Logger writes to `transaction.log` with Timestamp