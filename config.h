/*
 * @Author: zx
 * @Date: 2021-07-10 21:07:36
 * @LastEditTime: 2021-07-12 16:23:06
 * @LastEditors: zx
 * @Description: 
 * @FilePath: /zx/桌面/webserver/config.h
 * 可以输入预定的版权声明、个性签名、空行等
 */
#ifndef _CONFIG_H
#define _CONFIG_H

#include "webserver.h"

using namespace std;

class Config
{
public:
    Config();
    ~Config(){};

    void parse_arg(int argc, char*argv[]);

     //端口号
    int PORT;

    //数据库校验方式
    int SQLVerify;

    //日志写入方式
    int LOGWrite;

    //触发模式
    int TRIGMode;

    //优雅关闭链接
    int OPT_LINGER;

    //数据库连接池数量
    int sql_num;

    //线程池内的线程数量
    int thread_num;

    //是否关闭日志
    int close_log;

    //并发模型选择
    int actor_model;
};

#endif