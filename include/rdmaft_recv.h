/**
 * @author dai minglong
 * @date 2019.4
*/
#ifndef RDMAFT_RECV_H
#define RDMAFT_RECV_H

#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <rdma/rdma_cma.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <pthread.h>

struct rdmaft_recv_server_context {
    struct rdma_cm_id* listener;
    struct rdma_event_channel* ec;
    pthread_t* recv_thread;
};

/* rdmaft recv call back function pointer*/
typedef void (*rdmaft_recv_cb)(const char* filename); 

/**
 * rdmaft start recv function
 * @port:    receive port
 * @recv_dir:   recv directory for the recv file
 * @buffer_size: rdma memory recv buffer size, 0 means use default
 * @func:   call back function    
 * @return: recv thread
 *
*/
struct rdmaft_recv_server_context* 
    rdmaft_start_recv(char* port, char* recv_dir, size_t buffer_size, rdmaft_recv_cb func);


/**
 * rdmaft stop recv function
 *@context  rdmaft server context 
*/
void rdmaft_stop_recv(struct rdmaft_recv_server_context* context);


/**
 * rdmaft reset rdma memory recv buffer size
 *@buffer_size  recv buffer size 
*/
void rdmaft_reset_recv_buffer_size(size_t buffer_size);
#endif