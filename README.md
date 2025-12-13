# High-Concurrency Safe Transfer System (HSTS)

## Project Overview

HSTS is a robust, high-performance banking simulation system designed to demonstrate mastery of **System Programming** concepts. It features a custom application-layer protocol, multi-process architecture, and strict ACID transaction compliance using IPC mechanisms.

**Key Technical Features:**

- **Architecture:** Pre-forking Process Pool (Master-Worker Model)
- **IPC:** Shared Memory (State Storage), Message Queues (Async Logging), Semaphores (Locking)
- **Concurrency:** Handling 100+ concurrent connections with Deadlock Prevention (Resource Ordering)
- **Protocol:** Custom Binary Protocol with Checksum and Magic Bytes

---

## System Architecture

```mermaid
graph TD
    Client -->|TCP Socket| Master[Master Process]
    Master -->|Fork| W1[Worker Process 1]
    Master -->|Fork| W2[Worker Process 2]
    Master -->|Fork| WN[Worker Process N]
    
    subgraph IPC Layer
        W1 <-->|Read/Write| SHM[(Shared Memory\nAccount Table)]
        W2 <-->|Read/Write| SHM
        WN <-->|Read/Write| SHM
        
        W1 -->|msgrcv| MQ[Message Queue]
        W2 -->|msgrcv| MQ
        WN -->|msgrcv| MQ
    end
    
    MQ -->|msgrcv| Logger[Logger Process]
    Logger -->|File I/O| Disk[transaction.log]
```

---

## Project File Structure

```text
.
├── AGENTS.md                  # AI Agent Rules
├── CMakeLists.txt             # Root Build Configuration
├── LICENSE
├── README.md                  # Project Documentation
├── bin/                       # [Generated] Output Executables
├── build/                     # [Generated] Build Artifacts
├── docs/
│   ├── prd.md                 # Product Requirements Document
│   └── project_spec.md        # Technical Specification
├── include/                   # Header Files
│   ├── bank.h                 # [Back Core] Banking Logic Interfaces
│   ├── logger.h               # [Auditor] Logging Interfaces
│   ├── protocol.h             # [Orchestrator] Protocol Definitions
│   └── utils.h                # [Shared] Utility Functions
├── lib/                       # [Generated] Output Libraries
├── logs/                      # [Generated] Runtime Logs
├── src/
│   ├── client/
│   │   ├── CMakeLists.txt
│   │   └── main.c             # [QA/Tester] Client Application Entry Point
│   ├── common/
│   │   ├── CMakeLists.txt
│   │   ├── mq_wrapper.c       # [Auditor] Message Queue Implementation
│   │   ├── protocol.c         # [Orchestrator] Protocol Implementation
│   │   └── shm_wrapper.c      # [Back Core] Shared Memory Implementation
│   └── server/
│       ├── CMakeLists.txt
│       └── main.c             # [Orchestrator] Server Application Entry Point
└── tests/
    ├── CMakeLists.txt
    ├── test_bank.c            # [Back Core] Bank Logic Tests
    └── test_logger.c          # [Auditor] Logger Tests
```

---

## Build & Run

### Prerequisites

- GCC / Clang
- CMake (3.10+)
- Linux Environment (Ubuntu/WSL2 recommended)

### Compilation

```bash
mkdir -p build
cd build
cmake ..
make
```

### Running the System

**1. Start the Server:**

```bash
./bin/server
# The server will initialize SHM, Semaphores, and listen on Port 8080.
```

**2. Run the Client (Interactive Mode):**

```bash
./bin/client
```

**3. Run Stress Test (Auto Mode):**

```bash
./bin/client --stress
# Launches 100 threads to simulate high-concurrency transfers.
```

---

## Development Workflow

To ensure code stability, please follow this workflow for development and testing.

### 1. Build Environment Setup

We use `cmake` for build management. Always perform builds in the `build/` directory.

```bash
# 1. Create build directory
mkdir -p build
cd build

# 2. Generate Makefiles
cmake ..

# 3. Compile
make
```

### 2. Testing Your Changes

Before pushing code, run the relevant unit tests found in the `bin/` directory.

**For Bank Core Logic (ACID/Transactions):**
```bash
./bin/test_bank
```

**For Logger & IPC:**
```bash
./bin/test_logger
```

### 3. Clean Rebuild

If you modify `CMakeLists.txt` or add new source files, perform a clean build:

```bash
rm -rf build/*
cd build
cmake ..
make
```

## Team Roles & Contributions

| Role | Member Name | Responsibilities | Files Owned |
|------|-------------|------------------|-------------|
| Orchestrator | 陸紀霖 | Network Layer, Protocol Parsing, Process Management | `server.c`, `network.c`, `protocol.c` |
| Bank Core | @Member2 | ACID Logic, Shared Memory, Deadlock Prevention Algorithm | `bank.c`, `shm_manager.c` |
| Auditor | @Member3 | Async Logging, Message Queue Management, File Persistence | `logger.c`, `mq_manager.c` |
| QA / Tester | @Member4 | Stress Testing, Client CLI, Multi-threaded Load Generation | `client.c` |

---

## License

Academic Project - [System Programming & Security] Fall 2025