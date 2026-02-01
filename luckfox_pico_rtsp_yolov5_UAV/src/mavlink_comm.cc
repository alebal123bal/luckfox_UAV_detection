#include "mavlink_comm.h"
#include "uart_comm.h"
#include <string.h>
#include <sys/time.h>
#include <stdio.h>

// MAVLink v2 constants
#define MAVLINK_STX 0xFD
#define MAVLINK_DETECTION_MSG_ID 9000  // Custom message ID for detection

// System and component IDs
static uint8_t system_id = 1;
static uint8_t component_id = 1;
static uint8_t msg_seq = 0;

// CRC-16/MCRF4XX (MAVLink checksum)
static uint16_t crc_calculate(const uint8_t* data, uint16_t length) {
    uint16_t crc = 0xFFFF;
    
    for (uint16_t i = 0; i < length; i++) {
        uint8_t tmp = data[i] ^ (uint8_t)(crc & 0xFF);
        tmp ^= (tmp << 4);
        crc = (crc >> 8) ^ (tmp << 8) ^ (tmp << 3) ^ (tmp >> 4);
    }
    
    return crc;
}

static uint64_t get_time_usec() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000ULL + (uint64_t)tv.tv_usec;
}

int mavlink_pack_detection(
    int x, int y, int width, int height,
    float confidence, uint8_t class_id, uint8_t target_num,
    int frame_width, int frame_height,
    uint8_t* buffer, int buffer_size
) {
    // Calculate normalized coordinates (-1 to 1, center is 0)
    float norm_x = ((float)x / frame_width) * 2.0f - 1.0f;
    float norm_y = ((float)y / frame_height) * 2.0f - 1.0f;
    float norm_width = (float)width / frame_width;
    float norm_height = (float)height / frame_height;
    
    // Create payload
    mavlink_detection_payload_t payload;
    payload.time_usec = get_time_usec();
    payload.x = norm_x;
    payload.y = norm_y;
    payload.width = norm_width;
    payload.height = norm_height;
    payload.confidence = confidence;
    payload.target_num = target_num;
    payload.class_id = class_id;
    
    uint8_t payload_len = sizeof(mavlink_detection_payload_t);
    
    // Check buffer size
    int total_len = 10 + payload_len + 2; // header + payload + checksum
    if (buffer_size < total_len) {
        fprintf(stderr, "MAVLink buffer too small\n");
        return -1;
    }
    
    // Build MAVLink message
    uint8_t* ptr = buffer;
    
    // Header
    *ptr++ = MAVLINK_STX;                    // magic
    *ptr++ = payload_len;                    // length
    *ptr++ = 0;                              // incompat_flags
    *ptr++ = 0;                              // compat_flags
    *ptr++ = msg_seq++;                      // sequence
    *ptr++ = system_id;                      // sysid
    *ptr++ = component_id;                   // compid
    *ptr++ = MAVLINK_DETECTION_MSG_ID & 0xFF;        // msgid low
    *ptr++ = (MAVLINK_DETECTION_MSG_ID >> 8) & 0xFF; // msgid mid
    *ptr++ = (MAVLINK_DETECTION_MSG_ID >> 16) & 0xFF;// msgid high
    
    // Payload
    memcpy(ptr, &payload, payload_len);
    ptr += payload_len;
    
    // Calculate checksum (header + payload, excluding magic byte)
    uint16_t checksum = crc_calculate(buffer + 1, 9 + payload_len);
    
    // Add checksum
    *ptr++ = checksum & 0xFF;
    *ptr++ = (checksum >> 8) & 0xFF;
    
    return total_len;
}

int mavlink_send_detection(
    int uart_fd,
    int x, int y, int width, int height,
    float confidence, uint8_t class_id, uint8_t target_num,
    int frame_width, int frame_height
) {
    uint8_t buffer[280]; // MAVLink max message size
    
    int msg_len = mavlink_pack_detection(
        x, y, width, height,
        confidence, class_id, target_num,
        frame_width, frame_height,
        buffer, sizeof(buffer)
    );
    
    if (msg_len < 0) {
        return -1;
    }
    
    // Send via UART
    return uart_write(uart_fd, buffer, msg_len);
}
