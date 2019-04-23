/**
 * @author dai minglong
 * @date 2019.4
*/
#ifndef RDMAFT_SEND_H
#define RDMAFT_SEND_H

#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <rdma/rdma_cma.h>
#include <fcntl.h>
#include <libgen.h>
#include <pthread.h>

struct rdmaft_send_client_context {
    struct rdma_cm_id *conn;
    struct rdma_event_channel *ec;
    pthread_t* send_thread;
};

/* rdmaft send call back function pointer */
typedef void (*rdmaft_send_cb)(const char* filename);

/**
 * rdmaft send function
 * @addr    server_addr
 * @port    server listen port
 * @filename    file need to be transfered
 * @buffersize  rdma send memory buffer size 
 * @func    callback function
 * @return: rdmaft client context pointer
*/
struct rdmaft_send_client_context* 
    rdmaft_start_send(char* server_addr, char* port, char* filename, size_t buffer_size, rdmaft_send_cb func);

/**
 * rdmaft stop send function
 * @send_thread thread for transfer file
*/
void rdmaft_stop_send(struct rdmaft_send_client_context* client_context);
#endif