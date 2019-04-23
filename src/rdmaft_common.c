/**
 * @author dai minglong
 * @date 2019.4
*/
#include <rdmaft_common.h>


/* rdma connection context structure */
struct context {
    struct ibv_context *ctx;
    struct ibv_pd *pd;
    struct ibv_cq *cq;
    struct ibv_comp_channel *comp_channel;

    pthread_t cq_poller_thread;
};

static struct context *s_ctx = NULL;
static pre_conn_cb_fn s_on_pre_conn_cb = NULL;
static connect_cb_fn s_on_connect_cb = NULL;
static completion_cb_fn s_on_completion_cb = NULL;
static disconnect_cb_fn s_on_disconnect_cb = NULL;

static void build_context(struct ibv_context *verbs);
static void build_qp_attr(struct ibv_qp_init_attr *qp_attr);
static void event_loop(struct rdma_event_channel *ec, int exit_on_disconnect);
static void * poll_cq(void *);


void rc_init(pre_conn_cb_fn pc, connect_cb_fn conn, completion_cb_fn comp, disconnect_cb_fn disc)
{
    s_on_pre_conn_cb = pc;
    s_on_connect_cb = conn;
    s_on_completion_cb = comp;
    s_on_disconnect_cb = disc;
}

void rc_disconnect(struct rdma_cm_id *id)
{
    rdma_disconnect(id);
}

void rc_die(const char *reason)
{
    fprintf(stderr, "%s\n", reason);
    exit(EXIT_FAILURE);
}


struct ibv_pd * rc_get_pd()
{
    return s_ctx->pd;
}

/* function used to build a rdma connection */
void build_connection(struct rdma_cm_id *id)
{
    struct ibv_qp_init_attr qp_attr;

    //set context
    build_context(id->verbs);

    //set queue pair attr
    build_qp_attr(&qp_attr);

    //create rdma queue pair with queue pair attr
    TEST_NZ(rdma_create_qp(id, s_ctx->pd, &qp_attr));
}

/* function used to create & init context */
void build_context(struct ibv_context *verbs)
{
    if (s_ctx) {
        if (s_ctx->ctx != verbs)
        rc_die("cannot handle events in more than one context.");
        return;
    }

    s_ctx = (struct context *)malloc(sizeof(struct context));
    s_ctx->ctx = verbs;

    //create a protection domain for rdma device context
    TEST_Z(s_ctx->pd = ibv_alloc_pd(s_ctx->ctx));

    //create a completion channel
    TEST_Z(s_ctx->comp_channel = ibv_create_comp_channel(s_ctx->ctx));

    //create a completion queue, cqe=10 is arbitrary
    TEST_Z(s_ctx->cq = ibv_create_cq(s_ctx->ctx, 10, NULL, s_ctx->comp_channel, 0));
    
    //tell CA(net device's channel adapter) to send event when WC(work completion) enter CQ(completion queue)
    TEST_NZ(ibv_req_notify_cq(s_ctx->cq, 0));
     
    //create a completion queue poll thread.
    TEST_NZ(pthread_create(&s_ctx->cq_poller_thread, NULL, poll_cq, NULL));
}

/* cq_poller_thread's task function */
void * poll_cq(void *ctx)
{
    struct ibv_cq *cq;
    struct ibv_wc wc;//work completion

    while (1) {
        
        //get event from completion queue, if no event then blocked, to eliminate busy loop
        TEST_NZ(ibv_get_cq_event(s_ctx->comp_channel, &cq, &ctx));

        //ack for a notified event
        ibv_ack_cq_events(cq, 1);

        //tell CA to send an event when next WC enters CQ
        TEST_NZ(ibv_req_notify_cq(cq, 0));

        while (ibv_poll_cq(cq, 1, &wc)) {
            
            if (wc.status == IBV_WC_SUCCESS)
                s_on_completion_cb(&wc);
            else
                rc_die("poll_cq: status is not IBV_WC_SUCCESS");
        }
    }
    return NULL;
}

/* funtion used to set queue pair attr */
void build_qp_attr(struct ibv_qp_init_attr *qp_attr)
{
    memset(qp_attr, 0, sizeof(*qp_attr));

    qp_attr->send_cq = s_ctx->cq;
    qp_attr->recv_cq = s_ctx->cq;
    qp_attr->qp_type = IBV_QPT_RC;

    qp_attr->cap.max_send_wr = 10;
    qp_attr->cap.max_recv_wr = 10;
    qp_attr->cap.max_send_sge = 1;
    qp_attr->cap.max_recv_sge = 1;
}

/* function used to set all connection's params settings */
void build_params(struct rdma_conn_param *params)
{
    memset(params, 0, sizeof(*params));

    params->initiator_depth = params->responder_resources = 1;
    params->rnr_retry_count = 7; /* infinite retry */
}

/* send loop */
void rc_send_loop(struct rdmaft_send_client_context* context)
{
    struct rdmaft_send_client_context* client_context = context;

    struct rdma_cm_event *event = NULL;
    struct rdma_conn_param cm_params;

    //build connection params
    build_params(&cm_params);

    //events handle loop start
    while (rdma_get_cm_event(client_context->ec, &event) == 0) {

        //copy that event for next step handle & ack that event to release mem
        struct rdma_cm_event event_copy;
        memcpy(&event_copy, event, sizeof(*event));
        rdma_ack_cm_event(event);

        if (event_copy.event == RDMA_CM_EVENT_ADDR_RESOLVED) {
            
            //build a new connection
            build_connection(event_copy.id);

            //pre handle
            s_on_pre_conn_cb(event_copy.id);

            //reslove route
            TEST_NZ(rdma_resolve_route(event_copy.id, TIMEOUT_IN_MS));

        } else if (event_copy.event == RDMA_CM_EVENT_ROUTE_RESOLVED) {

            //connect to the other end
            TEST_NZ(rdma_connect(event_copy.id, &cm_params));

        } else if (event_copy.event == RDMA_CM_EVENT_ESTABLISHED) {
            //do-nothing
        } else if (event_copy.event == RDMA_CM_EVENT_DISCONNECTED) {

            //destroy queue pair
            rdma_destroy_qp(event_copy.id);
            //destroy id
            rdma_destroy_id(event_copy.id);
            //destroy event channel
            rdma_destroy_event_channel(client_context->ec);
            break;

        } else {//other unknown event handle
            rc_die("unknown event");
        }
    }
    //event handle loop end
}

/* recv loop */
void rc_recv_loop(struct rdmaft_recv_server_context* context)
{
    struct rdmaft_recv_server_context* server_context = context;
    struct rdma_cm_event* event = NULL;
    struct rdma_conn_param cm_params;

    //build params
    build_params(&cm_params);

    //event handle loop start
    while (rdma_get_cm_event(server_context->ec, &event) == 0) {

        //copy that event for next step handle & ack that event to release mem
        struct rdma_cm_event event_copy;
        memcpy(&event_copy, event, sizeof(*event));
        rdma_ack_cm_event(event);

        if (event_copy.event == RDMA_CM_EVENT_CONNECT_REQUEST) {

            //build a new connection
            build_connection(event_copy.id);
            //pre handle
            s_on_pre_conn_cb(event_copy.id);
            //accept this new connection
            TEST_NZ(rdma_accept(event_copy.id, &cm_params));

        } else if (event_copy.event == RDMA_CM_EVENT_ESTABLISHED) {

            //handle connected
            s_on_connect_cb(event_copy.id);

        } else if (event_copy.event == RDMA_CM_EVENT_DISCONNECTED) {

            //destroy queue pair
            rdma_destroy_qp(event_copy.id);
            //disconnect handle
            s_on_disconnect_cb(event_copy.id);
            //destroy id
            rdma_destroy_id(event_copy.id);

        } else {//ohter unknown events handle
            rc_die("unknown event\n");
        }
    }

    rdma_destroy_id(server_context->listener);
    rdma_destroy_event_channel(server_context->ec);
}


