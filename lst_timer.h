/*
 * @Author: zx
 * @Date: 2021-07-08 14:42:49
 * @LastEditTime: 2021-07-08 15:22:46
 * @LastEditors: zx
 * @Description: 定时器声明
 * @FilePath: /zx/桌面/webserver/lst_timer.h
 * 可以输入预定的版权声明、个性签名、空行等
 */
#ifndef _LST_TIMER_H
#define _LST_TIMER_H

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <time.h>
#include <functional>
#include <list>
#include"http_con.h"

using namespace std;

class util_timer; //前向声明

//客户端数据
struct client_data {
    sockaddr_in address; //socket地址
    int sockfd; //socket文件描述符
    util_timer* timer; //定时器
};

//定时器类
class util_timer {
public:
    time_t expire; //任务超时时间(绝对时间)
    function<void(client_data*)> callBackFunc; //回调函数
    client_data* userData; //用户数据
};

//定时器升序链表
class Timer_list {
public:
    explicit Timer_list();
    ~Timer_list();
public:
    void add_timer(util_timer* timer); //添加定时器
    void del_timer(util_timer* timer); //删除定时器
    void adjust_timer(util_timer* timer); //调整定时器
    void tick(); //处理链表上到期的任务
private:
    list<util_timer*> m_timer_list; //定时器链表
};

//将关闭活动链接操作封装成Utils类
class Utils
{
public:
    Utils() {}
    ~Utils() {}

    void init(int timeslot);

    //对文件描述符设置非阻塞
    int setnonblocking(int fd);

    //将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
    void addfd(int epollfd, int fd, bool one_shot, int TRIGMode);

    //信号处理函数
    static void sig_handler(int sig);

    //设置信号函数
    void addsig(int sig, void(handler)(int), bool restart = true);

    //定时处理任务，重新定时以不断触发SIGALRM信号
    void timer_handler();

    void show_error(int connfd, const char *info);

public:
    static int *u_pipefd;
    Timer_list m_Timer_list;
    static int u_epollfd;
    int m_TIMESLOT;
};

void callBackFunc(client_data *user_data);
#endif
