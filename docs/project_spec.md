###  Final Project: High-Concurrency Client-Server Network Service System

I. Project Goal To develop a Client-Server network system featuring high concurrency capabilities, a custom protocol, and fault tolerance mechanisms. The primary objective is to demonstrate mastery of underlying Operating System mechanisms (Process/Thread/IPC) and software architecture design capabilities.

II. Technical Specifications

1. Client Side (Stress Testing & Simulation):
    
    - Must be designed using a Multi-threaded architecture.
        
    - Must be capable of performing stress testing, simulating at least 100 concurrent connections sending requests to the server simultaneously to verify the server's load capacity.
        
    - Bonus: Provide statistical data on Latency and Throughput.
        
2. Server Side (Core Service):
    
    - Must use a Multi-Process architecture to handle connections (e.g., Preforking or Master-Worker patterns).
    - Must implement IPC (Inter-Process Communication) mechanisms (e.g., Shared Memory, Message Queue, Pipe, Unix Domain Socket) for data exchange or state synchronization between processes (e.g., counting total service requests, managing shared cache data).
        
3. Communication Protocol (Protocol Design):
    
    - The use of existing application-layer protocols such as HTTP or WebSocket is STRICTLY PROHIBITED.
        
    - You must define your own Application Layer Protocol, explicitly specifying the Payload structure (including Header and Body).
        
    - Example Structure: [Packet Length (4 bytes)] + [OpCode (2 bytes)] + [Checksum] + [Data Content].
        
4. Architecture Design (Software Architecture):
    
    - Modularity & Library Encapsulation: Common functionalities (e.g., Socket wrappers, Protocol parsing, Logging systems) must be encapsulated into Static (.a) or Dynamic Libraries (.so) to be shared between the Client and Server.
        

III. Service Scenarios (Choose one or define your own) The Server must provide a service with actual business logic. Examples include:

- Image Processing Service: The Client transmits binary image files; the Server passes the image to Worker Processes via IPC for filter application or recognition processing, then returns the result.
    
- Real-time Trading System: Simulates bank deposits and withdrawals. Must use IPC and Lock mechanisms to ensure data consistency (ACID) when multiple processes access account balances simultaneously.
    
- IoT/Map State Synchronization: Multiple Clients upload location coordinates; the Server aggregates these coordinates and broadcasts updates to other Clients.
    

IV. Security & Reliability (Quality Attributes) You must explain your design rationale in the documentation and provide runtime screenshots as proof.
bonus: authentication, for anti spoofing

1. Security (Choose at least one, not limited):
    
    - Integrity Check: Add Checksum or CRC mechanisms to packets to prevent transmission errors.
        
    - Encryption: Implement simple encryption (e.g., TLS, XOR, AES) on the Payload to prevent plaintext transmission.
        
    - Authentication: Implement a simple Login Handshake process.
        
2. Reliability (Choose at least one, not limited):
    
    - Keep-Alive/Heartbeat: Detect disconnections and support automatic reconnection.
        
    - Graceful Shutdown: Ensure the Server safely closes processes and releases IPC resources upon receiving a system signal (e.g., SIGINT).
        
    - Timeout Handling: Handle scenarios involving network congestion or server busy states (Request Timeouts).
        

V. Team Collaboration & Submission

1. Individual Contribution: Each team member must be responsible for the development of at least one functional module (specify roles in the README).
    
2. Version Control: Source code must be hosted on GitHub.
    
3. Build System: The project must include a Makefile (or CMakeLists.txt) to facilitate compilation and testing by the instructor/TA.