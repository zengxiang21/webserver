#ifndef _HTTP_CON_H
#define _HTTP_CON_H

#include <iostream>
#include <string>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/epoll.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <iostream>
#include <signal.h> 
#include <assert.h>
#include <errno.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <mysql/mysql.h>
#include <map>
#include <fstream>
#include "threadpool.h"
#include "lst_timer.h"
#include "sql_connection_pool.h"
using namespace std;
class http_con
{
public:
    static const int FILENAME_LEN=200;//请求资源名最大长度
    static const int READ_BUFFER_SIZE=2048;//读缓存大小
    static const int WRITE_BUFFER_SIZE=2048;//写缓存大小
    enum METHOD{GET=0,POST,HEAD,PUT,DELETE,TRACE,OPTIONS,CONNECT,PATH};
    //主状态机的状态
    enum CHECK_STATE{CHECK_STATE_REQUESTLINE=0,CHECK_STATE_HEADER,CHECK_STATE_CONTENT};
    //报文解析的结果
    enum HTTP_CODE{NO_REQUEST,GET_REQUEST,BAD_REQUEST,NO_RESOURCE,FORBIDDEN_REQUEST,FILE_REQUEST,INTERNAL_ERROR,CLOSED_CONNECTION};
    //从状态机的状态
    enum LINE_STATUS{LINE_OK=0,LINE_BAD,LINE_OPEN};

public:
    http_con();
    ~http_con();

public:
    /* 初始化新接收的连接 */
    void init(int sockfd,sockaddr_in addr, char *root, int TRIGMode,int close_log, string user, string passwd, string sqlname);
    //设置端口复用
    void setIOm(int sockfd);
    void close_con(bool read_close=true);
    bool read();
    bool write();
    void process();

private:
    void init();
    HTTP_CODE process_read();//解析HTTP请求
    bool process_write(HTTP_CODE ret);//填充HTTP应答
    /*这一族函数被process_read调用解析HTTP请求 */
    HTTP_CODE parse_request_line(char* text);//解析HTTP请求行
    HTTP_CODE parse_header(char* text);//解析HTTP请求头
    HTTP_CODE parse_content(char* text);//解析HTTP请求内容
    LINE_STATUS parse_line();//从状态机，被主状态机调用，获取一行
    HTTP_CODE do_request();    
    char* getline() {return m_read_buf+m_start_line;}

    void unmap();
    /* 这一组函数用来process_write处理HTTP请求 */
    bool add_response(const char* format,...);
    bool add_status_line(int status,const char* title);
    bool add_header_line(int content_len);
    bool add_content(const char* content);
public:
    static int m_efd;//所有socket公用一个m_efd;
    static int m_user_count;//user数量
private:
    int m_sockfd;//当前http_con对象的sockfd
    sockaddr_in m_addr;//对方的addr

private:
    char m_read_buf[READ_BUFFER_SIZE];
    char m_write_buf[WRITE_BUFFER_SIZE];
    int m_read_idx;//从socket读取的索引
    int m_write_idx;

    struct iovec m_iv[2]; //writv使用的io向量
    int m_iv_count;//writv使用的io向量计数

    CHECK_STATE m_check_state;    //主状态机状态
    int m_start_line;  //process_read处理行时记录每一行起始的索引
    int m_check_idx; //process_read处理到哪个字符的索引
    int m_content_length;//请求体长度
    char m_real_file[FILENAME_LEN];//客户端请求的目标文件名称(doc_root+m_url)
    char* m_url;
    char* m_version;
    char* m_host;
    bool m_linger;
    METHOD m_method;
    struct stat m_file_stat;//请求文件信息 
    char* m_file_address;//mmap后返回的文件地址
    int m_bytes_have_send=0;
    int m_bytes_to_send=0;


    //数据库的相关变量
    map<string, string> m_users;
    int m_close_log;

    char sql_user[100];
    char sql_passwd[100];
    char sql_name[100];

    int cgi;        //是否启用的POST
    char *m_string; //存储请求头数据

public:
    MYSQL *mysql;
    int timer_flag;

    int improv;
    int m_state;  //读为0, 写为1
    void initmysql_result(connection_pool *connPool);
    void initresultFile(connection_pool *connPool);
    sockaddr_in *get_address()
    {
        return &m_addr;
    }

private:
    //触发形式
    int m_TRIGMode;
    char *doc_root;
    int m_SQLVerify;
};



#endif