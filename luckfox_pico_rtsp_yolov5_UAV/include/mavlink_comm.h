#ifndef MAVLINK_COMM_H
#define MAVLINK_COMM_H

#include <stdint.h>

// MAVLink packet structure
#pragma pack(push, 1)
typedef struct {
    uint8_t magic;              // Packet start marker (0xFD for MAVLink v2)
    uint8_t len;                // Payload length
    uint8_t incompat_flags;     // Incompatibility flags
    uint8_t compat_flags;       // Compatibility flags
    uint8_t seq;                // Packet sequence number
    uint8_t sysid;              // System ID
    uint8_t compid;             // Component ID
    uint8_t msgid_low;          // Message ID low byte
    uint8_t msgid_mid;          // Message ID middle byte
    uint8_t msgid_high;         // Message ID high byte
    uint8_t payload[255];       // Payload (max 255 bytes)
    uint16_t checksum;          // CRC-16/MCRF4XX
} mavlink_message_t;

// Detection payload structure
typedef struct {
    uint64_t time_usec;         // Timestamp (microseconds since system boot)
    float x;                    // X coordinate (normalized -1 to 1, 0 is center)
    float y;                    // Y coordinate (normalized -1 to 1, 0 is center)
    float width;                // Bounding box width (normalized 0 to 1)
    float height;               // Bounding box height (normalized 0 to 1)
    float confidence;           // Detection confidence (0.0 to 1.0)
    uint8_t target_num;         // Target number (for multiple detections)
    uint8_t class_id;           // Object class ID
} mavlink_detection_payload_t;
#pragma pack(pop)

/**
 * @brief Pack detection data into MAVLink format
 * 
 * @param x X coordinate in pixels
 * @param y Y coordinate in pixels
 * @param width Bounding box width in pixels
 * @param height Bounding box height in pixels
 * @param confidence Detection confidence (0.0 to 1.0)
 * @param class_id Object class ID
 * @param target_num Target number
 * @param frame_width Frame width in pixels (for normalization)
 * @param frame_height Frame height in pixels (for normalization)
 * @param buffer Output buffer for MAVLink message
 * @param buffer_size Size of output buffer
 * @return int Number of bytes written, or -1 on error
 */
int mavlink_pack_detection(
    int x, int y, int width, int height,
    float confidence, uint8_t class_id, uint8_t target_num,
    int frame_width, int frame_height,
    uint8_t* buffer, int buffer_size
);

/**
 * @brief Send MAVLink detection message via UART
 * 
 * @param uart_fd UART file descriptor
 * @param x X coordinate in pixels
 * @param y Y coordinate in pixels
 * @param width Bounding box width in pixels
 * @param height Bounding box height in pixels
 * @param confidence Detection confidence (0.0 to 1.0)
 * @param class_id Object class ID
 * @param target_num Target number
 * @param frame_width Frame width in pixels
 * @param frame_height Frame height in pixels
 * @return int Number of bytes sent, or -1 on error
 */
int mavlink_send_detection(
    int uart_fd,
    int x, int y, int width, int height,
    float confidence, uint8_t class_id, uint8_t target_num,
    int frame_width, int frame_height
);

#endif // MAVLINK_COMM_H
