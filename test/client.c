/**
 * @author dai minglong
 * @date 2019.4
*/
# include <rdmaft_send.h>

void send_cb_func(const char* filename) {
    printf("send file %s done!\n", filename);
}

int main(int argc, char **argv) {

    rdmaft_start_send(argv[1], "15000", "test-file", 10*1024*1024, send_cb_func);
    while(1);

    return 0;
}
