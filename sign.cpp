#include <mysql/mysql.h>
#include <iostream>
#include <string>
#include <string.h>
#include <cstdio>
#include <map>
#include <fstream>
#include <sstream>
#include "locker.h"
using namespace std;

int main(int argc, char *argv[])
{
    map<string, string> users;

    Locker lock;
    string SQLVerify(argv[3]);
    
    if ("2" == SQLVerify)
    {
        string sql_user(argv[4]);
        string sql_passwd(argv[5]);
        string sql_name(argv[6]);

        MYSQL *con = NULL;
        con = mysql_init(con);

        if (con == NULL)
        {
            cout << "Error:" << mysql_error(con);
            exit(1);
        }
        con = mysql_real_connect(con, "localhost", sql_user.c_str(), sql_passwd.c_str(), sql_name.c_str(), 3306, NULL, 0);

        if (con == NULL)
        {
            cout << "Error: " << mysql_error(con);
            exit(1);
        }

        //在user表中检索username，passwd数据，浏览器端输入
        if (mysql_query(con, "SELECT username,password FROM user"))
        {
            printf("INSERT error:%s\n", mysql_error(con));
            return -1;
        }
        //从表中检索完整的结果集
        MYSQL_RES *result = mysql_store_result(con);
        //返回结果集中的列数
        int num_fields = mysql_num_fields(result);
        //返回所有字段结构的数组
        MYSQL_FIELD *fields = mysql_fetch_fields(result);

        //从结果集中获取下一行，将对应的用户名和密码，存入map中
        while (MYSQL_ROW row = mysql_fetch_row(result))
        {
            string temp1(row[0]);
            string temp2(row[1]);
            users[temp1] = temp2;
        }

        string name(argv[1]);
        const char *namep = name.c_str();
        string passwd(argv[2]);
        const char *passwdp = passwd.c_str();
        char flag = *argv[0];

        //如果是注册，先检测数据库中是否有重名的
        //没有重名的，进行增加数据
        char *sql_insert = (char *)malloc(sizeof(char) * 200);
        strcpy(sql_insert, "INSERT INTO user(username, password) VALUES(");
        strcat(sql_insert, "'");
        strcat(sql_insert, namep);
        strcat(sql_insert, "', '");
        strcat(sql_insert, passwdp);
        strcat(sql_insert, "')");

        if (flag == '3')
        {
            if (users.find(name) == users.end())
            {
                lock.lock();
                int res = mysql_query(con, sql_insert);
                lock.unlock();

                if (!res)
                    printf("1\n");
                else
                    printf("0\n");
            }
            else
                printf("0\n");
        }
        //如果是登录，直接判断
        //若浏览器端输入的用户名和密码在表中可以查找到，返回1，否则返回0
        else if (flag == '2')
        {
            if (users.find(name) != users.end() && users[name] == passwd)
                printf("1\n");
            else
                printf("0\n");
        }
        else
            printf("0\n");
        //释放结果集使用的内存
        mysql_free_result(result);
    }
    else
    {
        ifstream out(argv[2]);
        string linestr;
        while (getline(out, linestr))
        {
            string str;
            stringstream id_passwd(linestr);

            getline(id_passwd, str, ' ');
            string temp1(str);

            getline(id_passwd, str, ' ');
            string temp2(str);
            users[temp1] = temp2;
        }
        out.close();

        //只完成登录
        string name(argv[0]);
        const char *namep = name.c_str();
        string passwd(argv[1]);
        const char *passwdp = passwd.c_str();

        if (users.find(name) != users.end() && users[name] == passwd)
            printf("1\n");
        else
            printf("0\n");
    }
}
