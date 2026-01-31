#ifndef UART_COMM_H
#define UART_COMM_H

#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize and open a UART serial port
 * 
 * @param port_num UART port number (e.g., 3 for /dev/ttyS3)
 * @param baud_rate Baud rate (e.g., 115200, 9600, etc.)
 * @return int File descriptor on success, -1 on failure
 */
int uart_init(int port_num, int baud_rate);

/**
 * @brief Send a formatted string over UART
 * 
 * @param fd File descriptor of the serial port
 * @param format Printf-style format string
 * @param ... Variable arguments for the format string
 * @return int Number of bytes written on success, -1 on failure
 */
int uart_printf(int fd, const char *format, ...);

/**
 * @brief Send raw data over UART
 * 
 * @param fd File descriptor of the serial port
 * @param data Pointer to data buffer
 * @param length Number of bytes to send
 * @return int Number of bytes written on success, -1 on failure
 */
int uart_write(int fd, const void *data, size_t length);

/**
 * @brief Close UART serial port
 * 
 * @param fd File descriptor of the serial port
 */
void uart_close(int fd);

#ifdef __cplusplus
}
#endif

#endif // UART_COMM_H
