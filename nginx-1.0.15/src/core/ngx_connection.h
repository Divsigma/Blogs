
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_CONNECTION_H_INCLUDED_
#define _NGX_CONNECTION_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


typedef struct ngx_listening_s  ngx_listening_t;

struct ngx_listening_s {
    ngx_socket_t        fd;

    struct sockaddr    *sockaddr;
    socklen_t           socklen;    /* size of sockaddr */
    size_t              addr_text_max_len;
    ngx_str_t           addr_text;

    int                 type;

    int                 backlog;
    int                 rcvbuf;
    int                 sndbuf;

    /* handler of accepted connection */
    // 如，从监听对象建立新的HTTP连接需要经过accept和将初始化新连接的操作，
    // 该handler就是负责accept后初始化新连接，
    // 在HTTP框架中，该handler = ngx_http_init_connection，
    // 会负责申请内存、将新连接读写事件加入epoll等。
    ngx_connection_handler_pt   handler;

    void               *servers;  /* array of ngx_http_in_addr_t, for example */

    ngx_log_t           log;
    ngx_log_t          *logp;

    size_t              pool_size;
    /* should be here because of the AcceptEx() preread */
    size_t              post_accept_buffer_size;
    /* should be here because of the deferred accept */
    ngx_msec_t          post_accept_timeout;

    ngx_listening_t    *previous;
    // 当前监听对象对应的连接结构体，监听对象需要使用该连接结构体中的读写事件，
    // 如设置结构体ngx_connection_t中的读事件回调为函数void ngx_event_accept(ngx_event_t*)，
    // 以完成从全连接队列接收新连接（在内核创建fd）的工作
    ngx_connection_t   *connection;

    unsigned            open:1;
    unsigned            remain:1;
    unsigned            ignore:1;

    unsigned            bound:1;       /* already bound */
    unsigned            inherited:1;   /* inherited from previous process */
    unsigned            nonblocking_accept:1;
    unsigned            listen:1;
    unsigned            nonblocking:1;
    unsigned            shared:1;    /* shared between threads or processes */
    unsigned            addr_ntop:1;

#if (NGX_HAVE_INET6 && defined IPV6_V6ONLY)
    unsigned            ipv6only:2;
#endif

#if (NGX_HAVE_DEFERRED_ACCEPT)
    unsigned            deferred_accept:1;
    unsigned            delete_deferred:1;
    unsigned            add_deferred:1;
#ifdef SO_ACCEPTFILTER
    char               *accept_filter;
#endif
#endif
#if (NGX_HAVE_SETFIB)
    int                 setfib;
#endif

};


typedef enum {
     NGX_ERROR_ALERT = 0,
     NGX_ERROR_ERR,
     NGX_ERROR_INFO,
     NGX_ERROR_IGNORE_ECONNRESET,
     NGX_ERROR_IGNORE_EINVAL
} ngx_connection_log_error_e;


typedef enum {
     NGX_TCP_NODELAY_UNSET = 0,
     NGX_TCP_NODELAY_SET,
     NGX_TCP_NODELAY_DISABLED
} ngx_connection_tcp_nodelay_e;


typedef enum {
     NGX_TCP_NOPUSH_UNSET = 0,
     NGX_TCP_NOPUSH_SET,
     NGX_TCP_NOPUSH_DISABLED
} ngx_connection_tcp_nopush_e;


#define NGX_LOWLEVEL_BUFFERED  0x0f
#define NGX_SSL_BUFFERED       0x01

// 被动连接
struct ngx_connection_s {
    // （1）未使用时，指向连接池中下一个空闲连接（相当于next指针）；
    // （2）新连接建立后，指向Nginx模块所需的上下文，
    //      如HTTP框架中则指向结构体ngx_http_request_t，
    //      所以处理新HTTP连接的回调函数ngx_http_init_request中，
    //      会有类似c->data = c->data->request的操作，
    //      就是将next指针“转为”上下文指针
    void               *data;
    // 连接对应的读事件，指向结构体ngx_cycle_t中数组read_events的某个元素（即空间预分配了）
    ngx_event_t        *read;
    // 连接对应的写事件，指向结构体ngx_cycle_t中数组write_events的某个元素
    ngx_event_t        *write;

    ngx_socket_t        fd;

    // 接收网络字符流的方法，该方法中一般会调用recv(2)并重置读事件的ready位为0，
    // 如函数ssize_t ngx_unix_recv(ngx_connection_t *c, u_char *buf, size_t size)
    ngx_recv_pt         recv;
    // 直接发送网络字符流的方法，该方法中一般会调用send(2)并重置写事件的ready位为0，
    // 如函数ssize_t ngx_unix_send(ngx_connection_t *c, u_char *buf, size_t size)
    ngx_send_pt         send;
    // 直接批量接收网络字符流的方法，一般用于异步接收请求包体，该方法一般会调用readv(2)，
    // 如函数ssize_t ngx_readv_chain(ngx_connection_t *c, ngx_chain_t *chain)
    ngx_recv_chain_pt   recv_chain;
    // 直接批量发送网络字符流的方法，一般用于异步发送响应，该方法一般会调用writev(2)，
    // 如函数ngx_chain_t* ngx_writev_chain(ngx_connection_t *c, ngx_chain_t *in, off_t limit)
    ngx_send_chain_pt   send_chain;

    // 新连接对象都由某个监听对象产生
    ngx_listening_t    *listening;

    off_t               sent;

    ngx_log_t          *log;

    ngx_pool_t         *pool;

    struct sockaddr    *sockaddr;
    socklen_t           socklen;
    ngx_str_t           addr_text;

#if (NGX_SSL)
    ngx_ssl_connection_t  *ssl;
#endif

    struct sockaddr    *local_sockaddr;

    ngx_buf_t          *buffer;

    ngx_queue_t         queue;

    ngx_atomic_uint_t   number;

    ngx_uint_t          requests;

    unsigned            buffered:8;

    unsigned            log_error:3;     /* ngx_connection_log_error_e */

    unsigned            single_connection:1;
    unsigned            unexpected_eof:1;
    unsigned            timedout:1;
    unsigned            error:1;
    unsigned            destroyed:1;

    unsigned            idle:1;
    unsigned            reusable:1;
    unsigned            close:1;

    unsigned            sendfile:1;
    unsigned            sndlowat:1;
    unsigned            tcp_nodelay:2;   /* ngx_connection_tcp_nodelay_e */
    unsigned            tcp_nopush:2;    /* ngx_connection_tcp_nopush_e */

#if (NGX_HAVE_IOCP)
    unsigned            accept_context_updated:1;
#endif

#if (NGX_HAVE_AIO_SENDFILE)
    unsigned            aio_sendfile:1;
    ngx_buf_t          *busy_sendfile;
#endif

#if (NGX_THREADS)
    ngx_atomic_t        lock;
#endif
};


ngx_listening_t *ngx_create_listening(ngx_conf_t *cf, void *sockaddr,
    socklen_t socklen);
ngx_int_t ngx_set_inherited_sockets(ngx_cycle_t *cycle);
ngx_int_t ngx_open_listening_sockets(ngx_cycle_t *cycle);
void ngx_configure_listening_sockets(ngx_cycle_t *cycle);
void ngx_close_listening_sockets(ngx_cycle_t *cycle);
void ngx_close_connection(ngx_connection_t *c);
ngx_int_t ngx_connection_local_sockaddr(ngx_connection_t *c, ngx_str_t *s,
    ngx_uint_t port);
ngx_int_t ngx_connection_error(ngx_connection_t *c, ngx_err_t err, char *text);

ngx_connection_t *ngx_get_connection(ngx_socket_t s, ngx_log_t *log);
void ngx_free_connection(ngx_connection_t *c);

void ngx_reusable_connection(ngx_connection_t *c, ngx_uint_t reusable);

#endif /* _NGX_CONNECTION_H_INCLUDED_ */
