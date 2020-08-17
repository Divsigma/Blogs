# **目录**

[TOC]





# **前言**

&ensp;&ensp;&ensp;&ensp;Spark源码编译断断续续地搞了三天，期间因为虚拟机存储空间、网络等各种问题卡了很久，但幸好最后没有放弃，终于踩到不少坑并成功了。所以写下此文做些记录。

&ensp;&ensp;&ensp;&ensp;这篇文章主要记录一些采用**Maven**将**Spark2.4.3编译为支持Hadoop2.6.0-CDH5.13.0**过程中碰到的坑和自己的编译小结。



<br />

<br />

# **正片**

## **1. 阅读官网**

&ensp;&ensp;&ensp;&ensp;官网永远是最权威的。参见[building-spark in Spark Document 2.4.3](http://spark.apache.org/docs/2.4.3/building-spark.html "building-spark in Spark Document 2.4.3")

&ensp;&ensp;&ensp;&ensp;Spark源码编译中需要注意的前置条件主要有：

- Java版本：Spark2.2.0后Java7不再适用，一般使用Java8

- Maven版本：Maven编译Spark中，Spark对Maven版本应该是向上兼容的，所以对Spark2.4.3用Maven3.5.4+应该都没问题（我还没试过）

- JVM设置：即官网中MAVEN_OPT设置，因为Spark编译过程中需要JVM有较多内存空间，比如`-Xmx2g`代表Spark编译过程中JVM最多要2G的堆空间（这意味着自己的虚拟机内存要2G+），`-XX:ReservedCodeCacheSize`用于提高JVM中编译速度。

  

&ensp;&ensp;&ensp;&ensp;**当时就应该关注命令行的含义！就能避免踩下述`zinc进程被杀死`的坑了！**



> 背景知识：
>
> - code cache用于存放JVM生成的native code，主要是JIT编译器生成的Java方法机器码；
> - 它一般被设置为32M/48M，对大多数Java应用都是够用的——应用在执行的整个过程中JIT都可以无忧无虑地编译代码放入code cache；
> - degradation的原因有很多：
>   - JIT对 code cache的flushing占用CPU
>   - JIT对new "hot" codes的compile占用CPU
>   - 这段时间内代码执行恢复慢速的interpreted execution等等；
> - 具体的flushing策略也有考究，常用的是speculative flushing——类似LRU机制
>
> 
>
> 参考链接：
>
> - Configuring Apache Maven：https://maven.apache.org/configure.html
> - Java Virtual Machine Configuration：https://docs.oracle.com/cd/E13174_01/alui/deployment/docs604/maintenance/a_jvm_switches.html
> - Java Tuning：
>   - https://docs.oracle.com/cd/E13222_01/wls/docs81/perform/JVMTuning.html
>   - https://juejin.im/post/6844903601786060808
>   - https://cloud.tencent.com/developer/article/1408773
>
> - Java Code Cache Size：https://docs.oracle.com/javase/8/embedded/develop-apps-platforms/codecache.htm#JEMAG207
>
> - Difference among four kinds of code：https://www.quora.com/What-is-the-difference-between-bytecode-native-code-machine-code-and-assembly-code



<br />

<br />

## **2. 编译与坑**

### **2.1 一行命令跑编译**

&ensp;&ensp;&ensp;&ensp;我编译成功后，总结出的一个基本思想是：以官网为编译大纲，以报错信息为debug主要依据，以思考和搜索引擎为debug主要辅助。

&ensp;&ensp;&ensp;&ensp;阅读官网发现Maven编译Spark出奇的简单。采用下述两种命令之一即可：

- ./build/mvn：该命令是编译Spark的主要命令，可以在本机上编译出直接可用的Spark；

- ./dev/make-distribution.sh：该命令可用于编译生成DIY的spark-bin发行版，本质还是会调用./build/mvn（具体可看脚本）。如果需要搭建集群时，常采用方式；



&ensp;&ensp;&ensp;&ensp;本次编译采用第2种命令。编译命令行基本参数如下：

- --name：指明编译出来的bin文件压缩包名字`name`，最后文件名是spark2.4.3-bin-`name`.tar.gz

- --tgz：指明压缩格式
- -P：spark源码目录下的pom.xml中有许多spark可选的profile，该关键字指明需要spark支持的profile的id
- -D：同上，用于修改编译时spark的pom.xml中设置。该关键字指明编译spark时环境属性参数

&ensp;&ensp;&ensp;&ensp;例如下面的命令，就是指明编译出来的spark是在hadoop-2.6上运行、支持hive、支持hive-thriftserver、支持yarn，同时与之配合的hadoop版本是2.6.0-cdh5.13.0（mvn会去**spark目录下pom.xml**指定的仓库中寻找这个版本的hadoop）

```shell
$ ./dev/make-distribution.sh \
--name spark2.4.3-cdh5.13.0 \
--tgz \
-Phadoop-2.6 -Phive -Phive-thriftserver -Pyarn \
-Dhadoop.version=2.6.0-cdh5.13.0
```



> - 为了加快编译脚本执行速度，还可以根据写好的命令行直接修改脚本文件中相关变量
>
>   具体可参见<https://blog.csdn.net/weixin_38163331/article/details/89791384>

​	

&ensp;&ensp;&ensp;&ensp;无奈碰到很多问题。  我之前以为“官网肯定啥都有”，但事实上官网更多提供的是编译步骤、常见问题可能发生的原因和解决方案（甚至可能没有解决方案）。这个时候碰到bug就要`看日志` + `思考` + `尝试` + `看日志`  +  `搜索` + `思考` + `尝试` + `看日志`  +  `···`。



<br />

### **2.2 磕磕碰碰爬坑**

#### **2.2.1 `protocol version`和`parent.relativePath指向`问题**

&ensp;&ensp;&ensp;&ensp;一般采用Java7环境进行编译会产生该问题。

&ensp;&ensp;&ensp;&ensp;因为Java进程无法连接到maven的中央仓库（或其他库也可能），所以Java无法从中央库下载相应的pom.xml文件，进而导致**spark的pom.xml**中`parent.relativePath`指向空文件（默认指向build目录。正常的话，pom.xml文件会被下载到build目录）

&ensp;&ensp;&ensp;&ensp;该问题产生主要是因为maven的中央仓库改用了TLS1.2协议，而Java7默认还是使用TLS1.1，所以Java进程无法连接中央仓库。

&ensp;&ensp;&ensp;&ensp;Java7环境中，若要解决该问题，在环境变量MAVEN_OPTS中加入`-Dhttps.protocols=TLSv1.2`来override编译时Java采用的链接协议。（还可以将TLSv1，TLSv1.1以逗号分隔传入）



<br />

#### **2.2.2 `Cannot resolve dependencies`和`handshake_failure系列`**

&ensp;&ensp;&ensp;&ensp;前者一般是因为在**spark的pom.xml**中指定的仓库地址找不到hadoop2.6.0-cdh5.13.0的依赖包（因为根本没有啊），也可能是网络连接不稳定的问题（这是最崩溃的，我当时用宿舍校园网编译时网络就不是很稳定，这个错误一时有一时没有）；

&ensp;&ensp;&ensp;&ensp;解决办法是在spark源码目录的pom.xml文件中添加cloudera仓库：

```xml
<repository>
    <id>cloudera</id>
    <name>cloudera Repository</name>
   <url>https://repository.cloudera.com/artifactory/cloudera-repos</url>
</repository>
```

> 注意该仓库所属`repositories`应与central仓库的相同（因为pom.xml中有两个地方可以添加仓库），否则依旧会找不到



&ensp;&ensp;&ensp;&ensp;后者一般是国内连中央仓库（在国外）不稳定导致的网络问题，导致无法从maven的中央仓库获取到相应的数据；

&ensp;&ensp;&ensp;&ensp;解决办法是修改maven的pom.xml（$MAVEN_HOME/conf/settings.xml中）中的镜像（注意mirrorOf一定要是central，具体各参数意义可以参见上面的注释，这是最简单直接的方法，不能害怕读这些英文），本次编译采用阿里云的镜像：

```xml
<mirror>
    <id>nexus-aliyun</id>
    <mirrorOf>central</mirrorOf>
    <name>Nexus Aliyun</name>
    <url>http://maven.aliyun.com/nexus/content/groups/public</url>
</mirror>
```

> - 如果之前就搞过Java开发，这就是常规操作了orz
>
> - 在Java7中，“handshake_failure”还可能是因为Java在https上通讯协议安全机制的问题。这是我排除上述两种可能性猜测并解决的。
>
>   此时需要将$JAVA_HOME/jre/lib/security中的两个jar包替换为官网的补充包，网址参见：
>
>   http://www.oracle.com/technetwork/java/javase/downloads/jce-7-download-432124.html
>
>   问题具体的产生机制待日后补充（技术迭代后，Java7问题真多...）



<br />

#### **2.2.3 maven编译失败缓存问题**

&ensp;&ensp;&ensp;&ensp;maven编译过程中的包下载failure会形成`*.lastUpdated`文件缓存到本地仓库，直到过了中心仓库的更新区间才会再次尝试抓取，除非被强制更新。当时因为网络问题时常断网，会导致fetch失败，所以：

> In Maven 3 if you just had a failed download and have fixed it (e.g. by uploading the jar to a repository) it will cache the failure. To force a refresh add -U to the command line.
>
> by somebody@StackOverflow



<br />

#### **2.2.4 解压文件时候`gzip：stdin：unexpected end of file tar：归档文件中异常的 EOF`问题**

&ensp;&ensp;&ensp;&ensp;可以看出，是输入到解压命令的stdin流（即压缩文件）以异常的方式结束。

&ensp;&ensp;&ensp;&ensp;这种问题一般产生于最开始编译时下载的zinc和scala压缩文件不完全（这两个文件真的难下，网速不好甚至网络运营商选得不好时编译就会很崩溃）。

&ensp;&ensp;&ensp;&ensp;对该问题我的解决方法是先手动清除./build/下的zinc和scala相关文件夹和压缩包，自己wget两个压缩包并解压（具体下载链接在编译时stdout出来的log中有，复制即可）；如果还是很慢，可以尝试换另一个网络下载——我当时是用了家里wifi不行，开手机热点才下载下来的，而且使用手机热点的下载速度感人。

> 据官网介绍，zinc是用于加速编译的，而scala则是编译的必须包。
>
> 这两个bug**不会中断编译**的进行，但会给后面的编译中带来ComplieFailed



<br />

#### **2.2.5 `进程被杀死，${MVN_BIN}" -DzincPort=${ZINC_PORT} "$@`**

&ensp;&ensp;&ensp;&ensp;这个问题让我曾想放弃源码编译，情况都是编译长时间卡住，最后显示进程被杀死。但我还是靠最后**冷静**的`Debug Loop`解决了这个问题！

&ensp;&ensp;&ensp;&ensp;首先，zinc是个啥？

&ensp;&ensp;&ensp;&ensp;通过[官网zinc相关链接](https://spark.apache.org/developer-tools.html#reducing-build-times)，知道：

> The zinc process can subsequently be shut down at any time by running build/zinc-<version>/bin/zinc -shutdown and will automatically restart whenever build/mvn is called.

&ensp;&ensp;&ensp;&ensp;其次，从报错信息中可看出涉及杀死进程和端口。所以产生原因可能有：

- 防火墙策略问题：3030端口无法访问。事实上，只需要127.0.0.1可以在3030提供服务即可。

- 端口占用问题：3030端口被某个进程占用。

- 内存空间不足：编译需要启动JVM。事实上这应该是问题出现的本质，如果内存空间足够，应该不会有进程被杀死的情况，也不用像下面这样杀死进程了。

​	

&ensp;&ensp;&ensp;&ensp;为了检验我的猜测，我在启动编译前确认关闭防火墙、3030端口无占用。

&ensp;&ensp;&ensp;&ensp;此时进行编译，编译仍然卡住。此时我开启另一个ssh，将zinc进程杀死，使得编译暂时继续。

&ensp;&ensp;&ensp;&ensp;而后发现，依旧出现进程被杀死情况`进程被杀死，"${BUILD_COMMAND[@]}"`。便猜测是内存不足导致进程被杀死了。给虚拟机分配4G+内存后，此处不再出现问题。

&ensp;&ensp;&ensp;&ensp;为进一步检验，在编译的同时，我通过free -h监控编译过程中的内存占用，

&ensp;&ensp;&ensp;&ensp;于是按上述操作再跑一次，并通过free -h查看编译过程中内存使用情况，发现虚拟机内存占用一度上升到2.9G，而我之前的内存+swap分区都不足2G。

> - 手动杀死zinc的时机：编译卡顿一般产生于Accumulator有关的warning出现的地方（具体为什么在此处使用zinc待补充）
> - 虚拟机内存和磁盘分配过小不够玩Spark生态：比如再后来，硬盘容量过小导致HBase启动时RIT甚至无法生成副本而启不起来



<br />

#### **2.2.6 spark project core阶段的`testCompile：net.alchim31.maven：CompilerFailure`**

&ensp;&ensp;&ensp;&ensp;问题产生机理未明，估计是使用Java7导致的和Spark的版本冲突问题，可以尝试：

- 在pom.xml里面的net.alchim31.maven下面都加上个version为3.2.2

- 把./dev/make-distribution.sh 里面的MVN改成自己的maven目录



<br />

<br />

## **3. 手动释放内存**

&ensp;&ensp;&ensp;&ensp;编译涉及了大量的文件读写存取，这些文件的缓存可能把物理内存资源占完了（可通过`free -h`查看，其中`free`+`buffer/cache`就是物理内存容量）。导致应用被启动后会去申请swap分区内存。

&ensp;&ensp;&ensp;&ensp;但内存cache中这部分文件在应用中完全用不到，所以手动释放内存再启动应用能稍微提高性能。

```bash
$ sync
# 用于flush写缓冲区
$ sudo echo 3 | sudo tee /proc/sys/vm/drop_caches
# 或$ sudo sh -c "/usr/bin/echo 3 > /proc/sys/vm/drop_caches"
# 注意echo与>是两个命令，sudu echo 3 > /proc/sys/vm/drop_caches中>并未获得sudo权限
```

>- Documentation of "/proc/sys/vm/drop_caches" to see how "available" column in `free -h` is calculated：https://www.kernel.org/doc/Documentation/sysctl/vm.txt
>
>- How the columns in `free -h` are calculated：https://man7.org/linux/man-pages/man1/free.1.html
>  - 前期一些不提供"available"栏的Linux版本，"used"字段包含了"buffer"+"cache"。
>  - 内存使用情况的常见误区是只看"free"和"used"。（释放内存即让"available"转化为"free"）
>- How Linux define "available" (roughly)：https://www.linuxatemyram.com/（感觉里头的total有些问题？）
>- Only the action of writing matters：https://unix.stackexchange.com/questions/17936/setting-proc-sys-vm-drop-caches-to-clear-cache



<br />

<br />

## **4. 小结**

### **4.1 基本防雷措施**

- 镜像：给maven一个国内maven仓库镜像，给spark一个cloudera仓库镜像（加镜像应该是java项目开发常规操作吧orz）
- 版本：强烈建议按照官网提及的版本套装配置环境（当时图方便用了内置的Java7进行编译，但以学习为目的，碰到问题又很想用Java7硬杠）

- 虚拟机：最好配内存4G+硬盘15G（后期如果玩HBase需要建立在HDFS基础上，`/`分区挂载的硬盘最好能有10G）

- 网络：最好有能“畅通无阻”的网络以官网为参考大纲，以报错信息为debug主要依据，以思考和搜索引擎为debug主要辅助



<br />

### **4.2 基本Debug策略**

- 冷静：Debug注意带`脑子`（冲动是魔鬼
- Debug Loop：`看日志` + `思考` + `尝试` + `看日志`  +  `搜索` + `思考` + `尝试` + `看日志`  +  `···`。注意用`脑子`，别人描述的问题不一定是我碰到的问题。
- 搜索引擎：`Segmentfault`、`StackOverflow`、`Startpage/Google`、`掘金`等（>>`CSDN`和`Baidu`）







-------

<div align="center" >By Divsigma@github.com</div>

