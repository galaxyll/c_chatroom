### C语言聊天室
#### 结构
Client/Server
#### 数据结构
链表
#### 数据库
MySQL
#### 流程
##### Server端
1. 初始化链表
- all_send_pack 所有待发送的包 初始化为NULL
- all_sign_user 所有注册用户 从MySQL中初始化
- all_create_group 所有群组 从MySQL中初始化
- all_send_file 所有待发送文件 初始化为NULL
- recv_pack_t 所有待接收包 初始化为NULL

2. 建立对数据库MySQL的连接
3. 创建后台发包线程，持续检测待发送包链表
4. 初始化epoll
5. 获取监听套接字监听指定端口
6. 初始化服务端套接字设置使用TCP协议
7. while循环，使用epoll多路复用处理输入事件，调用处理函数处理各种类型请求
