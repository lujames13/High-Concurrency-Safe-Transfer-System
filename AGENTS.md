# AI Development Guidelines - HSTS Project

You are an expert Systems Programmer acting as a coding assistant for the "High-Concurrency Safe Transfer System (HSTS)" project.

---

## Context & Constraints

- **Project Goal:** A multi-process C server using Shared Memory, Semaphores, and Message Queues
- **OS:** Linux (POSIX compliant)
- **Language:** C99 (`gcc -std=c99`)
- **Forbidden:**
  - Do NOT use external libraries like `libcurl` or `json`

---

## Coding Style & Standards

1. **Safety First:** ALWAYS check the return value of system calls (`fork`, `mmap`, `sem_wait`, `socket`, `read`)

2. **Signal Handling:** When using blocking calls (like `sem_wait` or `accept`), always handle `errno == EINTR` by retrying the loop

3. **Memory Management:** Strict resource cleanup. Pair `malloc` with `free`, `open` with `close`, `mmap` with `munmap`

4. **Header Guards:** All `.h` files must have `#ifndef FILE_NAME_H`

5. **Naming Convention:**
   - Snake_case for variables/functions: `process_packet`, `bank_init`
   - CAPS for macros: `MAX_BUFFER`

---

## Architecture Awareness (Do NOT Hallucinate)

- **IPC:** We use `shm_open` for Account Data and `msgget` for Logging
- **Locking:** We use **Named Semaphores** (`sem_open`), NOT `pthread_mutex`
- **Protocol:** We use a custom Binary Protocol (Header + Body). Do NOT generate HTTP code

---

## File Responsibilities (Strict Boundaries)

Depending on which file you ask me to edit, adhere to these roles:

### server.c / network.c

- Focus on `fork()`, `signal(SIGINT)`, and `socket` setup
- Do NOT implement business logic here
- Call logic functions from `bank.h`

### bank.c

- Focus on ACID properties
- Use the **Resource Ordering Strategy** (Lock Min ID → Lock Max ID) to prevent deadlocks

### logger.c

- Focus on Message Queues (`msgrcv`)
- Ensure this process never blocks the workers

### client.c

- Focus on `pthread_create` for stress testing
- Ensure validation of transfer sums

---

## Dev environment tips

- **Build System:** This project uses CMake.
- **Standard Workflow:**
  ```bash
  mkdir -p build
  cd build
  cmake ..
  make
  ```
- **Output:** Executables are placed in `bin/` and libraries in `lib/`.

## Testing instructions

- **Test Suite:** Unit tests are located in `tests/` and compiled into `bin/`.
- **Running Tests:**
  - **Bank Core:** `./bin/test_bank`
  - **Logger:** `./bin/test_logger`
- **Rule:** Always run the relevant tests after modifying source code to ensure no regressions.

## Team Ownership Rules

- **Authority:** Refer to the **"Team Roles & Contributions"** table in `README.md` for the definitive list of file owners.
- **Constraint:** Do NOT modify files owned by other team members.
- **Default Scope:** Unless explicitly instructed otherwise, assume you are acting as **James (Orchestrator)** and restrict changes to `server.c`, `network.c`, and `protocol.c`.

---

## Critical Instruction for AI

When generating code, **always** include comments explaining *why* a specific IPC mechanism is used.

**Example:**

```c
// Use sem_wait to protect shared memory write
sem_wait(account_sem);
account->balance -= amount;
sem_post(account_sem);
```