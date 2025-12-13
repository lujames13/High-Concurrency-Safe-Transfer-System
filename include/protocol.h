#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

// Magic Byte
#define PROTOCOL_MAGIC 0x90

// Operation Codes
#define OP_LOGIN    0x10
#define OP_BALANCE  0x20
#define OP_TRANSFER 0x30
#define OP_ERROR    0xEE

// Packet Header
typedef struct {
    uint8_t  magic;    // 0x90
    uint8_t  op_code;  // Operation Type
    uint16_t checksum; // CRC16 of Body
    uint32_t body_len; // Length of Body
} __attribute__((packed)) PacketHeader;

// Transfer Body
typedef struct {
    int src_id;
    int dst_id;
    int amount;
} __attribute__((packed)) TransferBody;

// ============================================================================
// Protocol Helper API (Implemented in src/common/protocol.c)
// ============================================================================

/**
 * @brief Read a full packet from the socket.
 * 
 * Behavior:
 * 1. Read Header (blocking).
 * 2. Validate Magic Byte.
 * 3. Read Body (if body_len > 0).
 * 4. Verify Checksum.
 * 
 * @param fd Socket file descriptor.
 * @param header Pointer to store the read header.
 * @param body Pointer to store the read body (caller must free).
 * @return 0 on success, -1 on error/disconnect.
 */
int protocol_read_packet(int fd, PacketHeader* header, void** body);

/**
 * @brief Send a response packet.
 * 
 * @param fd Socket file descriptor.
 * @param op_code Operation code.
 * @param ret_code Return code (e.g., 0 for success, -1 for error).
 */
void protocol_send_response(int fd, uint8_t op_code, int ret_code);

#endif // PROTOCOL_H
