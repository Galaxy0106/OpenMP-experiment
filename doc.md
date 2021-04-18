# 数据结构
```C
每个线程都有一个本地工作集，即n/p，其中n为总的迭代次数，p为线程数
对于每一个线程的工作集，又分为许多chunk，每个chunk代表一次执行的迭代，每个chunk是原子性的，
即在执行该chunk时不能够被打断

采用的共享数据结构为一个工作队列的数组
每一个线程都有一个工作队列
而每一个工作队列由许多个chunk组成，这里的chunk可以是属于本地工作集的chunk，
也可以是该线程执行完本地工作集之后从其他线程那里得到的chunk

每个chunk包括循环迭代的上界和下界，workload的数量，即该chunk的(hi - lo)
以及用于访问下一个chunk的指针
typedef struct chunk{
  int lo;
  int hi;
  int workload;
  struct chunk *next;
} ck;

每个工作队列包括队首和队尾，该工作队列总的chunk的数量以及总的workload
typedef struct work_queue{
    ck *front;//the front of the queue
    ck *rear;//the rear of the queue
    int total_chunk_size;
    int total_workload;
} wq;

实际上通过这种数据结构实现的就是一个以chunk作为每个队列元素的一个双端队列
```

# 代码描述
```C
队列的定义就不再细节描述
包括队列的初始化
    队列的销毁
    队列的入队
    队列的出队
    队列的判空
    队列的打印(方便调试)

主要描述并行控制过程的流程

定义用于共享的公共变量，workqueues和locks（workqueues是所有线程工作队列的数组，locks是所有线程用于同步的锁的数组）

设置共享数据，线程数量，开始并行执行
#pragma omp parallel default(none) shared(loopid, work_queues, locks) num_threads(thread_nums)
{

下面的块只有一个线程执行，因为做的是初始化操作
#pragma omp single
    {
        初始化工作队列
        初始化锁
    }
从此处又开始多线程并行执行
    //根据线程id设置本地数据集
    计算local_set_start
    计算local_set_end
    循环：local set没有被分配完{
        给chunk赋值
        将chunk加入当前线程的工作队列
    }
会等待所有的线程运行到此处才会继续往后执行
#pragma omp barrier
    //当前线程执行本地工作集
    循环：队列不为空{
        //避免该chunk同时被其他已经执行完自己本地工作集的线程获取，造成资源争用
        加锁
        出队列，得到要执行的chunk
        释放锁
        对该chunk执行loop循环
    }
    //当前线程执行完本地工作集之后去获取其他线程工作队列中的chunk去执行，即work stealing过程
    循环：{
        得到所有工作队列中剩余workload最多的线程id（根据total_workload）
        if：
            发现所有的工作队列都没有剩余的workload，工作完成，退出循环
        else：
        对剩余workload最多的线程的工作队列加锁
            if：
                该线程的workload正在被其他线程执行
                释放锁，不去争用
            else：
                对该工作队列做出队列操作
                释放锁
                对出队列得到的chunk执行loop循环
    }
会等待所有的线程运行到此处才会继续往后执行
#pragma omp barrier
    释放资源，防止内存泄露
}

对于多线程的同步主要体现在每一次出队列操作，出队列的过程相当于与获取chunk执行的过程，
该过程是原子性的，不能被其他线程打断，否则会产生不可预计的结果
```

# 代码执行
    编译：
        icc -O3 -qopenmp loops2.c -o loops2
    执行：
        ./loops2
    改变线程数量：
        line 7: #define thread_nums 12
        修改代码thread_nums的值，默认写的是12

    
