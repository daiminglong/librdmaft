/**
 * @author dai minglong
 * @date 2019.4
*/
#include <rdmaft_recv.h>
#include <unistd.h>

void recv_cb_func(const char* filename) {
    printf("recv file %s done!\n", filename);
}

int main(int argc, char **argv) {

    struct rdmaft_recv_server_context* context = rdmaft_start_recv("15000", "./", 10*1024*1024, recv_cb_func);

    sleep(5);

    rdmaft_stop_recv(context);

    while(1);

    return 0;
}
