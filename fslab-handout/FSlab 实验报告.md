## 总体设计

### Bitmap 部分

bitmap分成两种

- inode bitmap

- datablock bitmap

#### inode bitmap

**前者有$2^{15}$个，大小为4KB,是1个datablock**

#### datablock bitmap

而共有256MB空间，为$2^{28}$ 大小

而每一个datablock都是$2^{12}$ 大小

**所以一空需要$2^{16}$ 个标记为**

**为64K bit，8KB 占用2个datablock**

### 空间安排

从第一块是inode

从第二三块，是datablock node 

### iNode 部分

```c
typedef struct 

{
    mode_t  st_mode; //文件对应的模式    4
    nlink_t st_nlink;//文件的链接数        8
    uid_t   st_uid;  //文件所有者        4
    gid_t   st_gid;  //文件所有者的组    4
    off_t   st_size; //文件字节数        8
    time_t  atime;//被访问的时间        8
    time_t  mtime;//被修改的时间        8
    time_t  ctime;//状态改变时间        8
    int direct_pointer[DerictPointer];
    int indirect_pointer[InDerictPointer];
    int id;
    char make_full[12];

}iNode;
```

*特别鸣谢徐晨同学为我提供了对其到128字节的思路，使得后续操作简单很多*

由于总体要支持32768个文件，所以要有$2^{15}$ 个iNode

经过设计和补全，每一个inode的大小都有128 = $2^7$ 字节大小。所以对于inode部分共有$2^{22}$ 也就是4MB的大小

**我们不妨规定，/这个最初的目录在所有文件中排第一个**

inode是从第25个块开始的

每一个块中有4KB，也就是会有32个inode

**所以inode部分为从第4块到第1027块**

### Data Block 部分

从1027到最终

### 目录文件的存储

设计数据结构

```c
typedef struct  DictioryContent {
    char file_name[24];
    char file_extent[4];
    int inode_number;
};
```

如此一来得到每一个文件占用32个字节，这样一个块4KB中有**128**个文件可以存

### 文件的储存结构

从上到下依次进行，有着大小标记。

## 具体实现

部分函数的流程图

### mv

![5cf3ea6f48c8190709](https://i.loli.net/2019/06/02/5cf3ea6f48c8190709.png)

### rename

![5cf3ea831e48956482](https://i.loli.net/2019/06/02/5cf3ea831e48956482.png)

### rm

![5cf3eaaa2047772718](https://i.loli.net/2019/06/02/5cf3eaaa2047772718.png)

### get attr

![5cf3eb072c5a695193](https://i.loli.net/2019/06/02/5cf3eb072c5a695193.png)

## 调试过程

### touch指令

touch (有bug)（已经此修复）

建立第二个目录之后，第二个目录中进行touch会出现错误

可能是mkdir的错误

或者是mknod的错误

**可能是目录挂载的时候出错了**

**或者是建立文件写入的时候没有*申请*声明初始的datablock**

#### 几个尝试

- 先创建了很多文件，再进入第一个文件夹，这个文件夹的中进行touch会出错

- 先创建了很多文件，再进入第一个文件夹，这个文件夹的中进行mkdir会出错

- 创建了两个文件夹，进入第二个之后创建文件夹和文件都会出错

- 创建第一个文件夹，再这一层中可以随意操作，但是再进一层之后则会出错

#### 发现问题

可以申请空间，也可以写进去

但是之后找不到

之后读进去也可以找到，可能是

understanding file的问题？？

**最终定位到了understading the path**

#### 最终解决方案

原本准备要对于understanding path 进行重构，

将understanding path阅读完成之后，发现是在解析路径的时候出现了一个参数的错误

这个参数的错误导致所有的搜寻都是在根目录上完成的

### rm指令

调试 rm(已经完成)

#### 发现问题

rm在根目录下可以进行正常的删除

**现在已经确定可以正常的读取比较了**

**错误在于将最后一个移动到被删除的位置的那一步操作，也就是line 1231**

#### 已经测试

在跟目录下进行删除空文件没有问题

![5cf5d98d3845190152](https://i.loli.net/2019/06/04/5cf5d98d3845190152.png)![5cf5d98d3845190152](https://i.loli.net/2019/06/04/5cf5d98d3845190152.png)

但是会出现删除错误的情况???????????????????????????????????

程序迷惑行为

![5cf5da6b3d18b86884](https://i.loli.net/2019/06/04/5cf5da6b3d18b86884.png)![5cf5da6b3d18b86884](https://i.loli.net/2019/06/04/5cf5da6b3d18b86884.png)

这部分功能已经完成

在非根目录下删除文件

在非根目录下删除非空的文件

在跟目录下删除非空的文件

### mv指令

![5cf5ed4568af038714](https://i.loli.net/2019/06/04/5cf5ed4568af038714.png)

进行move操作的时候没有把原来的文件写到新的路径中，反而是把目标目录reloadl了一次，十分迷惑

上述问题已经解决

![5cf5f32154e3239563](https://i.loli.net/2019/06/04/5cf5f32154e3239563.png)

**目前的问题式在更深层进行mv操作无法在新路径中挂载**

一层二层好像都可以，但是三层好像不行

~~调不动了，放弃治疗~~

## 心得体会

工程量大，要做好版本管理

即使最后自己的文件系统也不足以应对各种特殊情况，还有许多需要哦考虑

~~对自己的工程能力有了自信（不）~~

~~最后块干不动的时候对于以后的职业选择产生了怀疑（指正）~~

函数注释在大项目中十分重要

学会封装可以减少写作和调试的成本

~~以后的大工程可能双人写会更有效率一点~~
