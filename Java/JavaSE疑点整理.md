# 前言

&ensp;&ensp;&ensp;&ensp;此篇旨在整理一些JavaSE中值得注意的疑点和区分点（应该会不断更新）。

&ensp;&ensp;&ensp;&ensp;内容尽量来源于Oracle JavaSE的官方文档，每问后尽量附上参考链接



<br />

<br />

# 正文

## 1. 基础

### 1. 为什么有Java

- 最初用于为网络设备搭建应用，并解决软件的平台依赖问题

> See 1.1 of https://docs.oracle.com/javase/specs/jvms/se8/jvms8.pdf



<br />

<br />

## 1. JVM

### 1. 什么是JVM

- 1）一个有着指令集并负责管理运行时内存的虚拟计算机
- 2）它与Java语言无关，只会识别并执行`.class`文件，该文件包含了JVM的指令和符号表（就像某一架构上的汇编代码）

> See 1.2 of https://docs.oracle.com/javase/specs/jvms/se8/jvms8.pdf



### 2. JVM支持的数据类型有哪些

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



### 4. JVM的堆是由GC管理的，那么GC如何管理Heap？



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