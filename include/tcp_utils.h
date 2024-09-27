/* ********************************
 * Description:  A simple TCP server and client library
 ********************************/

#ifndef _TCP_UTILS_
#define _TCP_UTILS_

#include "tcp_buffer.h"

typedef struct tcp_server_ *tcp_server;
typedef struct tcp_client_ *tcp_client;

/**
 * @brief  Initialize a TCP server
 *
 * Initializes a TCP server.
 *
 * @param  port           port number to listen
 * @param  num_threads    number of threads to be created in the threadpool
 * @param  add_client     function to be called when a new client is added, can be NULL
 * @param  handle_client  function to be called when a client sends a message,
 *                        you can send a message back
 * @param  clear_client   function to be called when a client is cleared, can be NULL
 *
 * @return tcp_server     created server
 */
tcp_server server_init(int port, int num_threads, void (*add_client)(int id),
                       int (*handle_client)(int id, tcp_buffer *write_buf,
                                            char *msg, int len),
                       void (*clear_client)(int id));

/**
 * @brief  Start the server loop
 *
 * Start the server loop. This function will not return.
 *
 * @param  server  server to be started
 */
void server_loop(tcp_server server);

/**
 * @brief  Initialize a TCP client
 *
 * Initializes a TCP client.
 *
 * @param  hostname  hostname of the server
 * @param  port      port number of the server
 *
 * @return tcp_client  created client
 */
tcp_client client_init(const char *hostname, int port);

/**
 * @brief  Send a message to the server
 *
 * Send a message to the server. The message will be regarded as a whole data
 * packet.
 *
 * @param  client  client to send the message
 * @param  msg     message to be sent
 * @param  len     length of the message
 */
void client_send(tcp_client client, const char *msg, int len);

/**
 * @brief  Receive a message from the server
 *
 * Receive a message from the server.
 *
 * @param  client   client to receive the message
 * @param  buf      buffer to store the message
 * @param  max_len  maximum length of the buffer
 *
 * @return int      the number of bytes received, -1 if error or closed
 */
int client_recv(tcp_client client, char *buf, int max_len);

/**
 * @brief  Destroy a TCP client
 *
 * Destroy a TCP client.
 *
 * @param  client  client to be destroyed
 */
void client_destroy(tcp_client client);

#endif
