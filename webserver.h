/*
 * @Author: zx
 * @Date: 2021-07-10 21:08:03
 * @LastEditTime: 2021-07-12 16:26:11
 * @LastEditors: zx
 * @Description: 
 * @FilePath: /zx/桌面/webserver/webserver.h
 * 可以输入预定的版权声明、个性签名、空行等
 */
#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <cassert>
#include <sys/epoll.h>


#include "http_con.h"

const int MAX_FD = 65536;           //最大文件描述符
const int MAX_EVENT_NUMBER = 10000; //最大事件数
const int TIMESLOT = 5;             //最小超时单位

class WebServer
{
public:
    WebServer();
    ~WebServer();

    void init(int port,int SQLVerify,string user, string passWord, string databaseName, int log_write, 
                     int opt_linger, int trigmode, int sql_num, int thread_num, int close_log, int actor_model);

    void thread_pool();//启动线程池
    void sql_pool();//启动数据库
    void log_write();//启动写日志
    void trig_mode();//触发模式
    void eventListen();//启动事件监听
    void eventLoop();//处理读写+信号
    void timer(int connfd, struct sockaddr_in client_address);//将连接加入时间链
    void adjust_timer(util_timer *timer);//调整fd时间
    void deal_timer(util_timer *timer, int sockfd);//将长时间无数据fd下epoll树
    bool dealclientdata();//接受连接fd，并将fd加入时间链
    bool dealwithsignal(bool& timeout, bool& stop_server);//
    void dealwithread(int sockfd);
    void dealwithwrite(int sockfd);

public:
    //基础
    int m_port;
    char *m_root;
    int m_log_write;
    int m_close_log;
    int m_actormodel;

    int m_pipefd[2];
    int m_epollfd;
    http_con *users;

    //数据库相关
    connection_pool *m_connPool;
    string m_user;         //登陆数据库用户名
    string m_passWord;     //登陆数据库密码
    string m_databaseName; //使用数据库名
    int m_sql_num;
    int m_SQLVerify;

    //线程池相关
    ThreadPool<http_con> *m_pool;
    int m_thread_num;

    //epoll_event相关
    epoll_event events[MAX_EVENT_NUMBER];

    int m_listenfd;
    int m_OPT_LINGER;
    int m_TRIGMode;


    //定时器相关
    client_data *users_timer;//每个连接都有一个的定时器client_data
    Utils utils;//处理不活动链接大类
};
#endif
