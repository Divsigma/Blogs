
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
    // ����id
    ngx_pid_t           pid;
    // ��waitpid(2)��ȡ�Ľ���״̬
    int                 status;
    // ��socketpair(2)����ĸ��ӽ���IPC���
    ngx_socket_t        channel[2];

    // �ӽ��̹���ѭ��������
    // ��void ngx_worker_process_cycle(ngx_cycle_t*, void *data)
    ngx_spawn_proc_pt   proc;
    // �ȼ���void ngx_worker_process_cycle(ngx_cycle_t*, void *data)
    // �ڶ�������
    void               *data;
    // �������֣�
    // ����ϵͳ����ʾ�Ľ����������ͬ
    char               *name;

    // =1��ʾ�������������ӽ���
    unsigned            respawn:1;
    unsigned            just_spawn:1;
    // =1��ʾ���ڷ��븸�ӽ���
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


// �����1024�����̣�
// ��ngx_processes[]�����1024��Ԫ��
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
// fork(2)֮�󣬵�ǰ������ngx_processes[]�����е��±�
extern ngx_int_t      ngx_process_slot;
// ngx_processes[]�������������ngx_process_t�ṹ�������±�
extern ngx_int_t      ngx_last_process;
// ȫ�����飬�洢�����ӽ�����Ϣ��
// ��Ȼfork(2)֮���ӽ���Ҳ����һ�����н�����Ϣ�����飬
// �������������Ǹ�master����ʹ�õģ�
// ���ӽ���ͨ����ͨ��ngx_process_t�е�channel[2]���е�
extern ngx_process_t  ngx_processes[NGX_MAX_PROCESSES];


#endif /* _NGX_PROCESS_H_INCLUDED_ */
