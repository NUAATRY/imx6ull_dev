# 此文件是关于IMX6ull的驱动开发文件
实验1是有关字符设备的驱动

实验2是有关led的驱动（直接操作寄存器）

实验3是有关led的驱动（通过设备树读取寄存器信息）

实验4是有关led的驱动（通过设备树和GPIO一起实现）

实验5是有关beep的驱动（通过设备树和GPIO一起实现）

实验6是原子操作相关的实验，通过原子操作可以实现第二个线程必须等第一个线程结束才能开始工作（实际上就是保护了一下切换任务的那个变量）

实验7是自旋锁相关的实验，通过自旋锁操作可以实现第二个线程必须等第一个线程结束才能开始工作（优点是占用内存资源小，缺点是会一直去访问第一个线程是否结束，适合用在短期任务上）

实验8是信号量和互斥量的实验，把两个都写在了一块，把通过信号量实现的部分都屏蔽了，保留了互斥量实现的代码，原理跟uCOS3中的信号量和互斥量一样，通过变量实现线程锁的作用，相比自旋锁有以下优点（1,信号量可以使等待资源线程进入休眠状态，因此适用于那些占用资源比较久的场合。2,信号量不能用于中断中，因为信号量会引起休眠，中断不能休眠。3，如果共享资源的持有时间比较短，那就不适合使用信号量了，因为频繁的休眠、切换 线程引起的开销要远大于信号量带来的那点优势。==与自旋锁是互补的==）

实验9是key按键扫描的实验，此实验主要是利用GPIO的输入实现高低电平的读取，在读取高低电电平时采用了原子操作的方式，此方法没有进行消抖处理，且运行时会把CPU跑满，需要后期做一定处理才能使用。

实验10是定时器实验，在定时器中重点需要关注两个对象，一个是内核定时器的使用，需要用那些API函数，linux内核定时器跟HAL库的uwTick有点像，通过设置一个目标时间点，然后比较是否到达目标时间点实现定时;一个是ioctl的使用，使用此函数会非常方便，相比read函数的好处是：read函数的使用只能在调用驱动的时候传递一次参数，但是ioctl可以传递很多次参数。

实验11是按键中断实验，结合上一个实验的案件消抖完成，先初始化io口，然后配置中断获取中断号，并申请中断，当中断发生时进入中断服务函数，在中断服务函数中开启定时器，定时10ms后进入定时服务函数，读取按键是否按下的信息，最终在用户态通过read读取内核态的数据，内核态通过read函数里面的copy_to_user到用户态read函数的buf里面，用户态read函数的返回值reg即为内核态read函数的返回值。

实验12是阻塞实验blockio，之前的实验11按键代码没有做按下后和松开后的处理，在做实验12时对此进行了优化之后再进行的实验;在本实验中我们在中断中添加了一个唤醒程序，在读取函数的地方设置一个等待队列，设置完成后会执行一个切换任务的函数切换到其他任务上，只有发生信号或者在中断里面手动唤醒的情况下才会唤醒此任务继续执行，继续执行后需要把进程从任务队列中删除，等下次执行到对应的位置再创建即可。

实验13是非阻塞实验noblockio，在本实验中我们用户态中轮询访问是否有可读数据，当访问时没有可读数据是会返回超时现象，当访问到有可读数据时即可通过read函数读取到对应的值，当应用程序调用 select 或 poll 函数来对驱动程序进行非阻塞访问的时候，驱动程序 file_operations 操作集中的 poll 函数就会执行。

实验14是异步通信实验，在本实验中我们采用类似中断的方式实现按键的读取，当按键有反映时会在硬件中断中触发kill_fasync(&dev->async_queue,SIGIO,POLL_IN)函数，除此之外还需要在file_operations中配置好.fasync	= imx6uirq_fasync和.release	= imx6uirq_release,应用程序配置好signal(SIGIO,sigio_signal_func)后，while循环可以去做其他的事情，当软件中断发生时，就会自动跳转到对应的回调函数static void sigio_signal_func(int signum)。应用层fcntl的配置和驱动层.fasync和.release的配置具体原因暂时不清楚。

实验15是基于设备树的platform实验，在本实验中我们linux中的platform架构进行驱动的开发，实际上就是给原来的驱动外加了一个platform的皮，其他的节点获取，获取设备号，注册设备，创建设备，创建类都是一样的没有改变，可以详细了解一下platform如何实现device和driver匹配（有四种方式）init之后就会执行probe函数，exit就会执行remove函数。

实验15是基于设备树的platform实验，在本实验中我们linux中的platform架构进行驱动的开发，实际上就是给原来的驱动外加了一个platform的皮，其他的节点获取，获取设备号，注册设备，创建设备，创建类都是一样的没有改变，可以详细了解一下platform如何实现device和driver匹配（有四种方式）init之后就会执行probe函数，exit就会执行remove函数。

实验16是misc杂项实验，在上一个实验platform的基础上，通过misc对dev进一步封装，可以实现一个函数完成获取设备号，注册设备，创建设备，创建类四个函数的功能。

实验17是I2C驱动设备实验，该实验利用两个框架，一个是总线adapter和IIC控制器匹配利用paltform框架实现，一个是IIC驱动和设备匹配，利用IIC特定的类似于platform的框架实现，且因为IIC控制器和设备的设备树定义有相关联系，所以两个框架可以自动绑定，相当有总线是父节点，设备是字节点。