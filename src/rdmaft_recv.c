/**
 * @author dai minglong
 * @date 2019.4
*/
#include <rdmaft_common.h>
#include <rdmaft_recv.h>

static rdmaft_recv_cb s_cb_func = NULL;
static char s_dir_name[MAX_DIR_NAME+MAX_FILE_NAME];

struct conn_context
{
    char *buffer;
    struct ibv_mr *buffer_mr;

    struct message *msg;
    struct ibv_mr *msg_mr;

    int fd;
    char file_name[MAX_FILE_NAME];
};

/* function used to send message */
static void send_message(struct rdma_cm_id *id)
{
    struct conn_context *ctx = (struct conn_context *)id->context;

    struct ibv_send_wr wr, *bad_wr = NULL;
    struct ibv_sge sge;

    memset(&wr, 0, sizeof(wr));

    wr.wr_id = (uintptr_t)id;
    wr.opcode = IBV_WR_SEND;
    wr.sg_list = &sge;
    wr.num_sge = 1;
    wr.send_flags = IBV_SEND_SIGNALED;

    sge.addr = (uintptr_t)ctx->msg;
    sge.length = sizeof(*ctx->msg);
    sge.lkey = ctx->msg_mr->lkey;

    TEST_NZ(ibv_post_send(id->qp, &wr, &bad_wr));
}

/* function used to post a rdma receive */
static void post_receive(struct rdma_cm_id *id)
{
    struct ibv_recv_wr wr, *bad_wr = NULL;

    memset(&wr, 0, sizeof(wr));

    wr.wr_id = (uintptr_t)id;
    wr.sg_list = NULL;
    wr.num_sge = 0;

    TEST_NZ(ibv_post_recv(id->qp, &wr, &bad_wr));
}

/* recv pre handle */
static void on_pre_conn(struct rdma_cm_id *id)
{
    struct conn_context *ctx = (struct conn_context *)malloc(sizeof(struct conn_context));

    id->context = ctx;

    ctx->file_name[0] = '\0';

    //regist memory for buffer
    posix_memalign((void **)&ctx->buffer, sysconf(_SC_PAGESIZE), BUFFER_SIZE);
    TEST_Z(ctx->buffer_mr = ibv_reg_mr(rc_get_pd(), ctx->buffer, BUFFER_SIZE, IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE));
    //regist memory for msg
    posix_memalign((void **)&ctx->msg, sysconf(_SC_PAGESIZE), sizeof(*ctx->msg));
    TEST_Z(ctx->msg_mr = ibv_reg_mr(rc_get_pd(), ctx->msg, sizeof(*ctx->msg), IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE));

    post_receive(id);
}

/* function used to handle a new connection connected */
static void on_connection(struct rdma_cm_id *id)
{
    struct conn_context *ctx = (struct conn_context *)id->context;

    //connection established so send local registed memory to the sender
    ctx->msg->id = MSG_MR;
    ctx->msg->data.mr.addr = (uintptr_t)ctx->buffer_mr->addr;
    ctx->msg->data.mr.rkey = ctx->buffer_mr->rkey;
    ctx->msg->data.mr.buffer_size = BUFFER_SIZE;
    send_message(id);
}

/* function used to hanle a chunk write completion */
static void on_completion(struct ibv_wc *wc)
{
    struct rdma_cm_id *id = (struct rdma_cm_id *)(uintptr_t)wc->wr_id;
    struct conn_context *ctx = (struct conn_context *)id->context;

    if (wc->opcode == IBV_WC_RECV_RDMA_WITH_IMM) {

        uint32_t size = ntohl(wc->imm_data);
        if (size == 0) {//no data received means trans ended

            //send a file transfer down message to client
            ctx->msg->id = MSG_DONE;
            send_message(id);
            
            //callback 
            if (s_cb_func != NULL) {
                s_cb_func(ctx->file_name);
            }
            //don't need post_receive() since we're done with this connection

        } else if (ctx->file_name[0]) {//filename are not '/0'
        
            ssize_t ret;
            ret = write(ctx->fd, ctx->buffer, size);// write file
            if (ret != size)
                rc_die("write() failed");

            //post a receive for next chunk
            post_receive(id);

            ctx->msg->id = MSG_READY;
            send_message(id);

        } else {
            
            size = (size > MAX_FILE_NAME) ? MAX_FILE_NAME : size;
            memcpy(ctx->file_name, ctx->buffer, size);
            ctx->file_name[size - 1] = '\0';
            strcat(s_dir_name,ctx->file_name);            

            ctx->fd = open(s_dir_name, O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
            if (ctx->fd == -1)
                rc_die("open() failed");

            //post a receive for the first chunk
            post_receive(id);

            ctx->msg->id = MSG_READY;
            send_message(id);

        }
    }
}

/* function used to handle connection disconnected */
static void on_disconnect(struct rdma_cm_id *id)
{
    struct conn_context *ctx = (struct conn_context *)id->context;

    //close file
    close(ctx->fd);
    
    //deregiste memory
    ibv_dereg_mr(ctx->buffer_mr);
    ibv_dereg_mr(ctx->msg_mr);
    //free buffer & msg memory
    free(ctx->buffer);
    free(ctx->msg);

    //free connection context
    free(ctx);
}

void *rdmaft_recv_thread_func(void *server_context) 
{
    rc_recv_loop((struct rdmaft_recv_server_context*)server_context);
    return NULL;
}

struct rdmaft_recv_server_context* 
    rdmaft_start_recv(char* port, char* recv_dir, size_t buffer_size, rdmaft_recv_cb func)
{
    //init event handle function pointer
    rc_init(
        on_pre_conn,
        on_connection,
        on_completion,
        on_disconnect);

    //init transfer finished callback function
    s_cb_func = func;

    //set buffer size
    if (buffer_size != 0) {
        BUFFER_SIZE = buffer_size;
    }

    //set recv dir
    int len = strlen(recv_dir);
    if (len>MAX_DIR_NAME) {
        rc_die("too long dir name");
    } else {
        memcpy(s_dir_name, recv_dir, len);
    }

    //addr info init
    struct sockaddr_in6 addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin6_family = AF_INET6;
    addr.sin6_port = htons(atoi(port));

    //create a server context
    struct rdmaft_recv_server_context *server_context = 
        (struct rdmaft_recv_server_context*)malloc(sizeof(struct rdmaft_recv_server_context));

    server_context->ec = NULL;
    server_context->listener = NULL;

    //create event channel
    TEST_Z(server_context->ec = rdma_create_event_channel());

    //create channel id(listener) 
    TEST_NZ(rdma_create_id(server_context->ec, &server_context->listener, NULL, RDMA_PS_TCP));

    //bind addr to listener & start listen 
    //server start listening connection backlog=10 is arbitrary
    TEST_NZ(rdma_bind_addr(server_context->listener, (struct sockaddr *)&addr));
    TEST_NZ(rdma_listen(server_context->listener, 10));
    
    //init recv thread
    server_context->recv_thread = NULL;
    server_context->recv_thread = (pthread_t *)malloc(sizeof(pthread_t));
    TEST_NZ(pthread_create(server_context->recv_thread, NULL, rdmaft_recv_thread_func, server_context));

    return server_context;
}

void rdmaft_stop_recv(struct rdmaft_recv_server_context* server_context) 
{
    rdma_destroy_id(server_context->listener);
    rdma_destroy_event_channel(server_context->ec);
}

void rdmaft_reset_recv_buffer_size(size_t buffer_size) {
    if (buffer_size != 0) 
        BUFFER_SIZE = buffer_size;
}