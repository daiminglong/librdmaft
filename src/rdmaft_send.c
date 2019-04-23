/**
 * @author dai minglong
 * @date 2019.4
*/
#include <rdmaft_common.h>
#include <rdmaft_send.h>

struct conn_context {

    //data buffer & registred memory
    char *buffer;
    struct ibv_mr *buffer_mr;

    //message buffer & registerdd memory
    struct message *msg;
    struct ibv_mr *msg_mr;

    //peer's memory address & memory key
    uint64_t peer_addr;
    uint32_t peer_rkey;

    // file descripter & file name of file need to be send
    int fd;
    const char *file_name;
    rdmaft_send_cb cb_func;
};

/* function used to write remote mem */
static void write_remote(struct rdma_cm_id *id, uint32_t len)
{
    struct conn_context *ctx = (struct conn_context *)id->context;

    struct ibv_send_wr wr, *bad_wr = NULL;
    struct ibv_sge sge;

    memset(&wr, 0, sizeof(wr));

    wr.wr_id = (uintptr_t)id;
    wr.opcode = IBV_WR_RDMA_WRITE_WITH_IMM;
    wr.send_flags = IBV_SEND_SIGNALED;
    wr.imm_data = htonl(len);
    wr.wr.rdma.remote_addr = ctx->peer_addr;
    wr.wr.rdma.rkey = ctx->peer_rkey;

    if (len) {
        wr.sg_list = &sge;
        wr.num_sge = 1;

        sge.addr = (uintptr_t)ctx->buffer;
        sge.length = len;
        sge.lkey = ctx->buffer_mr->lkey;
    }

    TEST_NZ(ibv_post_send(id->qp, &wr, &bad_wr));
}

/* function used to post a receive */
static void post_receive(struct rdma_cm_id *id)
{
    struct conn_context *ctx = (struct conn_context *)id->context;

    struct ibv_recv_wr wr, *bad_wr = NULL;
    struct ibv_sge sge;

    memset(&wr, 0, sizeof(wr));

    wr.wr_id = (uintptr_t)id;
    wr.sg_list = &sge;
    wr.num_sge = 1;

    sge.addr = (uintptr_t)ctx->msg;
    sge.length = sizeof(*ctx->msg); 
    sge.lkey = ctx->msg_mr->lkey;


    TEST_NZ(ibv_post_recv(id->qp, &wr, &bad_wr));
}

/* function used to send next file chunk */
static void send_next_chunk(struct rdma_cm_id *id)
{
    struct conn_context *ctx = (struct conn_context *)id->context;

    ssize_t size = 0;

    size = read(ctx->fd, ctx->buffer, BUFFER_SIZE);

    if (size == -1)
        rc_die("read() failed\n");

    write_remote(id, size);
}

/* function usde to send the file name to the receiver */
static void send_file_name(struct rdma_cm_id *id)
{
    struct conn_context *ctx = (struct conn_context *)id->context;

    strcpy(ctx->buffer, ctx->file_name);

    write_remote(id, strlen(ctx->file_name) + 1);
}

/* send pre handle */
static void on_pre_conn(struct rdma_cm_id *id)
{
    struct conn_context *ctx = (struct conn_context *)id->context;

    //regist buffer memory 
    //memalign to prevent page in/out 
    // posix_memalign((void **)&ctx->buffer, sysconf(_SC_PAGESIZE), BUFFER_SIZE);
    // TEST_Z(ctx->buffer_mr = ibv_reg_mr(rc_get_pd(), ctx->buffer, BUFFER_SIZE, IBV_ACCESS_LOCAL_WRITE));

    //regist msg memory
    posix_memalign((void **)&ctx->msg, sysconf(_SC_PAGESIZE), sizeof(*ctx->msg));
    TEST_Z(ctx->msg_mr = ibv_reg_mr(rc_get_pd(), ctx->msg, sizeof(*ctx->msg), IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE));

    post_receive(id);
}

/*function used to handle completion queue event*/
static void on_completion(struct ibv_wc *wc)
{
    struct rdma_cm_id *id = (struct rdma_cm_id *)(uintptr_t)(wc->wr_id);
    struct conn_context *ctx = (struct conn_context *)id->context;

    //only hanle receive complete event
    if (wc->opcode & IBV_WC_RECV) {
        if (ctx->msg->id == MSG_MR) {

            //set remote memory addr & key
            ctx->peer_addr = ctx->msg->data.mr.addr;
            ctx->peer_rkey = ctx->msg->data.mr.rkey;
            BUFFER_SIZE = (BUFFER_SIZE < (ctx->msg->data.mr.buffer_size))?BUFFER_SIZE:(ctx->msg->data.mr.buffer_size);

            posix_memalign((void **)&ctx->buffer, sysconf(_SC_PAGESIZE), BUFFER_SIZE);
            TEST_Z(ctx->buffer_mr = ibv_reg_mr(rc_get_pd(), ctx->buffer, BUFFER_SIZE, IBV_ACCESS_LOCAL_WRITE));

            send_file_name(id);
        } else if (ctx->msg->id == MSG_READY) {
            send_next_chunk(id);
        } else if (ctx->msg->id == MSG_DONE) {
            if (ctx->cb_func != NULL) {
                ctx->cb_func(ctx->file_name);
            }
            rc_disconnect(id);
            return;
        }

        post_receive(id);
    }
}

/* function used to handle connection disconnected */
static void on_disconnect(struct rdma_cm_id *id)
{
    struct conn_context *ctx = (struct conn_context *)id->context;

    //close file
    close(ctx->fd);
}

void *rdmaft_send_thread_func(void *client_context) 
{
    rc_send_loop((struct rdmaft_send_client_context*)client_context);
    return NULL;
}

struct rdmaft_send_client_context* 
    rdmaft_start_send(char* server_addr, char* port, char* filename, size_t buffer_size, rdmaft_send_cb func) 
{
    
    struct conn_context ctx;

    ctx.file_name = basename(filename);//get the file name
    
    ctx.fd = open(filename, O_RDONLY);
    if (ctx.fd == -1) {
        fprintf(stderr, "unable to open input file \"%s\"\n", ctx.file_name);
        rc_die("openfile failed");
    }

    if (buffer_size != 0) {
        BUFFER_SIZE = buffer_size;
    }

    //init call back function
    ctx.cb_func = func;

    //init event handle function pointer
    rc_init(
        on_pre_conn,
        NULL,
        on_completion,
        on_disconnect);


    struct rdmaft_send_client_context *client_context = 
        (struct rdmaft_send_client_context*)malloc(sizeof(struct rdmaft_send_client_context));

    client_context->conn = NULL;
    client_context->ec = NULL;

    struct addrinfo *addr;
    TEST_NZ(getaddrinfo(server_addr, port, NULL, &addr));

    //create event channel
    TEST_Z(client_context->ec = rdma_create_event_channel());

    //create channel id(conn)
    TEST_NZ(rdma_create_id(client_context->ec, &(client_context->conn), NULL, RDMA_PS_TCP));
    
    //resolve addr
    TEST_NZ(rdma_resolve_addr(client_context->conn, NULL, addr->ai_addr, TIMEOUT_IN_MS));
    freeaddrinfo(addr);

    (client_context->conn)->context = (void*)(&ctx);

    //init send thread
    client_context->send_thread = NULL;

    client_context->send_thread = (pthread_t *)malloc(sizeof(pthread_t));
    TEST_NZ(pthread_create(client_context->send_thread, NULL, rdmaft_send_thread_func, client_context));

    return client_context;
}

void rdmaft_stop_send(struct rdmaft_send_client_context *client_context)
{
    //to-do
}
