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



### 2. MySQL存储引擎有哪些？它们的区别？

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



### 3. 什么是事务？事务的特性？

- 1）什么是事务？

  &ensp;&ensp;&ensp;&ensp;事务是DBMS执行过程中的一个逻辑操作单元，由操作序列组成，这些操作要么全做要么全不做。

- 2）事务的特性？

  &ensp;&ensp;&ensp;&ensp;就是ACID：

  - Atomicity：事务定义；
  - Consistency：事务执行结果使数据库从一个一致性状态（只包含事务成功执行的状态）转到另一个一致性状态，这是保证原子性的结果；
  - Isolation：并发事务操作互不影响；
  - Durability：提交事务造成数据库改变是“永久”的/可恢复的；

> - Explanation of ACID in MySQL document: https://dev.mysql.com/doc/refman/8.0/en/glossary.html#glos_acid
>
> - 代码实现事务：
>
>   - Spring中6种事务写法概述：https://developer.ibm.com/zh/articles/os-cn-spring-trans/
>     - Transactional注释机制：https://developer.ibm.com/zh/articles/j-master-spring-transactional-use/
>   - Laravel中编程式事务写法：https://laravel.com/docs/7.x/database#database-transactions
>
>   - 有代码实例：https://juejin.im/post/6844903608694079501



### 4. 事务隔离级别有哪些？

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



### 5. 简述MySQL索引机制

- 1）什么是索引？为什么用索引？

  - 索引是数据库中用于加快基于特定列查询的数据结构，使用索引能提高MySQL的语句执行效率/操作表的效率；
  - 如何知道一个索引是否起作用呢？首先看`EXPLAIN`的`Extra`栏有没有`Using index`，再`ALTER`表设置索引为`invisible`，若执行出错/`EXPLAIN`不同/查询变慢了，基本可以认为被隐藏索引起了优化查询作用。（`invisible`属性的索引依旧会随着表的更新而更新）

- 2）MySQL索引用到的数据结构有几种？

  - 大致有两种：B-tree及其变种（B+-tree、R-tree）、Hash。大部分引擎如`InnoDB`和`MyISAM`用B+-tree，`MEMORY`引擎常用Hash，对空间数据类型（spatial data type）用R-tree：https://dev.mysql.com/doc/refman/8.0/en/mysql-indexes.html

    

  - B+-tree Index搜索特性：

    - 适用于范围查找、用于`ORDER BY`和`GROUP BY`优化、一些特定情况的多条件查找（如`WHERE`条件是多个`AND` groups有作为多列索引的leftmost prefix的交集、`LIKE`条件是不以通配符为开头的常量字符串）；
    - 当需要返回表中大部分数据时，全表扫描可能会替代索引查找（但语句使用`LIMIT`时总是使用索引）；
    - `AND` groups：`WHERE`语句中一段连续的`AND`穿起来的条件就是一个`AND` groups。注意MySQL中`AND`优先级高于`OR`；

  - Hash Index搜索特性：

    - 适用于等值查找，适用于有全键随机查找，不适合范围查找、模糊匹配（因为hash会match the whole key）和`ORDER BY`优化（因为hash出来的东西难说有顺序）；

- 3）MySQL索引有哪些？

  - 聚簇索引（clustered index）：InnoDB表的行索引。通常是以主键建立的索引。InnoDB用聚簇索引管理所有表，**行级锁也是加在索引上的**，所以如果没有显式声明主键而且没有合适的唯一非空列，引擎会在每行的隐藏字段上自动创建聚簇索引`GEN_CLUST_INDEX`；

    **存疑**：可能是因为InnoDB的数据是存放在B+-tree叶子节点上的，区别于MyISAM的B+-tree——MyISAM中索引文件和数据文件分离。可能是因为这个，InnoDB中必须要有一个管理表的索引，而且叫聚簇索引。

  - 辅助索引？（secondary index）：除聚簇索引以外的其他索引；

  - 哈希索引（hash index，用于`MEMORY`引擎）、自适应哈希索引（adaptive hash index，用于`InnoDB`引擎的Buffer Pool）、R-tree索引；

  - 多列索引（multiple-column indexes）：MySQL会根据索引的leftmost prefix（“前缀和”）来使用索引优化查询，所以要合理安排多列索引中索引列顺序（多列索引可以看成一个用各列值连接后得到的值作为排序依据的有序数组）；
  - 覆盖索引（covering index）：查询只用到索引列中的信息（`SELECT key_part3 FROM tbl_name WHERE key_part1=1`），则查询结果可以从索引树直接返回（不需要读取索引指向的数据块了）；
  - 拓展索引（index extension）：InnoDB自动在secondary index后加上primary key index组成多列索引，有时可以优化查询效率。比如，当`WHERE`条件的`AND` groups中有secondary index和primary key index时，MySQL可以直接利用拓展的secondary index前缀能获取结果（而不是先用拓展前secondary index查找再用where对结果过滤）；
  - 降序索引（descending index）：主要用于`ORDER BY DESC`语句中防止触发文件排序；

- 4）基于索引的基本优化有哪些？
  - 利用覆盖索引，直接返回索引树信息，避免读取对应的行数据；
  - 利用B+-tree型索引的有序性，`ORDER BY`和`GROUP BY`可以直接基于索引操作返回结果，避免文件排序；
  - 利用B+-tree型索引的有序性，用降序索引应对`ORDER BY DESC`，避免文件排序；
  - 按信息查询热度分表，并用外键索引提高join速度（这样热门信息表一个blocks可以存储更多行，减少了整体I/O）；
  - 但有时用顺序扫描比用索引更快：
    - 小表（小于10行或每行都很短）；
    - `ON`或者`WHERE`中含有索引，但匹配的值对应了表的大部分数据；
    - 可以通过`FORCE INDEX`或设置`--max-seeks-for-key`指定全表扫描的阈值（但default value也挺大的，2^20）

> - What is `AND` groups: https://stackoverflow.com/questions/40778142/clarification-of-mysql-index-required-to-be-spanning-of-all-and-groups
> - Glossary `Index`: https://dev.mysql.com/doc/refman/8.0/en/glossary.html#glos_index
>   - Physical Structure of an InnoDB Index: https://dev.mysql.com/doc/refman/8.0/en/innodb-physical-structure.html
>   - Clusetered and Secondary Indexes (InnoDB): https://dev.mysql.com/doc/refman/8.0/en/innodb-index-types.html
> - Optimization and Indexes: https://dev.mysql.com/doc/refman/8.0/en/optimization-indexes.html



### 6. 什么是索引重建？为什么要索引重建？怎么索引重建？索引重建的原理？

（挖坟）

> https://www.cnblogs.com/Alight/p/4601003.html



### 7. `ORDER BY`过程及其优化（B+-tree索引）

- 1）过程

  - 优先考虑`ORDER BY`字段是否有可用索引及索引效率，若可用且效率高则使用索引排序，否则触发`filesort`

  - `ORDER BY`字段含索引，但不可用的情况大致有：

    - 字段中的索引不是有序的：`MEMORY`引擎表中的索引是Hash索引；

    - 字段中含有两个不同索引：`SELECT * FROM t1 ORDER BY key1, key2`
    - 字段中索引不是某个索引的leftmost prefix：`SELECT * FROM t1 ORDER BY key1_part1, key1_part3` / 只对`CHAR`类型字段的部分字节加了索引但`ORDER BY`要求字段所有字节；
    - 字段中含的是索引表达式：`SELECT * FROM t1 ORDER BY -key1` / `SELECT * FROM t1 ORDER BY ABS(key1)` 

  - `ORDER BY`字段含索引且可用，但索引效率不高的情况有：

    - 原则：扫表效率比用索引高，则即便有索引也不用。如`SELECT * FROM t1`获取的列信息远多于`ORDER BY`使用到的索引信息，此时`filesort`可能更高效；

- 2）优化

  - 优化主要集中在：
    - 避免`SELECT`信息太多导致引擎不使用可用索引；
    - 避免低效地使用索引（高I/O低CPU时可能是这种情况，可以降低`max_length_for_sort_data`来触发`filesort`。但该8.0.20后被砍掉了，那咋办？？？）
    - 提高`filesort`过程效率
    - （学会看`EXPLAIN`
  - 提高`filesort`过程效率
    - 使`filesort`尽量在内存进行并避免产生中间文件，如：`SELECT`语句中考虑加入`LIMIT`以减少待排序记录的大小；提高`max_sort_length`（用于排序的单条记录的最大字节数）和`sort_buffer_size`（8.0.20后该值表示MySQL在`filesort`时动态申请内存的上界，更加灵活。以前版本中该值代表用于`filesort`的内存固定大小，可能导致内存浪费）；
    - 注意设置`tmpdir`保证其中有足够大空间给中间文件（该配置中文件按照round-robin方法被使用）；
    - 提高`read_rnd_buffer_size`以提高一次读取的行数（？

> https://dev.mysql.com/doc/refman/8.0/en/order-by-optimization.html



### 8. `GROUP BY`过程及其优化（B+-tree索引）

- 1）过程
  - 优先考虑`GROUP BY`是否有可用索引，若有则考虑索引优化，否则扫表再生成临时表；
  - 根据`GROUP BY`字段、`SELECT`的字段和`WHERE`条件情况，引擎采用两种不同的index access方式：`Loose Index Scan`和`Tight Index Scan`；
  - `Loose Index Scan`和`Tight Index Scan`：
    - `Loose Index Scan`只读取`GROUP BY`字段的不同值的第一条索引记录（如果有`WHERE`子句则构造区间继续定位起始点），接着由引擎决定下一条不同值的记录的地址，直接跳转继续读取。查找下一条记录的算法是一种由引擎决定的hash算法，见博客解析：https://mysqlserverteam.com/what-is-the-scanning-variant-of-a-loose-index-scan/
    - `Tight Index Scan`会扫描全部所有符合`WHERE`的index（若没有`WHERE`则全索引扫描）；

- 2）优化
  - `Loose Index Scan`须要`GROUP BY`的字段是同一个索引的leftmost prefix，否则会考虑使用`Tight Index Scan`（扫描全部index进行过滤）。此时使用`Tight Index Scan`还需要一个额外条件`GROUP BY`字段之间的"gap"需要用`WHERE gap_key_part=const`"填充"；
  - `Loose Index Scan`的触发还有其他必要条件：`SELECT`的聚集函数是`MIN()`/`MAX()`/`SUM()`/`COUNT()`/`AVERAGE()`以及其中使用的索引列是紧跟在`GROUP BY`所用前缀索引列的下一列、`SELECT`中包含的非聚集函数中索引列或非`GROUP BY`字段索引列需要用`WHERE key_part_not_good=const`

> - https://dev.mysql.com/doc/refman/8.0/en/group-by-optimization.html
>
> - 结合`EXPLAIN`的实现解析：https://mysqlserverteam.com/what-is-the-scanning-variant-of-a-loose-index-scan/



<br />

<br />

## 2. InnoDB存储引擎

### 1. InnoDB有几种Isolation level？各自特点是？

- 1）REPEATABLE_READ

  - 处理方式：

    - 锁机制处理`locking reads`、`UPDATE`和`DELETE`，解决`Phantom`，实现写可串行化：若操作对应由record index决定的唯一一条数据，则采用`Record Lock`。否则用`Next-key Lock`；

      加`Next-key Lock`方法：首先扫描表索引，给扫描到的所有索引记录加`S锁`/`X锁`（此时row-level locks就是index-record locks！锁的是索引！），然后给这些记录之间加`Gap Lock`。

    - MVCC（Multi-versioned Concurrency Control）处理`不加锁SELECT`，解决`Non-repeatable Read`，实现读一致性：`不加锁SELECT`（即没有`FOR UPDATE`等字段的`SELECT`）属于`Consistent Read`操作，在同一事务中，`SELECT`的结果都是本事务中第一次出现相同`SELECT`时产生的snapshot。

      注意：snapshot仅对`SELECT`的查询结果产生影响，不会限制其他DML语句的行为——尽管当前事务的`SELECT`语句看不到最新数据，事务（包括当前事务）的修改仍然是作用于最新数据（例子见参考链接中Consistent Nonlocking Reads）。

  - 性能：

    解决了`Non-repeatable Read`和`Phantom`，一致性近似达到`SERIALIZABLE`，并发性能较`SERIALIZABLE`优秀，但比`READ_COMMITTED`差且发生死锁几率更大。它是InnoDB的默认隔离级别。

- 2）READ_COMMITTED

  - 处理方式：
    - 锁机制处理`locking reads`和`DELETE`：按索引搜索，最后用`Record lock`仅锁住符合条件的索引记录，而且不锁住记录间隔；
    - MVCC处理`不加锁SELECT`：不加锁的`SELECT`也属于`Consistent Read`操作，`SELECT`的结果都是最新的snapshot；
    - 用锁机制+半一致性读（semi-consistent read）+MVCC处理`UPDATE`：若`UPDATE`时对应记录已加锁，则读取对应记录最新版本进行条件匹配，若符合条件再继续请求读取原记录（等待原锁释放或对原记录加`Record lock`）；

  - 性能：

    可能存在`Non-repeatable Read`和`Phantom`，一致性不如`REPEATABLE_READ`，但并发性能较`REPEATABLE_READ`优秀且发生死锁几率更小（没有`Gap Lock`而且引入`semi-consistent read`）。

- 3）READ_UNCOMITTED

  - 最简单的情况，可能出现`Dirty Read`。

- 4）SERIALIZABLE

  - 用锁应对所有操作：与`REPEATABLE_READ`相比，它不同在于将`不加锁SELECT`转为`locking reads`（`autocommit=OFF`时）或单独事务（`autocommit=ON`时）（只读事务？？）。

> - InnoDB Locking: https://dev.mysql.com/doc/refman/8.0/en/innodb-locking.html#innodb-intention-locks
> - Transaction Isolation Levels: https://dev.mysql.com/doc/refman/8.0/en/innodb-transaction-isolation-levels.html
> - Consistent Nonlocking Reads: https://dev.mysql.com/doc/refman/8.0/en/innodb-transaction-isolation-levels.html
> - 别人的总结：https://www.cnblogs.com/rjzheng/p/10510174.html



### 2. 为什么InnoDB默认采用`REPEATABLE_READ`？

- 据说是历史原因（还没在官网找到根据）：

  &ensp;&ensp;&ensp;&ensp;MySQL采用Binlog记录操作，用于复制和恢复。Binlog按照串行化的操作顺序记录，记录格式有三种：`STATEMENT`、`ROW`和`MIX`。

  &ensp;&ensp;&ensp;&ensp;`STATEMENT`按照事务的`COMMIT`顺序记录，`ROW`根据各行数据实际变更顺序记录，`MIXED`综合前两种方式（具体待补充）。MySQL5.1以前`binlog_forma`只有`STATEMENT`格式，5.1以后才有`ROW`和`MIXED`格式。但根据`READ_COMMITTED`级别下Binlog复制出来的结果可能产生写丢失，如：

  &ensp;&ensp;&ensp;&ensp;`T1`先`DELETE` `行R`，`T2`再`INSERT` `行R`，`T2`先`COMMIT`，`T1`再`COMMIT`。

  &ensp;&ensp;&ensp;&ensp;上述序列原执行结果应该有`行R`，但根据按`COMMIT`顺序记录的Binlog还原出来的结果则无`行R`

  &ensp;&ensp;&ensp;&ensp;所以MySQL默认采用了会对`UPDATE`、`DELETE`和`locking reads`操作加锁的`REPEATABLE_READ`，保证了写可串行化。如果需要在`READ_COMMITTED`级别支持写可串行化，需要设置`binlog_format`为`ROW`或`MIXED`。

> - 别人的实验结果：https://www.cnblogs.com/vinchen/archive/2012/11/19/2777919.html
> - "Only row-based binary logging is supported with the `READ_COMMITTED` ": https://dev.mysql.com/doc/refman/8.0/en/innodb-transaction-isolation-levels.html



### 3. InnoDB的缓存机制是怎样的？有什么用（&应用场景）？

- 总的来说就是将某个query的结果存到内存，利用query语句得出的hash值作为其索引（直接通过query语句的字节得出的hash值，即便是语句中有注释也会得到不同hash值）。
- QueryCache减少了下次再次执行相同query语句（字节意义上的相同！）时的优化和读取时间，提高了查询速度。多用于查询操作远大于修改操作的数据库（如个人博客？）。
- 但对于修改较为频繁的数据库而言QueryCache的局限性较大：首先每一条`SELECT`语句需要经过hash计算和查询。其次因为对所查表的任何改变都会导致QueryCache中相关的结果失效，进而造成内存碎片增多，又带来了整理内存的开销。MYSQL5.7.20以后就deprecated了，MYSQL8.0以后就removed了。所以最好能在应用层有其他缓存机制替代MySQL的查询缓存。

> https://zhuanlan.zhihu.com/p/55947158



### 4. InnoDB的缓冲机制是怎样的？有什么用？

- 1）组成部分：

  - Buffer Pool：

    - 内存中一块区域（专用服务器上可以占到服务器内存的80%），用于保存表和索引数据，供用户SQL语句操作。
    - 以页（pages）为单位管理（InnoDB中默认为16KB），被安排成pages list，使用LRU算法维护。前5/8为`New Sublist`，保存至少被用户查询一次的数据；后3/8为`Old Sublist`保存刚从磁盘中读取的数据（包括预取数据）。读取的数据从`midpoint`（即`Old Sublist`开头）插入。

    - 定期执行`flush`/`purge`操作将`Buffer Pool`中内容按blocks批量写入磁盘
    - 可以结合`Adaptive Hash Index`优化元素的获取

  - Change Buffer

    - 内存中的一块区域（占用`Buffer Pool`一部分），用于保存二级索引数据上（但该数据不在`Buffer Pool`中）的修改。
    - 当对应的二级索引页被读入到`Buffer Pool`后，`Change Buffer`中相应内容会`merge`到`Buffer Pool`（因该过程可能耗费若干小时，这段时间的磁盘I/O可能会增加，成为应用瓶颈）。最后由`Buffer Pool`批量更新到磁盘。

  - Log Buffer

    - 记录要被写到磁盘log文件的数据

- 2）作用：

  - 缓解磁盘I/O读取和CPU处理速度不匹配问题，减少**磁盘**I/O，加快查询语句**获取数据**的速度

> - 相比基于Redis的查询，MySQL的缓冲查询依旧逊色不少（可能是去除QCache后，MySQL的查询还有语句解析优化等过程耗时），此处有别人的粗略实验结果：
>   - https://segmentfault.com/a/1190000005034197
>   - https://cloud.tencent.com/developer/article/1636473



### 5. InnoDB线程

（挖坟）

>- https://www.cnblogs.com/javazhiyin/p/13277580.html





<br />

<br />

## 3. 调优



<br />

<br />

## 4. 架构



<br />

<br />

----



<div align="center">by Divsigma@github.com</div>