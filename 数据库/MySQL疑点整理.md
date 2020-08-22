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



### 4. 为什么MySQL的InnoDB默认采用`REPEATABLE_READ`







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