#include "uart_comm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <stdarg.h>

int uart_init(int port_num, int baud_rate) {
    char serial_port[20];
    int serial_fd;
    struct termios tty;
    speed_t speed;

    // Map baud rate to speed_t constant
    switch (baud_rate) {
        case 9600:    speed = B9600; break;
        case 19200:   speed = B19200; break;
        case 38400:   speed = B38400; break;
        case 57600:   speed = B57600; break;
        case 115200:  speed = B115200; break;
        case 230400:  speed = B230400; break;
        case 460800:  speed = B460800; break;
        case 921600:  speed = B921600; break;
        default:
            fprintf(stderr, "Unsupported baud rate: %d\n", baud_rate);
            return -1;
    }

    // Create device path
    snprintf(serial_port, sizeof(serial_port), "/dev/ttyS%d", port_num);

    // Open serial port
    serial_fd = open(serial_port, O_RDWR | O_NOCTTY);
    if (serial_fd == -1) {
        perror("Failed to open serial port");
        return -1;
    }

    // Get current terminal attributes
    memset(&tty, 0, sizeof(tty));
    if (tcgetattr(serial_fd, &tty) != 0) {
        perror("Error from tcgetattr");
        close(serial_fd);
        return -1;
    }

    // Configure serial port settings
    cfsetospeed(&tty, speed);  // Set output baud rate
    cfsetispeed(&tty, speed);  // Set input baud rate

    tty.c_cflag &= ~PARENB;    // No parity bit
    tty.c_cflag &= ~CSTOPB;    // One stop bit
    tty.c_cflag &= ~CSIZE;     // Clear current data size setting
    tty.c_cflag |= CS8;        // 8 data bits

    // Apply settings
    if (tcsetattr(serial_fd, TCSANOW, &tty) != 0) {
        perror("Error from tcsetattr");
        close(serial_fd);
        return -1;
    }

    printf("UART%d initialized successfully at %d baud\n", port_num, baud_rate);
    return serial_fd;
}

int uart_printf(int fd, const char *format, ...) {
    char buffer[512];
    va_list args;
    int len;
    ssize_t bytes_written;

    if (fd < 0) {
        fprintf(stderr, "Invalid UART file descriptor\n");
        return -1;
    }

    // Format the string
    va_start(args, format);
    len = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if (len < 0) {
        fprintf(stderr, "Error formatting string\n");
        return -1;
    }

    if (len >= sizeof(buffer)) {
        fprintf(stderr, "Warning: UART message truncated (max 512 bytes)\n");
        len = sizeof(buffer) - 1;
    }

    // Write to UART
    bytes_written = write(fd, buffer, len);
    if (bytes_written < 0) {
        perror("Error writing to UART");
        return -1;
    }

    return (int)bytes_written;
}

int uart_write(int fd, const void *data, size_t length) {
    ssize_t bytes_written;

    if (fd < 0) {
        fprintf(stderr, "Invalid UART file descriptor\n");
        return -1;
    }

    if (data == NULL || length == 0) {
        fprintf(stderr, "Invalid data or length\n");
        return -1;
    }

    bytes_written = write(fd, data, length);
    if (bytes_written < 0) {
        perror("Error writing to UART");
        return -1;
    }

    return (int)bytes_written;
}

void uart_close(int fd) {
    if (fd >= 0) {
        close(fd);
        printf("UART closed\n");
    }
}
