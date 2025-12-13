#include "protocol.h"
#include <stddef.h>

int protocol_read_packet(int fd, PacketHeader* header, void** body) {
    // TODO: Implement protocol reading
    return -1;
}

void protocol_send_response(int fd, uint8_t op_code, int ret_code) {
    // TODO: Implement response sending
}
