# 前言

&ensp;&ensp;&ensp;&ensp;此篇旨在整理一些MySQL中值得注意的疑点和区分点（应该会不断更新）。

&ensp;&ensp;&ensp;&ensp;对问题的解答尽量来源于MySQL官方文档或标准文件，每问后尽量附上参考链接



<br />

<br />

# 正文

## 1. 基础

### 1. 什么是MySQL？它的特点是？

- 1）什么是MySQL？

  &ensp;&ensp;&ensp;&ensp;MySQL是SQL数据库的一种实现，目前仍然GPL开源（09年Sun被Oracle收购，原作者开启了Mariadb项目，意图取代MySQL）

- 2）MySQL的特点是？

  - 关系型的数据库、支持内核级多线程、支持网络连接与SSL的安全机制、社区活跃、支持多平台（类Unix、Windows到MacOS）；
  - 对大数据的支持不好（不支持自动分片，难拓展而且要手工维护）、基于内存的读写不够强健导致高并发时掉性能。

> - https://dev.mysql.com/doc/refman/8.0/en/features.html
> - http://makble.com/the-advantages-and-disadvantages-of-mysql
> - https://www.gridgain.com/resources/blog/5-limitations-mysql-big-data



### 2. 什么是事务？事务的特性？

- 1）什么是事务？

  &ensp;&ensp;&ensp;&ensp;事务是DBMS执行过程中的一个逻辑操作单元，由操作序列组成，这些操作要么全做要么全不做。

- 2）事务的特性？

  &ensp;&ensp;&ensp;&ensp;就是ACID：

  - Atomicity：事务定义；
  - Consistency：事务执行结果使数据库从一个一致性状态（只包含事务成功执行的状态）转到另一个一致性状态，这是保证原子性的结果；
  - Isolation：并发事务操作互不影响；
  - Durability：提交事务造成数据库改变是“永久”的/可恢复的；

> https://juejin.im/post/6844903670916579336



### 3. 事务隔离级别有哪些？

- 1）什么是隔离级别（Isolation Levels）？

  &ensp;&ensp;&ensp;&ensp;`事务隔离级别`的概念由SQL标准引出，它基于并发事务会对事务本身及SQL-data/schema产生不同影响，定义了事务操作对SQL-data和schemas的操作等级。SQL:1992标准中默认事务是`SERIALIZABLE`

  &ensp;&ensp;&ensp;&ensp;事务隔离级别根据SQL-transaction并发过程中可能产生的数据不一致现象进行划分

- 2）3种数据不一致性情况

  - Dirty Read：`T1`读取`行R`，`T2`修改`行R`。

    `T2`修改了`R`后，`T1`读取了`R`，但此时`T2` `ROLLBACK`了对`R`的修改，导致`T1`读取到`T2`未`COMMIT`的数据；

  - Non-repeatable Read：`T1`读取`行R`两次（同一事务中），`T2`修改`行R`。

    `T1`第一次读取`R`后，`T2` `COMMIT`了修改`R`/删除`R`的操作，导致`T1`第二次读取的`R`不等于第一次的`R`；

  - Phantom：`T1`读取满足`条件C`的`集合S`两次（同一事务中），`T2`添加满足`条件C`的`集合S0`（其中行数≥1行）

    `T1`第一次读取`S`后，`T2` `COMMIT`了添加`S0`的操作，导致`T1`第二次读取的`S`不等于第一次读取的`S`；

- 3）4种事务隔离级别

  &ensp;&ensp;&ensp;&ensp;打勾表示可能发生，否则不可能；

  |      Level       | Dirty Read | Non-repeatable Read | Phantom |
  | :--------------: | :--------: | :-----------------: | :-----: |
  | READ_UNCOMMITTED |     √      |          √          |    √    |
  |  READ_COMMITTED  |            |          √          |    √    |
  | REPEATABLE_READ  |            |                     |    √    |
  |   SERIALIZABLE   |            |                     |         |

  

> - Search `Isolation level` in https://www.contrib.andrew.cmu.edu/~shadow/sql/sql1992.txt



### 4. InnoDB有几种Isolation level？各自特点是？







### 5. 为什么MySQL的InnoDB默认采用`REPEATABLE_READ`？

- 据说是历史原因（还没在官网找到根据）：

  &ensp;&ensp;&ensp;&ensp;MySQL采用Binlog记录操作，用于复制和恢复。Binlog按照串行化的操作顺序记录，记录格式有三种：`STATEMENT`、`ROW`和`MIX`。

  &ensp;&ensp;&ensp;&ensp;`STATEMENT`按照事务的`COMMIT`顺序记录，`ROW`根据各行数据实际变更顺序记录，`MIXED`综合前两种方式（具体待补充）。MySQL5.1以前`binlog_forma`只有`STATEMENT`格式，5.1以后才有`ROW`和`MIXED`格式。但根据`READ_COMMITTED`级别下Binlog复制出来的结果可能产生写丢失，如：

  &ensp;&ensp;&ensp;&ensp;`T1`先`DELETE` `行R`，`T2`再`INSERT` `行R`，`T2`先`COMMIT`，`T1`再`COMMIT`。

  &ensp;&ensp;&ensp;&ensp;上述序列原执行结果应该有`行R`，但根据按`COMMIT`顺序记录的Binlog还原出来的结果则无`行R`

  &ensp;&ensp;&ensp;&ensp;所以MySQL默认采用了会对`UPDATE`、`DELETE`和`locking reads`操作加锁的`REPEATABLE_READ`，保证了写可串行化。如果需要在`READ_COMMITTED`级别支持写可串行化，需要设置`binlog_format`为`ROW`或`MIXED`。

> - 别人的实验结果：https://www.cnblogs.com/vinchen/archive/2012/11/19/2777919.html
> - "Only row-based binary logging is supported with the `READ_COMMITTED` ": https://dev.mysql.com/doc/refman/8.0/en/innodb-transaction-isolation-levels.html



for 4. 

- InnoDB是一个multi-versioning存储引擎（为了支持事务的并发和回滚），数据行R的多版本信息被存放到rollback segment，InnoDB会给行R新增DB_ROLL_PTR字段以指向对应的rollback segment中对应的undo log record。InnoDB利用DB_ROLL_PTR指向的信息进行回滚、提供consistent read
- consistent read是一种利用snapshot呈现查询信息的读取操作，该读取操作不限制其他事务对查询数据的修改。RR和RC级别对SELECT默认采用这种机制。
- undo log record分insert和update两种，前者在事务提交后即可抹去，后者还需要为consistent read服务直到被服务的事务提交（所以要定期commit事务！否则rollback segment很大！）
- 即避免锁机制而采用读取snapshot
- 对于一个事务中的non-locking SELECT，均读取第一次读取时产生的snapshot（如果是READ_COMMITTED则读取fresh snapshot）
- cluster index -- InnoDB对primary key index的称呼，InnoDB会按照cluster index安排表结构（每张表都要有cluster index，否则InnoDB会自动选取或生成隐藏索引DB_ROW_ID）
- secondary index -- 除cluster index以外的索引，它包含primary key columns（给InnoDB在cluster index中查找相应的行）和索引特定的columns
- Record lock -- 索引行上的锁。因InnoDB的表记录总有cluster index，所以该定义总是有意义的；
- Gap lock -- 索引行之间的锁。用于防止两个索引行对应索引值之间被插入新记录（purely inhibitive）
- InnoDB的REPEATABLE_READ用锁机制应对locking reads、UPDATE和DELETE：按情况选择record lock或gap lock或next-key lock；
- InnoDB的REPEATABLE_READ用consistent read应对plain SELECT（而不是锁机制）的Non-repeatable read：相同SELECT语句的query结果都是事务中第一次出现该语句时产生的snapshot。注意snapshot仅对SELECT语句的查询结果产生影响，不会限制其他DML语句的行为——事务（包括当前事务）仍然修改表的最新数据，尽管当前事务的SELECT语句无法看到修改的结果（例子见官网的Consistent Nonlocking Reads）。如果能看到最新修改结果，那就是READ_COMMITTED或者执行了locking reads；
- InnoDB的REPEATABLE_READ用next-key locking防止Phantom：首先扫描表索引，给扫描到的所有index record加S/X锁（此时row-level locks就是index-record locks），然后给这些records之间加gap lock（锁住每个index record之前的索引值，防止更新）





<br />

<br />

## 2. 架构

### 1. MySQL存储引擎有哪些？它们的区别？

- 1）存储引擎有：InnoDB、MyISAM、MEMORY（保存在内存）、NDB（5.7+支持，提供了MEMORY引擎的功能，但可用性更高且支持集群化）、CSV（用以逗号分隔的csv文件保存数据）、MERGE、ARCHIVE、FEDERATED等等

- 2）主要讲讲InnoDB和MyISAM引擎的区别：

  |                          |  InnoDB   |   MyISAM   |
  | :----------------------: | :-------: | :--------: |
  |      B-tree Indexes      |     √     |     √      |
  | Full-text Search Indexex |     √     |     √      |
  |    Transaction(ACID)     |     √     |            |
  |       Data Caches        |     √     |            |
  |       Foreign Key        |     √     |            |
  |           MVCC           |     √     |            |
  |      Storage Limits      |   64TB    |   256TB    |
  |   Locking granularity    |    Row    |   Table    |
  |   Application Scenario   | (Default) | Read-heavy |

> See Chapter 15 and 16 of https://dev.mysql.com/doc/refman/8.0/en/innodb-storage-engine.html



<br />

<br />

## 3. 调优



<br />

<br />

----



<div align="center">by Divsigma@github.com</div>