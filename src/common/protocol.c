#include "protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>

// ============================================================================
// Helper: Calculate Simple Checksum (XOR-based)
// ============================================================================
static uint16_t calculate_checksum(const void* data, size_t len) {
    uint16_t checksum = 0;
    const uint8_t* ptr = (const uint8_t*)data;
    for (size_t i = 0; i < len; i++) {
        checksum ^= ptr[i];
    }
    return checksum;
}

// ============================================================================
// Helper: Safe Read with EINTR Handling
// ============================================================================
static int safe_read(int fd, void* buf, size_t count) {
    size_t total_read = 0;
    uint8_t* ptr = (uint8_t*)buf;
    
    while (total_read < count) {
        ssize_t n = read(fd, ptr + total_read, count - total_read);
        
        if (n < 0) {
            if (errno == EINTR) continue; // Retry on signal interrupt
            perror("[Protocol] read failed");
            return -1;
        }
        
        if (n == 0) {
            // Connection closed by peer
            return -1;
        }
        
        total_read += n;
    }
    
    return 0;
}

// ============================================================================
// Helper: Safe Write with EINTR Handling
// ============================================================================
static int safe_write(int fd, const void* buf, size_t count) {
    size_t total_written = 0;
    const uint8_t* ptr = (const uint8_t*)buf;
    
    while (total_written < count) {
        ssize_t n = write(fd, ptr + total_written, count - total_written);
        
        if (n < 0) {
            if (errno == EINTR) continue;
            perror("[Protocol] write failed");
            return -1;
        }
        
        total_written += n;
    }
    
    return 0;
}

// ============================================================================
// Public API: Read Packet
// ============================================================================
int protocol_read_packet(int fd, PacketHeader* header, void** body) {
    if (!header || !body) {
        fprintf(stderr, "[Protocol] Invalid parameters\n");
        return -1;
    }
    
    *body = NULL;
    
    // Step 1: Read Header (8 bytes)
    if (safe_read(fd, header, sizeof(PacketHeader)) < 0) {
        return -1;
    }
    
    // Step 2: Validate Magic Byte
    if (header->magic != PROTOCOL_MAGIC) {
        fprintf(stderr, "[Protocol] Invalid magic byte: 0x%02X (expected 0x%02X)\n", 
                header->magic, PROTOCOL_MAGIC);
        return -1;
    }
    
    // Step 3: Convert Network Byte Order to Host Byte Order
    header->checksum = ntohs(header->checksum);
    header->body_len = ntohl(header->body_len);
    
    // Step 4: Read Body (if exists)
    if (header->body_len > 0) {
        // Sanity check: Prevent memory exhaustion attacks
        if (header->body_len > 1024 * 1024) { // Max 1MB
            fprintf(stderr, "[Protocol] Body too large: %u bytes\n", header->body_len);
            return -1;
        }
        
        *body = malloc(header->body_len);
        if (!*body) {
            perror("[Protocol] malloc failed");
            return -1;
        }
        
        if (safe_read(fd, *body, header->body_len) < 0) {
            free(*body);
            *body = NULL;
            return -1;
        }
        
        // Step 5: Verify Checksum
        uint16_t calculated = calculate_checksum(*body, header->body_len);
        if (calculated != header->checksum) {
            fprintf(stderr, "[Protocol] Checksum mismatch: got 0x%04X, expected 0x%04X\n",
                    calculated, header->checksum);
            free(*body);
            *body = NULL;
            return -1;
        }
    }
    
    return 0;
}

// ============================================================================
// Public API: Send Response
// ============================================================================
void protocol_send_response(int fd, uint8_t op_code, int ret_code) {
    PacketHeader header;
    
    // Prepare Header
    header.magic = PROTOCOL_MAGIC;
    header.op_code = op_code;
    header.body_len = htonl(sizeof(int)); // Body is just the return code
    
    // Prepare Body
    int body = htonl(ret_code); // Convert to network byte order
    
    // Calculate Checksum
    header.checksum = htons(calculate_checksum(&body, sizeof(int)));
    
    // Send Header
    if (safe_write(fd, &header, sizeof(PacketHeader)) < 0) {
        return;
    }
    
    // Send Body
    if (safe_write(fd, &body, sizeof(int)) < 0) {
        return;
    }
}