[TOC]



# 前言

参考《深入理解Nginx-模块开发与架构解析》（第二版，陶辉著）整理，我的Nginx学习笔记。尝试自顶向下。



# 1 编写第一个HTTP模块

## 1.1 编译Nginx

```
./configure
make
make install
```

## 1.2 `./configure`阅读顺序（可以用`make -d`跟踪）：

- `auto/init`：定义变量表示ngx_modules[]数组文件所在位置（$NGX_MODULES_C）、Nginx的Makefile所在位置（$NGX_MAKEFILES）
- `auto/options`：解析传入`./configure`的参数`--add-module=PATH`，将`PATH`加入$NGX_ADDONS（按空格分割）
- `auto/modules`：执行$NGX_ADDONS包含的路径下的config文件，向$NGX_MODULES_C写入ngx_modules[]数组
- 各个自定义模块的config文件：用户自定义，需要给定模块名称（用于`./configure`输出显示信息）、修改相应的模块类型的变量（如$HTTP_MODULES，用于后续将模块加入ngx_modules[]数组）、添加$NGX_ADDON_SRCS
- `auto/make`：根据config文件定义的$NGX_ADDON_SRCS生成响应的make指令，向$NGX_MAKEFILES生成Makefile文件

- 执行`./configure`后，产生的Makefile默认在`objs/`下，在原目录执行的`make build`和`make install`其实是：

  ```shell
  $(MAKE) -f objs/Makefile <param>
  ```

## 1.3 需要了解的数据结构和模块类型文件

- `src/core/ngx_string.h`：ngx_str_t字符串
- `src/core/ngx_list.h`：管理内存的链表，是一个分块数组链表
- `src/core/ngx_hash.h`：键值对类型
- `src/core/ngx_buf.h`：描述内存和磁盘文件数据（比如ngx_http_request_s中未解析的头部指针ngx_buf_t *header_in，其实是接收HTTP头部的缓冲区）
- `src/core/ngx_conf_file.h`：Nginx模块（ngx_modules_s）和其中配置项的结构（ngx_command_s）
- `src/http/ngx_http_config.h`：HTTP类型的模块结构（ngx_http_module_t）

- P85：Nginx在解析配置文件（`nginx.conf`）中的一个配置项时：首先会遍历所有模块（ngx_modules_s），对每一个模块而言，即通过遍历ngx_command_s数组进行，另外，在数组中检查到ngx_null_command时，会停止使用当前模块解析该配置项（P23：而一个请求同时符合多个模块处理规则时，Nginx会按顺序选择最靠前的模块优先处理）
- <span style="color:red">问题1</span>：配置项与模块对应关系是多对多？如何保证nginx.conf中一个多次出现的配置项名没有多义性？如果有多义性，如何给参数，Nginx又如何根据调用方式找到处理这种调用方式下的这个配置项的模块？

## 1.4 代码

### 1.4.1 处理用户请求

- 机制：考虑模块在NGX_HTTP_CONTENT_PHASE阶段介入（Nginx的HTTP框架中处理请求体的阶段）。在该阶段介入HTTP请求处理有两种方式，代码演示的是通过设置匹配的loc配置块的回调函数来实现的。Nginx的HTTP框架将处理请求分为11个阶段（注意此时已完成请求接收），在其执行到NGX_HTTP_CONTENT_PHASE阶段时，会去调用与请求行匹配的loc配置块的回调函数——即我们在配置解析函数中定义的`typedef ngx_int_t (*ngx_http_handler_pt)(hgx_http_request_t *r);`。该函数就是我们可以自定义的处理函数。**该函数虽包含了处理请求和发送响应的过程（P103），但它们都是异步的（尝试1次后把未完成工作交给后台）**。
- P94：Nginx对内存控制相当严格，比如ngx_str_t类型的设计、一个request结构体就存了request_start和request_end和method_end三个指针（method_name.data和request_start其实指向同一个内存地址，而不是字符串地址），谨防Nginx里将u_char*当作字符串使用
- P91~92、P97：事实上，模块的回调函数由ngx_http_finalize_request调用，它根据模块回调函数的返回值决定后续行为。当content_handler返回NGX_DONE时，控制权会被转接回Nginx。例如回调函数使用了ngx_http_read_client_request_body来异步读取HTTP包体时，回调函数调用异步读取包体函数后，直接返回NGX_DONE防止worker进程阻塞。包体读取完毕后，通过注册到ngx_http_read_client_request_body的回调函数完成后续处理（注意该回调函数就需要主动调用ngx_http_finalize_request来结束请求，配合11章内容看）。<span style="color:red">（问题4）如何跳回到后续处理（通过ngx_http_request_s中的上下文ctx？P151）？需要模块回调函数ngx_http_read_client_request_body后立刻就返回？</span>
- 结合11章的内容：Nginx高性能的秘诀之一，似乎是其异步并发的机制。Nginx的异步并发机制应该是通过epoll事件驱动和请求的引用计数实现的。Nginx将对同一个请求的处理划分为多个独立的事件（如接收包体、发送响应头、发送响应包体），这些事件每触发一次就会尝试一次网络IO，然后根据recv/send的返回值决定是否再次加入epoll监听该事件。基本思想就是，Nginx会将无法一次性完成的操作抽象为独立的事件，借由IO多路复用机制完成事件多次调度，这样能防止Nginx进程阻塞在某一IO上（比如TCP写缓存满了或读缓存空了），提高了Nginx效率。

### 1.4.2 发送响应

- 机制：在模块中定义的回调函数ngx_http_handler_pt中处理，分为发送头部（ngx_http_send_header()）和发送包体（ngx_http_output_filter()）两步。注意Nginx是全异步的服务器，ngx_http_output_filter返回时可能TCP缓冲区还不可写，所以ngx_http_handler_pt中需要使用内存池及其接口生成发送包体的内容，而不能使用进程栈或堆（P102）。

### 1.4.3 如何配置`nginx.conf`

- HTTP的配置模型（看书总结，未源码跟踪）

  - 阶段1-触发HTTP配置解析：通过`nginx.conf`中的http{}块触发HTTP配置框架，配置文件解析器从http{}块开始，递归地解析http{}及其中的配置块。

  - 阶段2-为**所有**HTTP模块申请当前配置块下存放配置值的空间：解析过程中，碰到http{}块、server{}块和location{}块时，都会先创建并初始化一个ngx_http_conf_ctx_t类型的数组指针目录（见下述代码块）。该目录有3个级别——main、srv和loc，各级目录项分别**指向**一个数组，数组元素指向各个HTTP模块用来存放后续解析到的配置值的结构体。而一个HTTP模块的各级结构体分别由自定义的create_main_conf、create_srv_conf和create_loc_conf方法创建（注意此时只是分配了结构体空间，并未解析配置，create嘛，顾名思义），所以阶段2的工作其实是在生成数组指针的目录后，**根据当前配置块级别**，调用所有HTTP模块的三个分配结构体的方法，将返回的结构体指针保存到各级目录指向的数组中。（即server{}块下没有main级别目录、loc{}块下没有main和srv级别目录）

    ```c
    typedef struct {
        void **main_conf;
        void **srv_conf;
        void **loc_conf;
    } ngx_http_conf_ctx_t;
    ```

    为什么http{}块要有各模块srv和loc级别的配置、server{}块要有各模块loc级别的配置呢？因为一个http{}块会有多个server{}块、一个server{}块会有多个loc{}块，为了达到效果——1个HTTP**模块**的1个**配置项**，在**外层配置块**中的**配置值**能默认适用于**内层配置块**中**该HTTP模块对应的配置项**。

  - 阶段3-根据各HTTP模块的command描述解析当前配置块下的配置项，填坑：这是个二重循环。对该配置块下的某个配置项，HTTP框架会遍历所有HTTP模块的command描述（只匹配名字？还是也会匹配调用形式？），若匹配成功则调用该HTTP模块对应command中的set函数。set函数负责解析配置项参数，填入阶段2中分配的结构体。

  - 阶段4-阶段3中若遇到http{}/server{}/location{}块，则递归进行阶段2~3。

  - 阶段5-逐级合并配置项：http{}块下的srv级别配置与server{}块下的srv级别配置合并，http{}块下的loc级别配置与server{}块下的loc级别配置合并，server{}块下的loc级别配置与loc{}块下的loc级别配置合并，上级loc{}块的loc级配置与下级loc{}块的loc级配置合并。合并时调用**各HTTP模块**的**各级别**的**合并配置结构体方法**（注意此时HTTP模块已经解析好了所有感兴趣的配置项，并将它们的配置值存到自定义的配置结构体中，所以**不需要分别定义“针对配置项合并”**的方法）merge_srv_conf和merge_loc_conf。

  - 总的来说：就是create、匹配、set（含递归）、merge。根据http{}块、server{}块和loc{}块数量，递归地解析配置。每次递归过程中，框架先调用各个HTTP模块的create分配至多3个级别配置下的结构体（视当前配置块级别而定），再为所有HTTP模块根据各自定义的command描述匹配并解析感兴趣的配置项（用二重循环）。<span style="color:red">理论上（问题5）</span>，类似一个配置项到command集合的映射std::map<conf_name, std::set\<command\>>？。

- 看看定义HTTP模块时的4个关键结构体

  - 配置结构体

    ```c
    // 当然需要一个配置结构体啦，这个配置结构体如果是自定义的，就需要自己写create并在set里头解析参数
    // 当然，如果不用解析参数，就不需要定义结构体、设置create等麻烦事了
    //（像1.4.4 Hello World或P87中采用的方法）
    typedef struct {
        ...
    } ngx_http_mytest_conf_t;
    ```

  - HTTP类型模块的上下文结构体

    ```c
    // 描述HTTP模块3个级别配置结构体的create和merge方法
    // 注意到，因为http{}只会有一个，所以不需要提供main级别的配置结构体合并方法
    typedef struct {
        // pre和post何时调用？（问题6）
        ngx_int_t (*preconfiguration)(ngx_conf_t *cf);
        ngx_int_t (*postconfiguration)(ngx_conf_t *cf);
        
        void *(*create_main_conf)(ngx_conf_t *cf);
        char *(*init_main_conf)(ngx_conf_t *cf, void *conf);
        
        void *(*create_srv_conf)(ngx_conf_t *cf);
        char *(*merge_srv_conf)(ngx_conf_t *cf, void *prev, void *conf);
        
        void *(*create_loc_conf)(ngx_conf_t *cf);
        char *(*merge_loc_conf)(ngx_conf_t *cf, void *prev, void *conf);
    } ngx_http_module_t;
    ```

  - 配置项描述结构体

    ```c
    // 描述1个感兴趣的配置项所属级别和解析方法（各级别下的解析方法都是set？
    struct ngx_command_s {
        ngx_str_t name;
        ngx_uint_t type;
        // cf是啥？（问题7）
        // void *conf指向模块通过各级别create产生的结构体
        char *(*set)(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
        // 使用预设配置项处理方法时的关键，配合offset使用
        ngx_uint_t conf;
        // 使用预设配置项处理方法时的关键，
        // 预设处理方法（set）可以直接往(conf + cmd->offset)写入预设类型的参数值。
        ngx_uint_t offset;
        void *post;
    };
    ```

  - Nginx模块结构体（书P83）

    ```c
    // 定义一个Nginx模块的入口，通过ctx和commands指定模块类型（如HTTP类型、event类型等）和所有感兴趣的配置项描述，ctx统一了各类型模块的公共接口，又提供了一定的灵活性
    typedef struct ngx_module_s ngx_module_t
    typedef struct {
        // 书P302：为提高同类型模块中索引模块顺序的效率，ctx_index为模块在相同类型模块中的顺序
        ngx_uint_t ctx_index;
        // 书P301：所有模块在ngx_modules[]中的序号
        ngx_uint_t index;
        ...
        // 指向该模块所属类型的特定上下文结构体，HTTP模块的指向一个ngx_http_module_t结构体
        void *ctx;
        // 感兴趣的配置项描述数组，每个元素都是1个ngx_command_s实例
        ngx_command_t *commands;
        // 该模块类型，HTTP模块的设置成NGX_HTTP_MODULE
        ngx_uint_t type;
        ngx_int_t (*init_master)(ngx_log_t *log);
        ngx_int_t (*init_module)(ngx_cycle_t *cycle);
        ngx_int_t (*init_process)(ngx_cycle_t *cycle);
        ...
    };
    ```

- <span style="color:red">问题8</span>：看起来，HTTP配置项和HTTP模块是多对多的关系，一个HTTP配置项可以被多个HTTP模块按照自己的command描述解析。是否需要保证nginx.conf中一个配置项无多义性？还是说这就像同名函数重构？

### 1.4.4 Hello World（书上说是阻塞式的？）

- 书本P87演示了自定义模块介入HTTP框架处理过程的1种方法——在自定义模块的配置阶段（set）设置回调函数。这个函数会在HTTP框架后续执行到NGX_HTTP_CONTEXT_PHASE时被回调。这样就达到了自定义模块介入HTTP处理过程的效果
- 如何单元测试？
- 如何使用error日志协助调试？
- <span style="color:red">问题9</span>：HTTP模块介入HTTP处理流程的方法，是否就是在模块配置阶段设置各阶段的回调函数？
  - 20220819：目前来看，可以将Nginx看作一个由`nginx.conf`配置动态驱动的服务器。Nginx的HTTP框架将HTTP请求处理划分为11个阶段，每个HTTP模块都会被归入某一个阶段，而每个HTTP模块的。接收完请求后，框架便进入11个处理请求的阶段，会调用各HTTP模块在配置阶段设置的自定义处理函数（Nginx用一个全局结构体保存所有模块的处理函数）
- **<span style="color:red">问题10</span>：一开始的html界面是在哪个模块里output的？**
  - 20220819：值得跟踪源码理解多阶段请求处理&异步响应（先尝试抛开配置相关的代码）
  - 20220830：通过debug日志，验证了应该是在ngx_http_static_module中调用的ngx_http_send_header和ngx_http_output_filter。



# 2 编写第一个HTTP过滤模块

- P198<span style="color:red">问题11</span>：为什么第三方HTTP过滤模块在ngx_http_headers_filter_module后、ngx_http_userid_filter_module前？
  - ngx_htt_headers_filter_module允许通过修改nginx.conf配置文件，在返回给用户的响应中添加任意的HTTP头部




# 3 尝试调试跟踪源码理解HTTP处理流程（如针对问题10）

## 3.1 异步处理和多阶段请求

书上说的，我也不知道：

- P257：异步处理和多阶段是相辅相成的，只有把请求分为多个阶段，才有所谓的异步处理
- 书P429：以上介绍了丢弃包体的全部流程...非常高效，没有任何阻塞进程，也没有让进程休眠的操作。同时，对于HTTP模块而言，它使用起来也比较简单，值得读者学习。（epoll、请求的引用计数和ngx_http_fianlize_request做到了请求的异步处理？）

## 3.2 模块结构

书上说的，我也不知道：

- P253：Nginx官方定义了五大类模块：核心模块、配置模块、HTTP模块、事件模块、mail模块......它们具有相同的ngx_module_t接口（此时每类模块就用void *ctx具体化上下文！）......配置模块和核心模块（核心模块包含6个具体模块ngx_core_module、ngx_errlog_module、ngx_events_module、ngx_openssl_module、ngx_http_module和ngx_mail_module）这两种模块类型由Nginx的框架代码所定义的，这里的配置模块是所有模块的**基础**，它实现了最基本的配置项解析功能（就是解析nginx.conf文件）。Nginx框架还会调用核心模块，但是其他3种模块都不会与框架产生直接关系。事件模块、HTTP模块、mail模块这3种模块在核心模块中各有1个模块作为自己的”代言人“（如ngx_http_module），并在同类模块中有1个作为核心业务与管理功能的模块（如ngx_http_core_module）——它又是同类模块中其他模块（如HTTP模块的日志模块ngx_http_log_module）的**基础**。
- P262、P298：Nginx的可配置性完全依赖于nginx.conf配置文件，Nginx所有模块的可定制性、可伸缩性等诸多特性也是完全依赖于nginx.conf配置文件的。......任何模块都是以配置项来定制功能的。
- 基本启动流程：
  - P267图8-6：所有模块（ngx_module_t）共有的是init_module和init_process，它们分别在启动worker进程前后被调用。对于核心模块，ngx_module_t的ctx指向了核心模块接口，其中的init_conf会在所有模块的init_module执行前被调用。而对其他类型模块，其ctx指向的模块接口则一般由对应的核心模块在解析配置时调用。（如各个事件驱动模块都实现了ngx_event_module_t，其中的create_conf和init_conf是由ngx_events_module核心模块在解析event{}块下的“use”配置时、回调ngx_events_block函数中调用的，书P301）
  - 基本启动流程就是，调用所有模块的init_module、调用所有模块实现的ctx接口库完成自身所属类别的初始化（ctx指向模块特定接口）、启动worker、调用所有模块的init_process

## 3.3 主从进程管理

### 3.3.1 进程资源结构体（参见core/ngx_cycle.h注释）

- P255：Nginx则不然 ，它不会使用进程或线程来作为事件消费者，所谓的事件消费者只能是某个模块（在这里没有进程的概念）。只有事件手机、分发器才有资格占用进程资源，它们会在分发某个事件时调用事件消费模块使用当前占用的进程资源。
- P260、P263、P297：Nginx核心的框架代码一直在围绕着一个结构体展开，它就是ngx_cycle_t。......每一个进程毫无例外地拥有唯一一个ngx_cycle_t结构体。服务在初始化时就以ngx_cycle_t对象为中心来提供服务，在正常运行时仍然会以ngx_cycle_t对象为中心。ngx_cycle_t保存配置文件信息、日志文件信息、连接池信息、监听池信息、占用文件信息、读写事件信息等，连接对象及其读写事件对象在此被预分配（书P297）

### 3.3.2 进程信息结构体（参见os/unix/ngx_process.h注释）

- P271：master进程不需要处理网络事件，它不负责业务的执行，只会通过管理worker子进程来实现重启服务、平滑升级、更换日志文件、配置文件实时生效等功能。

- 进程信息保存在ngx_processes**全局数组**，虽然**子进程中也会有ngx_processes数组，但这个数组仅仅是给master进程使用的**。

  master进程中所有子进程相关的状态信息都保存在ngx_processes数组中

- 书P269、P273：master如何通知worker进程停止服务或更换日志文件呢？对于这样控制进程运行的进程间通信方式，Nginx采用的是信号。因此，worker进程中会有一个方法来处理信号——void ngx_signal_handler(int signo)。......ngx_processes数组中这些进程的状态是怎么改变的呢？依靠信号！

## 3.4 关键对象：监听对象、连接对象、请求对象、读写事件对象

### 3.4.1 监听对象（参见core/ngx_connection.h注释）

- P261：ngx_listening_t结构体

### 3.4.2 连接对象（参见core/ngx_connection.h注释）

- 202200819-overview：Nginx中比较关键的是连接对象。一个连接对象对应一对读写事件，读写事件各自有自己的回调函数来完成事件被触发时需要执行的操作。目前来看，一个连接对象**有且仅有一对**读写对象，Nginx的HTTP框架通过**分阶段改变连接读写对象的回调函数**来实现连接在不同阶段的不同操作。Nginx进程的ngx_cycle_t会保存一个动态数组，存储其监听对象。目前我把监听对象看作一种特殊的连接对象，所以listen->connection->read->handler负责accept的操作。
- P292、P288~290：ngx_connection_t结构体

### 3.4.3 请求对象（参见http/ngx_http_request.h注释）

- P391：ngx_http_request_t结构体。而一个请求会被包装成ngx_http_request_t结构体（书P391），其中的phase_handler用作phase engine结构体中handlers数组的下标。HTTP框架就是用这种方式把各个HTTP模块集成起来处理请求的。结构体中的read_event_handler和write_event_handler是针对异步处理机制设立的，Nginx针对请求的读写被划分成独立的事件，这些事件可以借由epoll多次回调完成非阻塞的读写，多个读写事件共享同一个请求结构体，借助count字段和ngx_http_finalize_request函数管理引用计数，从而实现资源管理。参见http/ngx_http_request.h注释

### 3.4.4 读写事件对象（参见event/ngx_event.h注释）

- P288：Nginx事件结构体



## 3.5 事件模块（以epoll为例）

### 3.5.1 问题1：事件结构体的active位、ready位、write位分别什么含义？

- ready：
  - 设置ready=1：在ngx_epoll_process_events中，epoll_wait返回一个EPOLLIN|EPOLLOUT事件后，函数**设置该事件的ready=1**。
  - 设置ready=1：在ngx_event_accept中，会根据deferred accept/rtsig/aio/iocp特性，**设置事件ready=1**。
  - 设置ready=0：ngx_event_accept中，从监听对象accept连接前，会**设置”监听连接“的ready=0**。
  - 设置ready=0：连接对象的recv和send函数（分别为ngx_unix_recv和ngx_unix_send），会直接调用recv和send系统调用，然后根据返回值**设置读写事件的ready=0**（epoll模块会在ngx_epoll_init中将全局的ngx_io设为ngx_os_io，ngx_os_io指定了连接的读写事件的recv和send函数分别为ngx_unix_recv和ngx_unix_send，这两个函数中会操作ready位）
  - 询问ready==1：ngx_handle_write_event中，针对select/poll模块，active==1且ready==1时会将事件从事件驱动模块中删除
  - 询问ready==1：ngx_http_read_request_header中，ready==1时会直接调用r->connection->recv去读取数据
  - 询问ready==1：ngx_finalize_http_connection中，读事件回调ngx_http_lingering_close_handler会在ready==1时不断调用连接对象的recv。
  - 询问ready==0：ngx_handle_read_event只有在ready==0且active==0时才会往epoll添加事件。
  - 推断：ready应该是用于标记**事件对应的fd上能否读写，ready==0则一定无法读写，ready==1则可以尝试读写（但可能没有数据）**
  - 源码：ngx_event.h中给ready位的注释是——"the ready event; in aio mode 0 means that no operation can be posted"

- active：
  - 设置active=1：在ngx_epoll_add_event和ngx_epoll_add_connection中，会在成功调用epoll_ctl(.., EPOLL_CTL_ADD, ...)或epoll_ctl(..., EPOLL_CTL_MOD, ...)后**设置事件active=1**。（<span style="color:red">poll模块则在真正添加fd前设置，为什么？</span>）
  - 设置active=0：在ngx_epoll_del_event和ngx_epoll_del_connection中，会在成功调用epoll_ctl(.., EPOLL_CTL_DEL, ...)后**设置事件active=0**。（poll模块则在真正删除fd前设置）
  - 询问active==0：ngx_handle_read_event/ngx_handle_write_event中，当active==0且ready==0时才会往epoll添加或删除事件
  - 询问active==1：ngx_epoll_add_event/ngx_epoll_del_event中，会询问连接对象的peer事件的active是否为1，来确定对当前事件的fd是采用EPOLL_CTL_ADD还是EPOLL_CTL_MOD。
  - 询问active==1：ngx_epoll_process_event中，一个fd触发了EPOLLIN或EPOLLOUT时，还需要对应的rev->active==1或wev->active==1才会往下设置事件的ready位（<span style="color:red">有可能一个fd上注册了EPOLLIN但其读事件active==0吗？或这只是用于应对EPOLLERR和EPOLLHUP的情况？</span>）
  - 推断：active用于标记**事件是否在事件驱动模块中监听**，active==1表示在，active==0表示不在
  - 源码：ngx_event.h中给出active位的注释是——"the event was passed or **would be passed** to a kernel; in aio mode - operation was posted."（参考ngx_handle_read_event中eventports部分居然调用ngx_del_event）

- write：
  - 设置write==1（vs全局搜索似乎只有这一项）：ngx_get_connection中对新连接的**写事件设置write=1**。
  - 询问write==1：ngx_http_request_handler中判断事件ev->write==1则调用r->write_event_handler，否则调用r->read_event_handler
  - 询问write==1：通过vs的全局引用搜索，看到select和upstream相关文件中用到ev->write，类似用来判断当前事件是否为写事件
  - 推断：write用于标记事件是否为写事件，write==1表示为写事件，write==0表示为非写事件（有点像accept位作用）
  - 书P288：write==1表示事件是可写的。通常情况下，它表示对应的TCP连接目前状态是可写的，也就是连接处于可以发送网络包的状态。（存疑：从连接结构体看到有”连接->事件“的联系，但从事件结构体并没看见”事件->连接“的联系？）

### 3.5.2 <span style="color:red">问题12</span>：worker进程基本的事件处理循环是怎样的？（参见event/ngx_event.c注释和event/modules/ngx_epoll_module.c注释）

- P312：默认情况下，Nginx是通过ET模式使用epoll的
- P286~287：Nginx事件处理框架定义了9个事件驱动模块，在ngx_event_core_module模块（event类型模块的基础模块，类似HTTP模块的ngx_http_core_module。区分于核心模块ngx_events_module）的初始化过程中，Nginx会从这9个模块中选取1个作为Nginx进程的事件驱动模块。
- P270：worker进程事件处理和信号处理循环
- worker进程的事件处理循环是通过worker工作循环不断地调用事件处理函数完成的
- 事件处理函数主要完成4项工作（参见event/ngx_event.c注释）：调用1次事件处理机制（如epoll）的处理函数，统计处理机制处理本批次事件的耗时，然后处理定时事件和分发的延迟事件（如posted队列中的事件）
- 处理机制（如epoll）的处理函数主要完成2项工作（参见event/modules/ngx_epoll_module.c注释）：调用1次epoll_wait(2)获取1批待处理事件，分发或直接处理。

### 3.5.3 <span style="color:red">问题13</span>：epoll是如何实现的？为什么相比select和poll（P308），epoll能处理百万级并发？

- 书上：基于红黑树管理事件，利用设备驱动触发回调函数，将事件从红黑树中提出到一个双向链表，epoll_wait直接访问查询双向链表（轮训查询？epoll_wait如何实现阻塞的？）。

### 3.5.4 问题14：什么是”惊群“？Nginx如何防止的？什么是“负载均衡”？“负载均衡”和“惊群”的关系？

- n个进程或线程注册了同一个fd上的读写事件，当fd上有一个事件到来时，n个进程或线程的epoll_wait都被唤醒，但只有1个进程能成功获取并处理这个事件，其余n-1个进程徒增了非阻塞系统调用的开销。
- 所以，worker进程如果想要在一次epoll_wait中接收新连接，需要某种“同步”机制达成多个进程处理新连接的同步（即大家都知道现在谁正在接受新连接）。Nginx通过文件锁实现进程间同步。在epoll_wait前先获得accept_mutex锁并往epoll中添加监听事件，而无法获得锁时仍然可以处理已建立连接的其他事件
- 解决了“惊群”，只是保证fd有事件到来时，**只有一个**进程被唤醒，但很可能**只有这一个**进程被唤醒。所以还需要引入建立连接的负载均衡。即一个worker进程不能保有太多的连接，或，保有一定量的连接后就禁止它接受新连接。
- accept_mutex锁配合ngx_accept_disabled变量能做一定的负载均衡。具体地，即worker进程若想在一次epoll_wait中接受新连接，还需要存活的连接数<=连接池可容纳连接总数的7/8。
- 理论上，Nginx应该在建立和销毁连接时对ngx_accept_disabled++或--，但因为连接可以复用、或者被异步地关闭，这样精确地维护会比较复杂。本质上，负载均衡只是让当前worker进程在“接受新连接”这个操作上静默一段事件，所以Nginx在worker进程尝试处理epoll_wait事件前就--ngx_accept_disabled，达到了静默的效果，代码也容易编写。

### 3.5.5 <span style="color:red">问题15</span>：定时器有什么用？

- 比如用于优雅退出？（P271：就是将连接中的close标志位设成1，再调用读事件的处理方法）

### 3.5.6 <span style="color:red">问题16</span>：核心模块ngx_events_module和基础模块ngx_event_core_module，到底谁在操作&定义结构体ngx_event_module_t，从而最终让框架选用了epoll模块？

- ngx_event_module_t属于各类型模块的上下文（由ngx_module_t的ctx成员指向），类似ngx_http_module_t，可以理解为**接口定义**，需要由处理模块自身实现，参考ngx_epoll_module模块的实现（书P313）。这里注意区分书P297说的：**ngx_event_module_t接口本身是由ngx_events_module核心模块定义的**。
- 所以，事件驱动机制的基础模块ngx_event_core_module也需要实现一个ngx_event_module_t作为其ngx_module_t定义中的ctx成员。回忆之前提到的，定义一个Nginx模块需要指定ctx和commands，commands代表了模块如何解析配置，这提供了模块间联动的可能。所以ngx_event_core_module的ctx只与自身有关，而commands中有一个“use”配置项，其中的ngx_event_use回调函数确定选择了选择哪一个事件模块作为事件驱动机制（书P303）。而根据ngx_event_core_module基础模块的的配置结构体ngx_event_conf_t（书P304）可以<span style="color:red">猜测</span>，**函数是通过保存ngx_epoll_module模块的ctx_index，让框架选用了epoll对应的接口**，后续便可以直接调用。
- 所以，提问其实有问题：框架有很多事件驱动模块，每个事件驱动模块都有实现了一个ngx_event_module_t接口。即基础模块ngx_event_core_module和epoll机制模块ngx_epoll_module都实现了一个ngx_event_module_t接口（而ngx_events_module核心模块因为不属于事件驱动类模块，它实现的是名为ngx_core_module_t的接口，书P298）。
- 但框架运行时采用的事件驱动模块（应该？）只有一个，这是由ngx_event_core_module在解析“use”配置项时确定的（具体细节需要看ngx_event_use是如何处理“use”配置项的）。

### 3.5.7 <span style="color:red">问题17</span>：epoll模块为什么要分发事件？怎么分发事件？

- 书P315~318
- Nginx中，每个连接都有1个套接字fd、1个读事件对象指针和1个写事件对象指针。而每个事件都有1个所属连接的对象指针。所以执行ngx_epoll_add_event或ngx_epoll_del_event时，使用的是epoll_ctl向epoll对象添加或删除事件，添加或删除的文件描述符是事件对应的连接的fd、传入的((struct epoll_event) ev).data是**事件所属连接的对象指针**。**通过这个指针，可以当前阶段该连接的读写对象的描述**，执行相应的回调函数。
- 执行ngx_epoll_process_event时，主要就是调用epoll_wait并处理返回的事件列表。对一个返回的事件，主要是根据事件类型EPOLLIN或EPOLLOUT作相应处理（Nginx还可以通过返回事件的data字段拿到这个事件对应的连接对象，从而拿到连接对象的读写对象，读写对象的active位决定了ngx_epoll_process_events是否处理当前的EPOLLIN和EPOLLOUT事件。）。
- 什么是“分发”事件：对1次epoll_wait返回的1批事件，通过ngx_epoll_process_event传入的flags参数判断这批事件是否需要延后处理，若需要延后处理则加入ngx_posted_accept_events队列（新连接事件队列）或ngx_posted_events队列（普通读写事件队列）；若不需要延后处理，则直接调用通过data字段找到的连接对象对应的读写对象，回调读写对象中的handler来处理读写对象自己（rev->handler(rev)）。
- 所以“分发”事件就是根据flags位判断是否延后处理，可以理解为为事件处理“排序”。而处理读写则是直接回调套接字对应连接对象的读写事件对象的处理函数。
- 20220830-debug分析日志

### 3.5.8 问题18：有关业务和事件驱动解耦联动的机制，HTTP的读写事件的回调及添加删除是如何借助epoll完成的？

- 可能除了监听连接和建立连接后第一次读取外，读写事件多数还是跟业务框架有关。
- 其实应该就是通过事件驱动模块提供的对外接口，ngx_handle_read_event和ngx_handle_write_event实现交互的，有点像系统调用。

### 3.5.9 问题19：Nginx使用ET模式epoll处理连接的读事件，如何保证再下一次添加完读事件时候前recv是返回EAGAIN的（即判断recv返回EAGAIN到添加新事件的间隙中，fd有了新数据）？

- fd的EPOLLIN一直在epoll中，只是handler改变了。handler改变则是上层协议决定的了。
- 比如HTTP协议规定，解析完请求行就是解析请求头，本次recv的数据已经包含整个请求行，此时从协议的语义上来说，回调函数就可以更改为处理请求头的回调了。
- 因此处理请求头的回调函数应该实现从接受缓冲+新EPOLLIN数据解析请求头的功能！采用非阻塞模式编程，接收缓存是必须的，异步回调函数就应该有同时处理 接收缓冲+EPOLLIN新数据 的功能！
- 事实上，利用事件的ready位能在一次epoll_wait返回后，模拟触发多个回调函数的过程。当然，回调函数中须要把流程串起来，比如解析完HTTP请求行，就把连接读事件的回调函数改为解析HTTP请求头的函数（记为process_headers），然后检查发现ready==1则调用一次process_headers。

### 3.5.10 <span style="color:red">问题20</span>：epoll中ET和LT模式的accept有什么区别？为什么很多网络库如libevent、boost.asio、muduo都用LT模式，而Nginx用ET模式？哪种方式效率高？



### 3.5.11 <span style="color:red">问题21</span>：Nginx是否默认使用了EPOLLONESHOT？不然怎么书上说recv返回EAGAIN时都要再次添加读事件？

- HTTP接受请求的过程中，ngx_http_init_connection、ngx_http_read_request_header和ngx_http_request_handler似乎出现了往epoll多次添加事件的操作
  - ngx_http_init_connection(ngx_connection_t*)：这是新连接的第一个HTTP相关处理函数，参数对应连接的读事件的回调函数应该在ngx_event_accept中被设置，但ngx_event_accept中只针对deferred accept/rtsig/aio/iocp特性设置了读写事件的ready位（如init_connection函数中Nginx原有注释所言！），并没有设置回调。所以对epoll而言，此处调用的ngx_handle_read_event实际上是**新连接读事件第一次被加入epoll**。
  - ngx_http_read_request_header(ngx_http_request_t*)：参照源码（http/ngx_http_request.c），当r->connection->read->ready==1时，该函数会尝试调用一次r->connection->recv来读取数据，若读取返回NGX_AGAIN（当读事件ready==1，调用一次recv是否可能返回NGX_EAGAIN？（有可能。ET模式中，若多个回调函数流式地处理，回调函数1刚好读完，并将回调改为下一个处理函数时，需要调用一次下一个回调函数。这时下一个回调函数再recv，可能返回EAGAIN。但看Nginx中对连接读写函数c->recv的封装，c->recv会及时在读取到EAGAIN时将事件的ready位置0。所以这种情况在Nginx中应该不会出现），则会调用ngx_handle_read_event往epoll添加读事件。但是参照源码（event/ngx_event.c）会发现不论哪种事件驱动模块，只有事件active和ready位同时==0时，事件驱动模块的ngx_add_event才会被调用。而且ngx_handle_read_event根据传入的flags不同，还可能调用ngx_del_event。所以书上P290~291的说法不全对。ngx_handle_read_event其实是根据flags处理事件驱动中的读事件，默认flags情况下会**尝试**往事件驱动模块添加读事件（当事件已经在里面，即active==1；或事件已经准备好，即ready==1时，都不应该再次添加。前者是因为不需要，后者是因为既然事件已经准备好，则应该由事件回调函数及时处理，至少ngx_http_read_request_header里体现了这点）
  - ngx_http_request_handler：其实并没有多次添加连接读写事件或请求的读写事件。<span style="color:red">为什么处理请求行和请求头的时候就要在ngx_http_process_request_line和ngx_http_process_request_headers中多次调用ngx_handle_read_event呢？ET模式的epoll需要这样吗？</span>
- HTTP接受请求包体的过程中，
- HTTP发送响应的过程中，ngx_http_send_header和ngx_http_output_filter都会尝试一次发送数据的操作，若无法一次性发送数据，则会将剩余数据插入一个发送缓冲的链表（尾插入）并返回NGX_AGAIN，该值会被ngx_http_finalize_request接收到，后者则修改请求的写函数为ngx_http_writer，将剩余数据交由ngx_http_writer继续发送，ngx_http_writer出现了往epoll多次添加事件的操作
  - ngx_http_writer(ngx_http_request*)：其实ngx_handle_write_event不一定会往epoll添加事件，<span style="color:red">**甚至会从select/poll移除事件（？LT模式的write需要及时移除写事件防止busy loop？所以ngx_http_writer最后还会调用一次ngx_http_finalize_request并传入回调返回值？）**</span>，对epoll而言，该接口只是**逻辑上尝试**添加。





## 3.6 多模块处理HTTP请求

### 3.6.1 初始化：自定义的模块处理函数（handler）是如何注册到处理流程的？

- 全局的ngx_http_core_main_conf_t保存ngx_http_phase_engine_t，这个phase engine是所有HTTP模块可以同时处理用户请求的关键！phase engine结构体保存了ngx_http_phase_handler_t数组（书P375），这个数组保存了请求将经历的所有处理方法！

- 而一个请求会被包装成ngx_http_request_t结构体（书P391），其中的phase_handler用作phase engine结构体中handlers数组的下标。HTTP框架就是用这种方式把各个HTTP模块集成起来处理请求的。

- 介入方式：HTTP框架有一个全局的配置结构体（ngx_http_core_main_conf_t），这个配置结构体有一个二维数组，保存着11个HTTP处理阶段所有HTTP模块的自定义处理方法（一个HTTP模块在同一个阶段可以插入多个处理方法）。所以自定义模块只需要在适当时候往这个二维数组插入方法即可。因为完成配置读取后，框架会调用所有模块的postconfiguration方法，所以可以在postconfiguration中向框架注册自定义方法。我们在自定义的ngx_http_module_t结构体中，定义一个postconfiguration方法，在该方法中往全局配置结构体的阶段处理方法二维数组（phases[PHASE_ID].handlers动态数组，PHASE_ID代表11个阶段的阶段索引）中添加处理方法即可。


### 3.6.2 执行流程：一个新连接从建立、接收请求头、异步接收包体、处理请求、异步发送包体、释放的过程时怎样的？



- 20220903：

  <img src="./img/callback_stack.png" alt="img" style="zoom:80%;" />



#### （1） 监听对象初始化连接及连接的回调

- 处理新连接的入口：监听-->ngx_event_accept-->ngx_http_init_connection。
  - “监听连接”到达新的读事件时，调用“监听连接”读回调函数ngx_event_accept，**ngx_event_accept中会从监听端口accept并建立新连接，紧接着调用ls->handler，由此新连接的处理进入了init_connection**。
  - 注意，ngx_http_init_connection是新连接的第一个处理函数，它设置新连接的第一个读回调函数为ngx_http_init_request（由ngx_http_init_connection设置）
- 在哪里设置了ngx_event_accept？

  - 总之：**在ngx_event_core_module的init_process方法中被设置，而worker进程进入循环处理函数前会调用各个模块的init_process**

  - 具体的，Nginx启动时，当前进程扮演master进程，调用ngx_start_worker_processes（os/unix/ngx_process_cycle.c中）来启动worker进程。ngx_start_worker_processes则会调用ngx_spawn_process（os/unix/ngx_process.c中）来执行fork、并传入worker进程的循环函数ngx_worker_process_cycle（os/unix/ngx_process_cycle.c中），ngx_spawn_process在fork出子进程后调用循环函数。

  - worker进程的循环函数ngx_worker_process_cycle（os/unix/ngx_process_cycle.c中）会**先调用各个模块的init_process方法**，（**此处得以为监听对象的“监听连接”建立读回调——ngx_event_accept**），**再循环调用ngx_process_event**方法（该方法在event/ngx_event.h中宏定义为ngx_event_actions.process_events，即所选模块对应的处理方法）。

  - 设立读回调：event/ngx_event.c:ngx_int_t ngx_event_process_init(ngx_cycle_t *cycle)，该方法是ngx_event_core_module核心模块的init_process方法，会在Nginx启动时被调用！（书P267）：

- 在哪里进入了ngx_event_accept？

  - 监听对象作为一种特殊连接，监听端口有新的建立连接请求到达时，其“监听连接”触发读回调，进入ngx_event_accept。
- ngx_event_accept如何accept并建立新连接？如何让新连接进入到init_connection和后续处理？

  - accept并建立新连接后，**紧接着对新连接调用ls->handler(c)**，即init_connection(c)。

- 在哪里设置了新连接的第一个回调函数是init_connection？

  - 新连接的第一个读回调函数**不是init_connection**，init_connection是新连接的**第一个处理函数**，它负责设置新连接的**第一个回调函数init_request**。
  - ngx_http.c:ngx_http_add_listening中（该函数被ngx_http.c:ngx_http_init_listening调用），用ngx_create_listening创建了监听对象ls，然后设置ls->handler=ngx_http_init_connection。（却没有设置监听对象的fd）。
- 新连接在哪里进入了init_connection？（总之：init_connection是**新连接被accept后的第一个读回调函数**）

  - “监听连接”到达新的读事件时，回调了“监听连接”的ngx_event_accept，**ngx_event_accept中会对新连接直接调用ls->handler**，由此新连接进入了init_connection。


#### （2） 处理请求的11个阶段（假设用epoll事件驱动模块）

- 20220819-overview、20220828-RTFSC(no gdb，以下函数名均省略`ngx_http_`前缀)
  - 1、新连接第一个处理函数init_connection(**ngx_connection_t *c**)（仅1次）：设置连接的读事件rev->handler=init_request，写事件c->write->handler=empty_handler，若rev->ready为1，则**返回并在此前调用**rev->handler(rev)，否则**往epoll添加当前读事件**。
  - 2、连接读回调init_request(**ngx_event_t *rev**)（仅1次）：直接设置连接的rev->handler=process_request_line，并在函数**最后调用**rev->handler(rev)。该函数中，c->requests++（即，连接处理的请求数++），还有近似<span style="color:red">“c->data=c->data->request”</span>的操作（设置连接的请求，这里应该有考究）。
  - 3、连接读回调process_request_line(**ngx_event_t *rev**)（借助epoll多次回调自身）：**循环**调用read_request_header，若读取到数据则调用parse_request_line来解析请求行，若读到NGX_AGAIN则return给事件处理模块。
    - read_request_header(**ngx_http_request_t *r**)：调用rev->connection->recv读socket数据，若recv返回0则输出log提示对端已关闭连接，若recv返回NGX_AGAIN则**往epoll再次添加当前读事件**（此时读事件rev->handler=process_request_line）然后返回（process_request_line接收到NGX_AGAIN的返回值后直接return，控制权交还给事件处理模块了）
    - 解析请求行：parse_request_line返回NGX_OK代表解析完毕，此时process_request_line会修改rev->handler=process_request_headers，然后**返回并在此前调用**rev->handler(rev)；parse_request_line返回NGX_AGAIN代表还要继续读取，此时process_request_line回到循环开始处调用read_request_header
  - 4、连接读回调process_request_headers(**ngx_event_t *rev**)（借助epoll多次回调自身）：**循环**调用read_request_header，若读取到数据则调用parse_header_line来解析头部字段，若读到NGX_AGAIN则return给事件处理模块。
    - read_request_header(**ngx_http_request_t *r**)：同process_request_line中被循环回调的read_request_header。read_request_header完成recv或**往epoll再次添加当前读事件**（此时的读事件rev->handler=process_request_headers，因为请求行解析完毕才第一次调用process_request_headers，在第一次调用前，process_request_line已经将rev->handler修改）
    - 解析头部字段：parse_header_line返回NGX_OK代表解析完一行，此时process_request_headers会continue，进入下一个read parse循环；parse_header_line返回NGX_HTTP_PARSE_HEADER_DONE代表解析完头部，此时process_request_header**s**会调用process_request_header，并在**返回并在此前调用**process_request；parse_header_line返回NGX_AGAIN代表还要继续读取才能解析一行，此时process_requeset_headers也会continue
    - （process_request_header：类似判断头部某些字段是否合理？仅返回NGX_ERROR或NGX_OK，无读写事件发生。）
  - 5、连接读回调process_request(**ngx_http_request_t *r**)（仅1次）：将连接r->connection的读写回调都设置为request_handler，将请求r的读回调设置为block_reading，函数**最后调用**ngx_http_handler(r)和ngx_http_run_posted_requests(c)
    - 请求读回调block_reading(**ngx_http_request *r**)：不执行读取，只记录日志，如果是NGX_USE_LEVEL_EVENT的触发，而且r->connection->read->active为1，则调用ngx_del_event(r->connection->read, NGX_READ_EVENT, 0)移出连接读事件。
    - handler(**ngx_http_request *r**)（仅1次）：设置r->write_event_handler=core_run_phases(r)，**最后调用**r->write_event_handler(r)
    - 请求写回调core_run_phases(**ngx_http_request *r**)：根据请求所处阶段和状态（如phase_handler索引），调用各HTTP模块的所属阶段的checker函数，由各checker函数调用HTTP模块的自定义handler。
  - 6、连接读回调和写回调request_handler(**ngx_event_t *ev**)（？借助epoll多次回调自身？）：仅负责触发请求读回调和写回调。ev->write==0则调用ev->data->data->read_event_handler(r)（即请求的读回调），否则调用ev->data->data->write_event_handler(r)（即请求的写回调）
    - 请求读回调block_reading(**ngx_http_request *r**)：同前所述
    - 请求写回调core_run_phases(**ngx_http_request *r**)：根据请求所处阶段和状态（如phase_handler索引），调用各HTTP模块的所属阶段的checker函数，由各checker函数调用HTTP模块的自定义handler。
      - HTTP模块的自定义handler：可以使用3.6.3.2和3.6.3.3的异步接口，如ngx_http_static_module的handler。<span style="color:red">（异步接口在请求还是连接上产生新的读写回调函数？）</span>然后根据返回值决定handler中具体的处理行为（Hello World中不用等到包体接收完毕，比较简单。<span style="color:red">如果HTTP模块需要调用异步接口接受包体后处理或发送请求后再处理，岂不是要阻塞等待接受/发送完毕？还是说，这不同于不同编程，需要采用异步编程模式？</span>）。这些异步接口把请求处理划分成了独立的事件。
- <span style="color:red">问题22</span>：HTTP请求即连接结构体的data字段，为什么一个连接只能有一个请求？这岂不是串行处理请求对象？不能像一个服务端口并发百万TCP连接。还是说下面的异步接收包体和发送响应实现了“并发处理”多个请求？
- <span style="color:red">问题23</span>：如果两种介入CONTENT_PHASE的方式是等价的，为什么调用请求的content_handler前要把请求的写事件回调函数设为空，调用模块的handler前就不用？书说“可以防止该HTTP模块异步地处理请求时却有其他HTTP模块还在同时处理可写事件、向客户端发送响应”？（书P413）













#### （3） HTTP框架为模块开发提供接口：异步接收包体（3Q）

- overview：
  - <span style="color:red">是否会修改请求的读回调函数？如果修改了，是否需要将读回调设置回block_reading？在哪里设置的？</span>
  - <span style="color:red">是否会修改请求的写回调函数？如果修改了，是否需要将写回调设置回core_run_phase？在哪里设置的？</span>
  - <span style="color:red">到底什么时候会将引用计数+1？目前来看肯定不是触发”一个为请求添加新事件的独立操作“时+1，不然ngx_http_output_filter就要count+1。</span>
    - P162：理论上说，每执行一次异步操作应该把引用计数+1，而异步操作结束时应该调用ngx_http_finalize_request方法把引用技术-1...务必要保证对引用计数的增加和减少时配对进行的。
- P417：丢弃不等于可以不接受包体，这样做可能会导致客户端出现发送请求超市的错误，所以这个丢弃只是对于HTTP模块而言的，HTTP框架还是需要“尽职尽责”地接收包体，在接收后直接丢弃。
- P418：在HTTP模块中每进行一类新的操作，包括为一个请求添加新的事件，或者把一些已经由定时器、epoll中移除的事件重新加入其中，都需要把这个请求的引用计数+1（<span style="color:red">那为什么只有接收包体的流程图中提高引用计数+1？</span>）。这是因为需要让HTTP框架知道，HTTP模块对于该请求有独立的异步处理机制，将由该HTTP模块决定这个操作什么时候结束，防止在这个操作还未结束时HTTP框架却把这个请求销毁了（如其他HTTP模块通过调用ngx_http_finalize_request方法要求HTTP框架结束请求），导致请求出现不可知的严重错误。**这就要求每个操作在“认为”自身的动作结束时，都得最终调用到ngx_http_close_request方法，该方法会自动检查引用计数，当引用计数为0时才真正地销毁请求**。实际上，很多结束请求的方法最后一定会调用到ngx_http_close_request方法。
- P438：每个HTTP请求都有一个引用计数，每派生出一种新的、会独立向事件收集器注册事件的动作时（如ngx_http_read_client_request_body方法），都会把引用计数+1，这样每个动作结束时都通过调用Ngx_http_finalize_request方法来结束请求，检查引用计数，为0再作销毁......也即是说，HTTP框架要求在请求的某个动作结束时，必须调用ngx_http_finalize_request来结束请求。
- P427：当ngx_http_discard_request_body方法返回NGX_OK时，是可能表达很多意思的。HTTP框架的目的是希望各个HTTP模块不要去关心丢弃包体的执行情况，这些工作完全由HTTP框架完成。















#### （4） HTTP框架为模块开发提供的接口：异步发送响应（6Q）

- overview：
  - <span style="color:red">如果一个HTTP请求处理过程中，如何保证一定有一个模块调用了异步发送接口？</span>
  - <span style="color:red">如果一个HTTP请求处理过程中，有多个模块调用了异步发送请求头的接口会怎么样？</span>
  - <span style="color:red">如果某个HTTP模块调用了send_header或output_filter方法、该方法改了请求写回调，那岂不是无法继续调用HTTP处理11个阶段中其他模块的checker了？</span>
  - <span style="color:red">如果HTTP模块中调用了send_header或output_filter，无法一次性发送或能一次性发送时，请求写回调是不是core_run_phases？</span>
    - send_header和output_filter最后都会调用到ngx_http_write_filter，ngx_http_write_filter先将传入的发送内容插入到发送链表尾部，再尝试一次c->send_chain，然后直接返回结果，上层函数（一定是ngx_http_finalize_request？）根据返回值再设置异步发送回调函数或接收请求。
    - 所以调用这两个接口后，具体的请求写回调状态要看接收send_header和output_filter返回值的上层函数的处理结果

  - <span style="color:red">既然发送响应只用到了c->send_chain，那为什么接收的时候不用c->recv_chain？</span>
  - <span style="color:red">ngx_http_write_filter从r->pool获取内存了、还会往epoll和定时器多次添加事件，但为什么不增加r->count？</span>

- P193：HTTP过滤模块的另一个特性是，在普通HTTP模块处理请求完毕，并调用ngx_http_send_header发送HTTP头部，或者调用ngx_http_output_filter发送HTTP包体时，才会由这两个方法一次调用所有的HTTP过滤模块来处理这个请求。
- 综合书P433和书P435：ngx_http_send_header方法最终会调用ngx_http_write_filter方法来发送响应头部，而ngx_http_output_filter方法最终也是调用**ngx_http_write_filter**方法来发送响应包体的。ngx_http_write_filter是最后一个HTTP过滤模块的方法（最后一个过滤模块需要负责发送请求），它会尝试一次发送请求结构体中的out成员，如果无法一次性发送，则返回NGX_AGAIN。ngx_http_output_filter也会返回NGX_AGAIN，表示out中还有未发送的数据。所以不论是ngx_http_send_header还是ngx_http_output_filter，在无法一次性发送out数据时，都返回NGX_AGAIN给ngx_http_finalize_request，根据传入的是NGX_AGAIN，**ngx_http_finalize_request设置请求的写事件回调函数为ngx_http_writer**并往epoll添加第一次ngx_http_writer的回调，而ngx_http_writer会尝试调用一次**ngx_http_output_filter**来发送out数据，**然后通过往epoll设置自身的多次回调来完成out数据的发送**。

  - ngx_http_set_write_handler-->ngx_http_writer-->rc = ngx_http_output_filter(r, NULL)-->ngx_http_top_body_filter（又要走一遍body的HTTP过滤模块）-->...-->ngx_http_writer_filter。







#### （5）结束请求和结束连接

- NGX_HTTP_CONTENT_PHASE阶段的每个模块的返回值会被传入ngx_http_finalize_request，由ngx_http_finalize_request通过传入的返回值决定行为——异步or结束请求。若结束尝试请求，则会调用ngx_http_finalize_connection；

- ngx_http_finalize_connection事实上不止操作connection，还会调用ngx_http_close_request来管理request的引用计数并真正地调用ngx_http_free_request来释放请求资源。ngx_http_close_request还会调用ngx_http_close_connection，而ngx_http_close_connection才会调用ngx_http_free_connection来释放资源。

  即finalize会完成异步和调用close的操作来尝试释放资源

  finalize_request会尝试finalize_connection，finalize_connection才真正地尝试用close释放request（进而释放connection资源）。但close不一定会释放资源（因为引用计数&keepalive特性），free才会释放资源。





### 3.6.3 如果多个HTTP模块的handler中都对处理的请求发送了HTTP响应，框架如何保证发送HTTP响应的顺序是正确的？

