# 前言

&ensp;&ensp;&ensp;&ensp;此篇旨在整理一些JavaSE中值得注意的疑点和区分点（应该会不断更新）。



<br />

<br />

# 正文

## 1. JVM





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