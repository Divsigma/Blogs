# 前言

&ensp;&ensp;&ensp;&ensp;此篇旨在整理一些Redis中值得注意的疑点和区分点（应该会不断更新）。

&ensp;&ensp;&ensp;&ensp;对问题的解答尽量来源于Redis官方文档或标准文件，每问后尽量附上参考链接



<br />

<br />

# 正文

## 1. 基础

### 1. 什么是Redis?

- 开源的、基于内存的（但也提供了持久化机制）、数据结构存储数据库；
- 它用C语言编写（ANSI C），主要存储模型是键值对，“键”是字符串，但“值”可以是多种数据结构，如字符串、列表（Linked List）、集合（Set）、有序集合（Sorted Set）、哈希表、流（Streams）等等；
- 

> https://redis.io/topics/introduction



### 2. 如何实现多个Redis命令的原子性？

- 1）用Redis事务
  - 实现：用`WATCH`给变量加乐观锁（provide Check-And-Set behavior），用`MULTI`将创建事务队列，用`EXEC`一次性执行事务队列的命令；
  - 大致原理：
    - `EXEC`让Redis事务队列中命令串行执行，无法被其他命令（不同于MySQL的事务），从而实现多个命令的原子性。所以Redis的事务执行过程中，仅有编程性错误（如操作错误数据类型，这种低级问题在生产上是不会发生的！），所以Redis不提供回滚机制而且在事务的某条命令错误后会继续执行。抛弃回滚机制提升了简化了Redis实现并提升了它处理事务的效率；
    - `WATCH`给监控的key加乐观锁，主要防止加锁后到调用`EXEC`前key发生变化。调用`EXEC`时，Redis自动释放所有key上的`WATCH`，并检查它们是否有被修改，若有则abort事务，若无则执行队列中命令。
- 2）用Lua脚本
  - 实现：`EVAL`/`EVALSHA`执行脚本字符串；
  - 大致原理：Redis保证了`EVAL`的命令是由同一个Lua解释性解释执行的，执行过程无法被打断
- 3）两种方式优缺点对比？
  - 脚本是在Redis2.6后引入的，用它执行事务写起来更方便，在持久化、解析执行上都比多条命令组成的Redis事务更高效，在主从复制时也能节省网络带宽；
  - 但利用脚本复制也有局限，脚本中的写命令只能操作数据库中确定的元素（如系统时间、随机返回的元素则不确定），否则会导致主从不一致（可以用`script effects replication`的复制机制解决，该机制解析脚本并将命令封装为Redis事务再进行传输并计入AOF，此时可以操作不确定元素）；
  - 保留Redis事务机制似乎只是为了语义上的价值（？

> - Transactions of Redis: https://redis.io/topics/transactions
> - Lua Script of Redis: https://redis.io/commands/eval



### 3. 简述Redis的持久化机制

- 主要分两种方式：RDB快照持久化和AOF日志持久化
- RDB快照持久化：
  - 优点：
    - RDB快照是压缩形式的单文件数据库快照，用于保存数据库备份、灾难恢复、主从复制上都很方便；
    - 数据库数据量大时，用RDB快照重启Redis的速度很快；
    - 生成RDB快照时，父进程fork出子进程进行I/O（copy-on-write semantic），父进程依旧可以处理其他请求（父进程不进行I/O），这提高了Redis的性能；
  - 缺点：
    - RDB快照只能周期性生成（非实时持久化），而且粒度一般较大（5min），所以降低数据丢失的能力有限；
    - 数据库数据量大时，fork用于生成RDB快照的子进程时开销会比较大（甚至导致Redis停止为客户端服务）；
- AOF日志持久化：
  - 优点：
    - 支持更小粒度的数据库保存（实时持久化，可以达到命令级别）；
    - AOF日志是append only的文件，写入时不需要查找，断电后也没有文件崩坏的问题，即便有写入了一半的命令， 也有自动恢复工具（redis-check-aof）可以快速解决；
    - **log rewrite？？？**
  - 缺点：
    - AOF日志一般比RDB快照大，而且用AOF重启大数据量的数据库速度较慢；
    - AOF日志的fsync策略制定不当时，备份开销依旧可能比RDB快照模式要大；
    - 通过执行命令的模式进行复制时，可能有主从不一致问题；
- 文档说计划合并两种方式（毕竟RDB好用，性能也不错，但AOF粒度比较好能保证更好的durability）

> https://redis.io/topics/persistence



### 4. 简述Redis的复制机制

- 主从复制模式，从节点可以自动连接到主节点请求复制信息，断开连接后从节点自动重连；
- 复制模式：
  - 命令流（command stream）：主从节点连接正常时，主节点按顺序发送命令行给从节点复制；
  - 部分重新同步（partial resynchronization）：主从节点连接断开并重连后，主节点先发送链接断开期间的命令流，再按命令流方式继续发送命令。部分重新同步的实现基于主节点的数据库快照使用`Replication ID, offset`结构来维护的，从节点将它保存的这份信息传送给主节点，主节点就会发送欠缺部分；
  - 完全重新同步（full resynchronization）：上述两种方式都失效后，从节点请求完全重新同步。此时主节点会缓存客户端输入的命令，同时启动进程生成RDB快照并传送给从节点，最后传送缓冲的命令。从节点接收RDB文件后载入，最后接受主节点发送的缓冲命令。
- 注意：主节点虽然可以知道从节点对命令流的执行情况，但Redis为保证低时延和高性能，默认采用异步复制方式；

> https://redis.io/topics/replication



### 5. Redis分布式锁？





<br />

<br />

----



<div align="center">by Divsigma@github.com</div>

