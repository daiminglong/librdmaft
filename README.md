# RDMA file transfer library  
## Intro
This librdmaft is a toy C lib used to transfer file through RDMA based on RoCE. It is easy to be used to develop a demo.

## Dependency  
+ librdmacm: [https://github.com/ofiwg/librdmacm](https://github.com/ofiwg/librdmacm)  
+ pthread  
+ ibverbs

## Build & Install Lib 
This proj is a CMake-Based proj, you need to install cmake first & build with command line below.  
```
$> cd ${path-to-proj}/librdmaft  
$> mkdir build
$> cd build  
$> cmake ..
$> make
$> make install
```

## API Intro  

### (1) rdmaft_start_send() 

Description：API used to start a asynchronous rdma file send client & start send file.    

Usage:
```
struct rdmaft_send_client_context* rdmaft_start_send(char* server_addr, char* port, char* filename, size_t buffer_size, rdmaft_send_cb func);

args:
    server_addr: file receive server's IP address.  
    port: file receive's RDMA listen port.  
    filename: file need to be transfered.  
    buffer_size: rdma send memory buffer size
    func: transfer finished callback function. 
return:
    rdmaft control struct of the send side defined below.

callback function pointer of the send side:  
typedef void (*rdmaft_send_cb)(const char* filename);

rdmaft control struct of the send side:  
struct rdmaft_send_client_context {
    struct rdma_cm_id *conn;// id related to librdmacm
    struct rdma_event_channel *ec;// rdma event channel
    pthread_t* send_thread;// sub thread for send client.
};
```  

### (2) rdmaft_start_recv()  
  
Description：API used to start a asynchronous rdma file recv server & start recv file.   

Usage:
```  
struct rdmaft_recv_server_context* rdmaft_start_recv(char* port, char* recv_dir, size_t buffer_size, rdmaft_recv_cb func);  

args:
    port: RDMA listen port.  
    recv_dir: receiver directory for receive a file.  
    buffer_size: rdma recv memory buffer size
    func: transfer finished callback function. 
return:
    rdmaft control struct of the recv side defined below.

callback function pointer of the recv side:  
typedef void (*rdmaft_recv_cb)(const char* filename); 

rdmaft control struct of the recv side:  
struct rdmaft_recv_server_context {
    struct rdma_cm_id* listener;// id related to librdmacm
    struct rdma_event_channel* ec;// rdma event channel
    pthread_t* recv_thread;// sub thread for receive server.
};
```

### (3) rdmaft_stop_recv()  
  
Description：API used to stop a rdma file recv server.   

Usage:
```
void rdmaft_stop_recv(struct rdmaft_recv_server_context* context);  

args: 
    context: rdmaft recv server context.
```


### (4) rdmaft_reset_recv_buffer_size()  
  
Description：API used to reset rdma file recv server' rdma recv memory buffer.   
Usage:
```
void rdmaft_reset_recv_buffer_size(size_t buffer_size);

args: 
    buffer_size: new buffer size for all recv rdma file recv server.
```

## Examples  
There are an example client.c & server.c for you in the test directory. you can build & run the example.  
```
$> cd test
$> make  

server side: 
$> ./server

client side:
$> ./client <server_ip>
```

P.S.: client & server side will use the same buffer size that of the minimum buffer size set by both client side & server side.

## Opinions
Your opinions to daiminglong94 At gmail DOT com
