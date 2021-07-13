/*
 * @Author: zx
 * @Date: 2021-07-11 14:49:58
 * @LastEditTime: 2021-07-12 23:30:49
 * @LastEditors: zx
 * @Description: 
 * @FilePath: /zx/桌面/webserver/main.cpp
 * 可以输入预定的版权声明、个性签名、空行等
 */
#include "config.h"
int main(int argc, char* argv[])
{
    //需要修改的数据库信息,登录名,密码,库名
    string user = "root";
    string passwd = "123456";
    string databasename = "zx";

    //命令行解析
    Config config;
    config.parse_arg(argc, argv);

    WebServer server;

    //初始化
    server.init(config.PORT,config.SQLVerify, user, passwd, databasename, config.LOGWrite, 
                config.OPT_LINGER, config.TRIGMode,  config.sql_num,  config.thread_num, 
                config.close_log, config.actor_model);
    

    //日志
    server.log_write();

    //数据库
    server.sql_pool();

    //线程池
    server.thread_pool();

    //监听
    server.eventListen();

    //运行
    server.eventLoop();
   
    return 0;
}
