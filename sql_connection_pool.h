/*
 * @Author: zx
 * @Date: 2021-07-10 09:34:02
 * @LastEditTime: 2021-07-11 22:21:43
 * @LastEditors: zx
 * @Description: 
 * @FilePath: /zx/桌面/webserver/sql_connection_pool.h
 * 可以输入预定的版权声明、个性签名、空行等
 */
#ifndef _CONNECTION_POOL_
#define _CONNECTION_POOL_

#include <stdio.h>
#include <list>
#include <mysql/mysql.h>
#include <error.h>
#include <string.h>
#include <iostream>
#include <string>
#include "locker.h"
#include "log.h"

using namespace std;

class connection_pool
{
public:
	MYSQL *GetConnection();				 //获取数据库连接
	bool ReleaseConnection(MYSQL *conn); //释放连接
	int GetFreeConn();					 //获取连接
	void DestroyPool();					 //销毁所有连接

	//单例模式
	static connection_pool *GetInstance();

	void init(string url, string User, string PassWord, string DataBaseName, int Port, int MaxConn, int close_log); 

private:
	connection_pool();
	~connection_pool();

	int m_MaxConn;  //最大连接数
	int m_CurConn;  //当前已使用的连接数
	int m_FreeConn; //当前空闲的连接数
	Locker lock;
	list<MYSQL *> connList; //连接池
	Sem reserve;

public:
	string m_url;			 //主机地址
	int m_Port;		 //数据库端口号
	string m_User;		 //登陆数据库用户名
	string m_PassWord;	 //登陆数据库密码
	string m_DatabaseName; //使用数据库名
	int m_close_log;	//日志开关
};

/* 
RAII：Resource Acquisition Is Initialization，
C++的RAII机制就是类似于C#或者Java的GC机制，垃圾回收。
合理的回收系统资源，避免程序员大量的写重复的delete代码来手动回收。
粗暴点将就是，靠类对象的作用域/生存期来管理资源。
是C++语言的一种管理资源、避免泄漏的惯用法。利用的就是C++构造的对象最终会被销毁的原则。RAII的做法是使用一个对象，在其构造时获取对应的资源，在对象生命期内控制对资源的访问，
使之始终保持有效，最后在对象析构的时候，释放构造时获取的资源。 



*/
class connectionRAII{

public:
	connectionRAII(MYSQL **con, connection_pool *connPool);
	~connectionRAII();
	
private:
	MYSQL *conRAII;
	connection_pool *poolRAII;
};

#endif
