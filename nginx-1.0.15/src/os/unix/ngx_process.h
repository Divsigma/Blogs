
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_PROCESS_H_INCLUDED_
#define _NGX_PROCESS_H_INCLUDED_


#include <ngx_setproctitle.h>


typedef pid_t       ngx_pid_t;

#define NGX_INVALID_PID  -1

typedef void (*ngx_spawn_proc_pt) (ngx_cycle_t *cycle, void *data);

typedef struct {
    // 进程id
    ngx_pid_t           pid;
    // 由waitpid(2)获取的进程状态
    int                 status;
    // 由socketpair(2)赋予的父子进程IPC句柄
    ngx_socket_t        channel[2];

    // 子进程工作循环方法，
    // 如void ngx_worker_process_cycle(ngx_cycle_t*, void *data)
    ngx_spawn_proc_pt   proc;
    // 等价于void ngx_worker_process_cycle(ngx_cycle_t*, void *data)
    // 第二个参数
    void               *data;
    // 进程名字，
    // 操作系统中显示的进程名与此相同
    char               *name;

    // =1表示正在重新生成子进程
    unsigned            respawn:1;
    unsigned            just_spawn:1;
    // =1表示正在分离父子进程
    unsigned            detached:1;
    unsigned            exiting:1;
    unsigned            exited:1;
} ngx_process_t;


typedef struct {
    char         *path;
    char         *name;
    char *const  *argv;
    char *const  *envp;
} ngx_exec_ctx_t;


// 最多有1024个进程，
// 即ngx_processes[]最多有1024个元素
#define NGX_MAX_PROCESSES         1024

#define NGX_PROCESS_NORESPAWN     -1
#define NGX_PROCESS_JUST_SPAWN    -2
#define NGX_PROCESS_RESPAWN       -3
#define NGX_PROCESS_JUST_RESPAWN  -4
#define NGX_PROCESS_DETACHED      -5


#define ngx_getpid   getpid

#ifndef ngx_log_pid
#define ngx_log_pid  ngx_pid
#endif

ngx_pid_t ngx_spawn_process(ngx_cycle_t *cycle,
    ngx_spawn_proc_pt proc, void *data, char *name, ngx_int_t respawn);
ngx_pid_t ngx_execute(ngx_cycle_t *cycle, ngx_exec_ctx_t *ctx);
ngx_int_t ngx_init_signals(ngx_log_t *log);
void ngx_debug_point(void);


#if (NGX_HAVE_SCHED_YIELD)
#define ngx_sched_yield()  sched_yield()
#else
#define ngx_sched_yield()  usleep(1)
#endif


extern int            ngx_argc;
extern char         **ngx_argv;
extern char         **ngx_os_argv;

extern ngx_pid_t      ngx_pid;
extern ngx_socket_t   ngx_channel;
// fork(2)之后，当前进程在ngx_processes[]数组中的下标
extern ngx_int_t      ngx_process_slot;
// ngx_processes[]数组中有意义的ngx_process_t结构体的最大下标
extern ngx_int_t      ngx_last_process;
// 全局数组，存储所有子进程信息，
// 虽然fork(2)之后，子进程也会有一个所有进程信息的数组，
// 但这个数组仅仅是给master进程使用的，
// 父子进程通信是通过ngx_process_t中的channel[2]进行的
extern ngx_process_t  ngx_processes[NGX_MAX_PROCESSES];


#endif /* _NGX_PROCESS_H_INCLUDED_ */
