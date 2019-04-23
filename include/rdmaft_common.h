#ifndef RDMAFT_COMMON_H
#define RDMAFT_COMMON_H

/**
 * @author dai minglong
 * @date 2019.4
*/

#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <rdmaft_recv.h>
#include <rdmaft_send.h>
#include <rdma/rdma_cma.h>

#define TEST_NZ(x) do { if ( (x)) rc_die("error: " #x " failed (returned non-zero)." ); } while (0)
#define TEST_Z(x)  do { if (!(x)) rc_die("error: " #x " failed (returned zero/null)."); } while (0)
#define MAX_FILE_NAME 256
#define MAX_DIR_NAME 256

/* arbitrary timeout time setting */
static const int TIMEOUT_IN_MS = 500;
static size_t BUFFER_SIZE = 10 * 1024 * 1024;//default value 10M

enum message_id
{
    MSG_INVALID = 0,//invalid
    MSG_MR,//registered memory info
    MSG_READY,//ready to receive data
    MSG_DONE//server received file successfully
};

/* message structure */
struct message
{
    int id;

    union
    {
        struct
        {
            uint64_t addr;
            uint32_t rkey;
            size_t buffer_size;
        } mr;
    } data;
};

typedef void (*pre_conn_cb_fn)(struct rdma_cm_id *id);
typedef void (*connect_cb_fn)(struct rdma_cm_id *id);
typedef void (*completion_cb_fn)(struct ibv_wc *wc);
typedef void (*disconnect_cb_fn)(struct rdma_cm_id *id);

void rc_init(pre_conn_cb_fn, connect_cb_fn, completion_cb_fn, disconnect_cb_fn);
void rc_disconnect(struct rdma_cm_id *id);
void rc_die(const char *message);
struct ibv_pd * rc_get_pd();

void rc_send_loop(struct rdmaft_send_client_context*);
void rc_recv_loop(struct rdmaft_recv_server_context*);
#endif