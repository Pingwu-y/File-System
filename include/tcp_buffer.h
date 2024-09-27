#ifndef _TCP_BUFFER_
#define _TCP_BUFFER_

#define TCP_BUF_SIZE 4096

typedef struct tcp_buffer {
    int read_index;
    int write_index;
    char buf[TCP_BUF_SIZE];
} tcp_buffer;

/**
 * @brief  Initialize a buffer
 *
 * Initializes a buffer.
 *
 * @return tcp_buffer    created buffer
 */
tcp_buffer *init_buffer();

/**
 * @brief  Send to buffer
 *
 * Send a string to the buffer.
 * The string will be regarded as a whole data packet.
 * The first 4 bytes of the message will be the length of the message.
 *
 * @param  buf   buffer to be written
 * @param  s     string to be written
 * @param  len   length of the string
 */
void send_to_buffer(tcp_buffer *buf, const char *s, int len);

/**
 * @brief  Buffer input
 *
 * Read all the data from the socket and write to the buffer.
 *
 * @param  buf     buffer to be written
 * @param  sockfd  socket to be read
 *
 * @return int     the number of bytes read, -1 if error or closed
 */
int buffer_input(tcp_buffer *buf, int sockfd);

/**
 * @brief  Buffer output
 *
 * Write all the data in the buffer to the socket.
 *
 * @param  buf     buffer to be read
 * @param  sockfd  socket to be written
 */
void buffer_output(tcp_buffer *buf, int sockfd);

/**
 * @brief  Adjust buffer
 *
 * If read_index is larger than TCP_BUF_SIZE / 2,
 * move the data to the beginning of the buffer.
 * Used after recycle_read and recycle_write.
 *
 * @param  buf   buffer to be adjusted
 */
void adjust_buffer(tcp_buffer *buf);

/**
 * @brief  Recycle write
 *
 * Move the write_index by len.
 *
 * @param  buf   buffer to be adjusted
 * @param  len   length to be moved
 */
void recycle_write(tcp_buffer *buf, int len);

/**
 * @brief  Recycle read
 *
 * Move the read_index by len.
 *
 * @param  buf   buffer to be adjusted
 * @param  len   length to be moved
 */
void recycle_read(tcp_buffer *buf, int len);

#endif
