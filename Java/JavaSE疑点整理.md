# 前言

&ensp;&ensp;&ensp;&ensp;此篇旨在整理一些JavaSE中值得注意的疑点和区分点（应该会不断更新）。

&ensp;&ensp;&ensp;&ensp;内容尽量来源于Oracle JavaSE的官方文档，每问后尽量附上参考链接



<br />

<br />

# 正文

## 1. 基础

### 1. 为什么有Java？

- 最初用于为网络设备搭建应用，并解决软件的平台依赖问题

> See 1.1 of https://docs.oracle.com/javase/specs/jvms/se8/jvms8.pdf



<br />

<br />

## 1. JVM

### 1. 什么是JVM？

- 1）一个有着指令集并负责管理运行时内存的虚拟计算机
- 2）它与Java语言无关，只会识别并执行`.class`文件，该文件包含了JVM的指令和符号表（就像某一架构上的汇编代码）

> See 1.2 of https://docs.oracle.com/javase/specs/jvms/se8/jvms8.pdf



### 2. JVM支持的数据类型有哪些？

- 1）Primitive Type：
  - 在Java语言中有对应的：numerical type（byte、char、short、int、long、double、float）、boolean type
  - 在Java语言中没有对应的（仅用于JVM指令）：returnAddress type——指向JVM指令的操作码（opcodes of JVM instructions）

- 2）Reference Type：
  - 该类型对应Java语言中引用类型，在JVM中分三大类，class type、array type、interface type
  - array type：包含component type数据的一维数据类型，component type可以是JVM支持的数据类型（即嵌套）；但最后必须落实到一种element type——primitive type、class type或interface type

> See 2.2, 2.3 and 2.4 of https://docs.oracle.com/javase/specs/jvms/se8/jvms8.pdf



### 3.  JVM运行时数据分区是怎样的？各区作用？

- 1）PC Register
  - 每个线程独享自己的pc Register；
  - 若当前线程在执行的是native方法则pc为undefined；否则pc指向当前执行的指令地址（不同于IA-32架构的PC？）
- 2）JVM Stacks——类比Linux进程图像中`栈`段
  - 每个线程独享自己的JVM Stack；
  - 它由frame（栈帧）构成，保存局部变量与部分结果，帮助实现函数调用与返回（与Linux进程图像中`栈`段类似）；
  - frame可以被动态分配（may be heap allocated），无须连续（与Linux进程图像中`栈`段不同），可以被指定最大最小值（与Linux进程图像中`栈`段类似）；
  - 两种相关错误：
    - 当线程需求栈帧大小超过限定值时报`StackOverflowError`；
    - 当线程需求的栈帧在堆中无法进行分配（如空间不足或不足以给下一个线程分配最小栈帧）时报`OutOfMemoryError`
- 3）Heap
  - 所有线程共享一个Heap；
  - 它在JVM启动时即被创建，保存所有类实例（包含了非static的成员变量）与数组；
  - 它的空间管理由garbage collector管理，可以固定大小、动态缩小/扩大、指定最大最小值。GC系统方案由JVM的实现方提供；
  - 相关错误：
    - 当GC无法清理出满足要求的堆空间时报`OutOfMemoryError`
- 4）Method Area——类比Linux进程图像中`.text`段
  - 所有线程共享一个Method Area；
  - 它在JVM启动时即被创建，保存所有类的结构（在运行时常量池中，包含了static修饰的成员）、方法代码和构造器（与Linux进程图像中`.text`段功能类似）
  - 它可以固定大小或动态变化，它的空间逻辑上属于heap的一部分，但GC实现应该避免修改这部分区域大小与内容
  - 相关错误
    - 当没有足够的方法区空间供分配时报`OutOfMemoryError`
- 5）Constant Pool——类比Linux进程图像中`.text`段的`.symbol`部分
  - 属于Method Area的一部分
  - 它在每个类或接口被创建（通过loader加载）时创建；
  - 它保存每个类或接口的结构（描述信息，包括static修饰的成员），可以看作`.class`文件中`constant pool`表的”运行时状态“；
  - 相关错误：
    - 当没有足够的方法区空间供创建类或接口时报`OutOfMemoryError`
- 6）Native Method Stacks
  - 每个线程独享自己的Native Method Stack；
  - 它只为native method服务，基本特点及相关错误类似JVM Stacks

> - See 2.5 of https://docs.oracle.com/javase/specs/jvms/se8/jvms8.pdf
> - What's the meaning of `native` keyword in Java: https://www.geeksforgeeks.org/native-keyword-java/
> - What's the difference of `native code` and `bytecode`: https://www.quora.com/What-is-the-difference-between-bytecode-native-code-machine-code-and-assembly-code



### 4. 什么是GC？为什么要有GC？

- 1）什么是GC？
  - GC即Garbage Collector，是VM中自动管理内存的机制。
  - 它的功能主要有：
    - 从OS申请并分配内存
    - 确保仍被引用的对象留在内存（used by application）
    - 回收不再被引用的对象占用的内存（这种对象称为Garbage）
  - 它的评估指标主要有：
    - 吞吐量（Throughput）：一段时间内CPU用于非GC操作的时间占比；
    - 暂停时间（Pause Time）：GC发生时应用暂停的总时长；
    - Footprint（还不知咋翻译）：进程的工作集；
    - Promptness（同样还不知咋翻译orz）：对象实例被判定为垃圾到其占用内存被回收的时间
  - 它对内存的管理思想是世代收集（`Generational Collection`）：
    - 基于`Weak Geneartional Hypothesis`（即a majority of objects "die young"），内存被划分为`Generations`。最常见的就是划分为新生代区（`Young Generation`）和老年代区（`Old Generation`），对象被按照存活时间放入相应区域；
    - 不同区可以采用不同的收集算法，`Young Generation`的收集称为`Minor Collection`，所有`Generation`的收集称为`Major Collection`（或`Full Collection`）；
    - `Young Generation`的GC算法更关注时间效率（该区的GC发生次数多），`Old Generation`的GC算法更关注空间效率（该区占据大部分堆区域）
- 2）为什么要有GC？
  - 将程序员从繁琐的动态内存管理中解放，因为动态内存管理可能出现难以调试的bugs、发生`dangling reference`和`space leaks`

> - See 2 and 3 of https://www.oracle.com/technetwork/java/javase/memorymanagement-whitepaper-150215.pdf
> - See 1 and 3 of https://docs.oracle.com/javase/9/gctuning/introduction-garbage-collection-tuning.htm



### 5. HotSpot JVM中GC如何管理内存？

- 1）`Generational Collection`图谱（HotSpot Generations）

  - `Young Generation` + `Old Generation` + `Permanent Generation`。对象在`Young Generation`被实例化，经过若干次`Minor collection`后依旧存活的对象被移入`Old Generation`，`Old Generation`根据不同算法适时触发`Major Collection`（一般在`Old Generation`或`Permanent Generation`满时触发）。
  - `Young Generation` 被划分为1个`Eden`和2个`Survivor Space`。
  - 占用空间较大的对象会被直接分配到`Old Generation`；
  - `Permanent Generation`存放便于GC运作的数据，如类的结构描述、方法等。

- 2）Garbage Collector的常用算法

  - `Young Generation`中

    - 复制算法。

      &ensp;&ensp;&ensp;&ensp;大部分对象在`Eden`新生，在`Survivor Space`间辗转（一次`Minor Collection`会回收`Eden`和1个`Survivor Space`，完成一次辗转），辗转达到一定次数后则完成`Aging`（或称为`Promotion`），被移入`Old Generation`；

  - `Old Generation`中

    - Mark-Compact算法（“标记-整理”算法）

      &ensp;&ensp;&ensp;&ensp;识别存活的对象、滑动紧缩

    - Mark-Sweep算法（“标记-清除”算法）

      &ensp;&ensp;&ensp;&ensp;识别死亡的对象、标记为清除

> - See 4 of https://www.oracle.com/technetwork/java/javase/memorymanagement-whitepaper-150215.pdf
> - See 3 of https://docs.oracle.com/javase/9/gctuning/garbage-collector-implementation.htm
> - 受不了了，Oracle的资料好乱，找了一天都找不到网上讲的7种收集器
>   - 7种收集器：https://crowhawk.github.io/2017/08/15/jvm_3/
>   - 7种收集器：https://blogs.oracle.com/jonthecollector/our-collectors
>   - 7种收集器：https://juejin.im/post/6844903685374377998



### 6. HotSpot JVM的收集器有哪几种？

- 1）Serial Collector
  - 单线程。听说还分用于新生代的Serial和用于老生代的SerialOld
- 2）Parallel Collector
  - 多线程。听说还分用于新生代的ParNew（关注pause time）&ParallelScavenge（关注throughput）和用于老生代的ParallelOld，而且ParallelScavenge只能跟SerialOld或ParallelOld配合（？
- 3）CMS Collecotor (Concurrent-Mark-Sweep)
  - 初始标记（STW、单线程。标记GC Root）、并发标记（多线程）、重新标记（STW、多线程。处理并发标记阶段改变的标记）、并发清理（单线程？）
  - 官网也说CMS只是老生代GC算法，需要配合Parallel方式的新生代GC算法使用
- 4）G1 Collector
  - 初始标记（STW、单线程。标记GC Root）、并发标记（单线程？）、最终标记（STW、多线程。处理并发标记阶段改变的标记，将线程的Remembered Set Logs并入Region的Remebered Set）、筛选回收（因为G1实现可预测停顿，所以它会在满足停顿时间要求前提下，优先回收价值最大（？）的）
- 5）ZGC
  - Java11后的新特性！

> 受不了了，Oracle的资料好乱，找了一天都找不到网上讲的7种收集器
>
> - 7种收集器：https://crowhawk.github.io/2017/08/15/jvm_3/
> - 7种收集器：https://blogs.oracle.com/jonthecollector/our-collectors
> - 7种收集器：https://juejin.im/post/6844903685374377998



### 7. 为什么要GC调优？如何进行GC调优？

- 1）为什么要GC调优？
  - 因为大部分GC都采用了并行优化，这可能导致多核多线程处理器上GC占据过多处理器时间，进而导致应用吞吐量急剧下降（见参考链接中的Figure 1-1）。所以小规模系统上（如本地环境）可忽略的吞吐量问题部署到大规模系统上（如生产环境）后可能成为性能瓶颈；
  - 为解决上述问题，须根据应用场景、系统环境选择GC并进行调优
- 2）如何进行GC调优？
- 

> - See `Introduction` of https://docs.oracle.com/javase/9/gctuning/introduction-garbage-collection-tuning.htm









<br />

<br />

## 2. 数据类型





<br />

<br />

## 3. 关键字





<br />

<br />

## 4. 面向对象

### 4.1 为什么重写equals()还要重写hashcode()

- 1）原生的equals()和hashCode()是怎样的？

  &ensp;&ensp;&ensp;&ensp;原生的equals()判断引用是否相同，原生的hashCode()判断由内存地址生成的integer是否相同（是个运行时代码，所以每次运行结果可能不一样）；

- 2）为什么要重写equals()？

  &ensp;&ensp;&ensp;&ensp;业务上，我们常希望忽略对象的地址区别，而根据对象的某些属性特点便判断二者为“相等”，此时需要复写相应的equals()方法；

- 3）为什么要重写hashCode()？

  &ensp;&ensp;&ensp;&ensp;一方面是文档对二者的约定——equals()相同者hashCode()必须相同，反之不一定（见参考链接。可以理解成equals()是相比hashCode()更本质的“相等”）；

  &ensp;&ensp;&ensp;&ensp;另一方面，可以看出这个约定是有必要的——只是重写equals()在List容器中或许能够满足需求，因为它仅调用equals()判断是否相等；但对于采用了Hash的容器（如HashMap、HashSet、HashTable）则会产生矛盾的地方，因为采用Hash的容器先调用hashCode()判断相等，再调用equals()判断相等，若只按照业务逻辑重写equals()，很可能出现两个业务上相等的对象在Hash集合中重复出现或无法检索。这导致隐藏的bug；

- 4）所以记住以下原则：

  &ensp;&ensp;&ensp;&ensp;重写equals()必须重写hashCode()，且要保证equals()返回true时hashCode()也必然返回true。

> - [https://docs.oracle.com/javase/7/docs/api/java/lang/Object.html](https://docs.oracle.com/javase/7/docs/api/java/lang/Object.html)
> - [https://dzone.com/articles/working-with-hashcode-and-equals-in-java](https://dzone.com/articles/working-with-hashcode-and-equals-in-java)



### 4.2 interface和abstract class的区别





### 4.3 Override（重写）和Overload（重载）的区别

- 1）Override表现为两个原型相同的函数有不同的实现。通常体现在父子类关系上。

- 2）Overload表现为两个名字相同的函数有不同的原型或实现。一般能override都能overload？

- 3）能overload但不能override的情况有父类构造器、private和final方法等。

  &ensp;&ensp;&ensp;&ensp;事实上，父类构造器是无法被子类继承的（因为构造器是一个能操作类内成员且与类同名的函数，如果被子类继承为构造器，则破坏了封装性以及与类同名原则），而且没有返回值（无法看成函数显示调用）、子类对父类构造器override时编译器会报错。也正因为父类构造器无法被子类继承，所以子类的构造器在没有显式this()或super()时会自行调用super()进行构造。

> - https://www.geeksforgeeks.org/constructors-not-inherited-java/

<br />

<br />

----



<div align="center">by Divsigma@github.com</div>