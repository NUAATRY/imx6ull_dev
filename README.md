# 此文件是关于IMX6ull的驱动开发文件
实验1是有关字符设备的驱动

实验2是有关led的驱动（直接操作寄存器）

实验3是有关led的驱动（通过设备树读取寄存器信息）

实验4是有关led的驱动（通过设备树和GPIO一起实现）

实验5是有关beep的驱动（通过设备树和GPIO一起实现）

实验6是原子操作相关的实验，通过原子操作可以实现第二个线程必须等第一个线程结束才能开始工作（实际上就是保护了一下切换任务的那个变量）

实验7是自旋锁相关的实验，通过自旋锁操作可以实现第二个线程必须等第一个线程结束才能开始工作（优点是占用内存资源小，缺点是会一直去访问第一个线程是否结束，适合用在短期任务上）
