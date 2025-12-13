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

#endif
