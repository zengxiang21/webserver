#include"http_con.h"
#include<sys/epoll.h>
using namespace std;
//定义http响应的一些状态信息
const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file form this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form = "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving the request file.\n";

//初始化静态static变量
int http_con::m_efd=-1;
int http_con::m_user_count=0;


//定义锁

Locker m_lock;
map<string, string> users;

void setnonblocking(int fd)
{
    int oldflag=fcntl(fd,F_GETFL);
    int newflag=oldflag|O_NONBLOCK;
    fcntl(fd,F_SETFL,newflag);
}

void addfd(int efd,int fd,bool one_shot,int TRIGMode)
{
    epoll_event event;
    event.data.fd=fd;
    if(TRIGMode==1)
        event.events=EPOLLIN|EPOLLRDHUP|EPOLLET;
    else
        event.events=EPOLLIN|EPOLLRDHUP;        
    if(one_shot)
    {
        event.events|=EPOLLONESHOT;
    }
    epoll_ctl(efd,EPOLL_CTL_ADD,fd,&event);
    setnonblocking(fd);
}

void removefd(int efd,int fd)
{
    epoll_ctl(efd,EPOLL_CTL_DEL,fd,NULL);
    close(fd);
}

void modfd(int efd,int fd,int env,int TRIGMode)
{
    struct epoll_event event;
    event.data.fd=fd;
    if(TRIGMode==1)
        event.events=env|EPOLLIN|EPOLLRDHUP|EPOLLONESHOT;
    else 
        event.events=env|EPOLLIN|EPOLLRDHUP;        
    epoll_ctl(efd,EPOLL_CTL_MOD,fd,&event);
}

void http_con::init()
{
    m_check_state=CHECK_STATE_REQUESTLINE;
    m_check_idx=0;
    m_start_line=0;
    m_read_idx=0;
    m_write_idx=0;
    m_url=NULL;
    m_version=NULL;
    m_method=GET;
    m_host=NULL;
    m_content_length=0;
    m_linger=false;
    m_bytes_have_send=0;
    m_bytes_to_send=0;
    m_state = 0;
    timer_flag = 0;
    improv = 0;
    mysql=NULL;
    memset(m_read_buf,0,sizeof(m_read_buf));
    memset(m_write_buf,0,sizeof(m_write_buf));
    memset(m_real_file,0,sizeof(m_real_file));
    cgi = 0;//post默认关闭
}


http_con::http_con()
{

}

http_con::~http_con()
{

}

void http_con::setIOm(int sockfd)
{
    int reuse=1;
    setsockopt(sockfd,SOL_IP,SO_REUSEADDR,&reuse,sizeof(reuse));
}

void http_con::init(int sockfd,sockaddr_in addr, char *root, int TRIGMode,int close_log, string user, string passwd, string sqlname)
{
    m_addr=addr;
    m_sockfd=sockfd;
    //设置端口复用
    setIOm(m_sockfd);
    //将sockfd加入到epoll
    addfd(m_efd,sockfd,true,m_TRIGMode);
    m_user_count++;
    doc_root = root;
    m_TRIGMode = TRIGMode;
    m_close_log = close_log;

    strcpy(sql_user, user.c_str());
    strcpy(sql_passwd, passwd.c_str());
    strcpy(sql_name, sqlname.c_str());
    init();
}

void http_con::close_con(bool read_close)
{
    if(read_close&&(m_sockfd!=-1))
    {
        removefd(m_efd,m_sockfd);
        m_sockfd=-1;
        m_user_count--;
    }
}
//循环读取客户数据，直到无数据可读或对方关闭连接
//非阻塞ET工作模式下，需要一次性将数据读完
bool http_con::read()
{
   if (m_read_idx >= READ_BUFFER_SIZE)
    {
        return false;
    }
    int bytes_read = 0;
    while (true)
    {
        bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
        if (bytes_read == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            return false;
        }
        else if (bytes_read == 0)
        {
            return false;
        }
        m_read_idx += bytes_read;
    }
    return true;
}

bool http_con::write()
{
    int temp=0;
    //响应消息为空,基本不会发生
    if(m_bytes_to_send==0)
    {
        modfd(m_efd,m_sockfd,EPOLLIN,m_TRIGMode);
        init();
        return true;
    }
    while(1)
    {
        temp=writev(m_sockfd,m_iv,m_iv_count);
        if(temp<=-1)
        {
            //写缓存区满，write阻塞，但是m_sockfd设置为nonblocking
            if(errno==EAGAIN)
            {
                modfd(m_efd,m_sockfd,EPOLLOUT,m_TRIGMode);
                return true;
                //return true是为了让主线程不与该线程断开连接
            }
            //如果不是缓存的原因，说明出错
            unmap();
            return false;
        }

        m_bytes_have_send += temp;
        m_bytes_to_send -= temp; 
        if(m_bytes_have_send>=m_iv[0].iov_len)
        {
            //说明m_iv[0]中存的m_write_buf响应头部信息写完了
            m_iv[0].iov_len=0;
            m_iv[1].iov_base=m_file_address+(m_bytes_have_send-m_write_idx);
            m_iv[1].iov_len=m_bytes_to_send;
        }   
        else
        {
            m_iv[0].iov_base = m_write_buf + m_bytes_have_send;
            m_iv[0].iov_len = m_iv[0].iov_len - m_bytes_have_send;            
        } 
        if (m_bytes_to_send <= 0)
        {
            unmap();
            modfd(m_efd, m_sockfd, EPOLLIN,m_TRIGMode);
            if (m_linger)
            {
                init();
                return true;
            }
            else
            {
                return false;
            }
        }
    }
}

//主状态机收到的内容\r\n全部被替换为\0\0
http_con::HTTP_CODE http_con::process_read()
{
    //定义状态
    HTTP_CODE ret=NO_REQUEST;
    //从状态机初始状态
    LINE_STATUS line_status=LINE_OK;
    char* text=0;
    //GET||POST
    while( (m_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK)|| ( ( line_status=parse_line() )==LINE_OK) )
    {
        text=getline();
        m_start_line=m_check_idx;
        LOG_INFO("%s", text);

        switch(m_check_state)
        {
            case CHECK_STATE_REQUESTLINE:
            {
                ret=parse_request_line(text);
                if(ret==BAD_REQUEST)
                {
                    return BAD_REQUEST;
                }
                break;
            }
            case CHECK_STATE_HEADER:
            {
                ret=parse_header(text);
                if(ret==BAD_REQUEST)
                {
                    return BAD_REQUEST;
                }
                else if(ret==GET_REQUEST)
                {
                    return do_request();
                }
                break;                
            }
            case CHECK_STATE_CONTENT:
            {
                ret=parse_content(text);
                if(ret==GET_REQUEST)
                {
                    return do_request();
                }
                line_status=LINE_OPEN;
                break;                
            }
            default:
            {
                return INTERNAL_ERROR;
            }
        }

    }
    return NO_REQUEST;
}

//下面这些函数就是被process_read调用，用于解析HTTP请求
//解析HTTP请求
//GET / HTTP/1.1\0\0
//仅支持HTTP 1.1
http_con::HTTP_CODE http_con::parse_request_line(char* text)
{
    m_url=strpbrk(text," \t");
    if(!m_url)
    {
        return BAD_REQUEST;
    }
    *m_url++='\0';

    char* method=text;
    if(strcasecmp(method,"GET")==0)
    {
        m_method=GET;
    }
    else if (strcasecmp(method, "POST") == 0)
    {
        m_method = POST;
        cgi = 1;
    }
    else
    {
        return BAD_REQUEST;
    } 
    m_url+=strspn(m_url," \t");
    m_version=strpbrk(m_url," \t");
    if(!m_version)
    {
        return BAD_REQUEST;
    }
    *m_version++='\0';
    m_version+=strspn(m_version,"/t");
    if(strcasecmp(m_version,"HTTP/1.1")!=0)
    {
        return BAD_REQUEST;
    }
    if(strncasecmp(m_url,"http://",7)==0)
    {
        m_url+=7;
        m_url=strchr(m_url,'/');
    }
    if(!m_url||m_url[0]!='/')
    {
        return BAD_REQUEST;
    }
    //当url为/时，显示判断界面
    if (strlen(m_url) == 1)
    {
        strcat(m_url, "judge.html");        
    }
    m_check_state=CHECK_STATE_HEADER;
    return NO_REQUEST;
}
http_con::HTTP_CODE http_con::parse_header(char* text)
{
    if(text[0]=='\0')
    {
        if(m_content_length!=0)
        {
            m_check_state=CHECK_STATE_CONTENT;  
            return NO_REQUEST;
        }
        return GET_REQUEST;
    }
    else
    {
        if(strncasecmp(text,"Connection:",11)==0)
        {
            text+=11;
            text+=strspn(text," \t");
            if(strcasecmp(text,"keep-alive")==0)
            {
                m_linger=true;
            }
        }
        else if(strncasecmp(text,"Content-Length:",15)==0)
        {
            text+=15;
            text+=strspn(text," \t");
            m_content_length=atol(text);
        }
        else if(strncasecmp(text,"Host:",5)==0)
        {
            text+=5;
            text+=strspn(text," \t");
            m_host=text;
        }
        else{
            printf("I cannot handle this header:%s\n",text);
            LOG_INFO("oop!unknow header: %s", text);
        }
        return NO_REQUEST;
    }

}
http_con::HTTP_CODE http_con::parse_content(char* text)
{
    //仅仅判断请求体是否被读入
    if(m_read_idx>=m_content_length+m_check_idx)
    {
        text[m_content_length]='\0';
        //POST请求中最后为输入的用户名和密码
        m_string = text;
        return GET_REQUEST;
    }
    return NO_REQUEST; 
}

//从状态机
http_con::LINE_STATUS http_con::parse_line()
{
    //一个字符一个字符判断
    char temp;
    for(;m_check_idx<m_read_idx;++m_check_idx)
    {
         temp=m_read_buf[m_check_idx];
        if(temp=='\r')
        {
            //此时read没读完，将从状态机状态至为LINE_OPEN
            if(m_check_idx+1==m_read_idx)
            {
                return LINE_OPEN;
            }
            else if (m_read_buf[m_check_idx+1]=='\n')
            {
                m_read_buf[m_check_idx++]='\0';
                m_read_buf[m_check_idx++]='\0';
                return LINE_OK;
            }
            return LINE_BAD;        
        }
        else if(temp=='\n')
        {
            if((m_check_idx>1)&&(m_read_buf[m_check_idx-1]=='\r'))
            {
                m_read_buf[m_check_idx-1]='\0';
                m_read_buf[m_check_idx++]='\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }

    }
    return LINE_OPEN;
}


//写请求
bool http_con::process_write(HTTP_CODE ret)
{
    switch(ret)
    {
        case INTERNAL_ERROR:
        {
            add_status_line(500,error_500_title);
            add_header_line(strlen(error_500_form));
            if(!add_content(error_500_form))
            {
                return false;
            }
            break;
        }
        case BAD_REQUEST:
        {
            add_status_line(400,error_400_title);
            add_header_line(strlen(error_400_form));
            if(!add_content(error_400_form))
            {
                return false;
            }
            break;
        }
        case NO_RESOURCE:
        {
            add_status_line(404,error_404_title);
            add_header_line(strlen(error_404_form));
            if(!add_content(error_404_form))
            {
                return false;
            }
            break;
        }
        case FORBIDDEN_REQUEST:
        {
            add_status_line(403,error_403_title);
            add_header_line(strlen(error_403_form));
            if(!add_content(error_403_form))
            {
                return false;
            }
            break;
        }
        case FILE_REQUEST:
        {
            add_status_line(200,ok_200_title);
            if(m_file_stat.st_size!=0)
            {
                add_header_line(m_file_stat.st_size);
                m_iv[0].iov_base=m_write_buf;
                m_iv[0].iov_len=m_write_idx;
                m_iv[1].iov_base=m_file_address;
                m_iv[1].iov_len=m_file_stat.st_size;
                m_bytes_to_send = m_write_idx + m_file_stat.st_size;
                m_iv_count=2;
                return true;
            }
            else
            {
                //如果请求的资源大小为0，则返回空白html文件
                const char* ok_string="<html><body></body></html>";
                add_header_line(strlen(ok_string));
                if(!add_content(ok_string))
                {
                    return false;
                }

            }
            break;
        }
        default:
        {
            return false;
        }
    }
    //除FILE_REQUEST状态外，其余状态只申请一个iovec，指向响应报文缓冲区
    m_iv[0].iov_base=m_write_buf;
    m_iv[0].iov_len=m_write_idx;
    m_iv_count=1;
    return true;
}

void http_con::process()
{
    //解析HTTP报文头
    HTTP_CODE read_ret=process_read();
    if(read_ret==NO_REQUEST)
    {
        modfd(m_efd,m_sockfd,EPOLLIN,m_TRIGMode);
        return;
    }

    bool write_ret=process_write(read_ret);
    if(!write_ret)
    {
        close_con();
    }
    modfd(m_efd,m_sockfd,EPOLLOUT,m_TRIGMode);
}

http_con::HTTP_CODE http_con::do_request()
{
    //拿到请求地址doc
    strcpy(m_real_file,doc_root);
    int len=strlen(doc_root);
//////////
    const char *p = strrchr(m_url, '/');

    //处理cgi
    if (cgi == 1 && (*(p + 1) == '2' || *(p + 1) == '3'))
    {

        //根据标志判断是登录检测还是注册检测
        char flag = m_url[1];
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/");
        strcat(m_url_real, m_url + 2);
        strncpy(m_real_file + len, m_url_real, FILENAME_LEN - len - 1);
        free(m_url_real);

        //将用户名和密码提取出来
        //user=123&password=123
        char name[100], password[100];
        int i;
        for (i = 5; m_string[i] != '&'; ++i)
            name[i - 5] = m_string[i];
        name[i - 5] = '\0';

        int j = 0;
        for (i = i + 10; m_string[i] != '\0'; ++i, ++j)
            password[j] = m_string[i];
        password[j] = '\0';

        if (0 == m_SQLVerify)
        {
            if (*(p + 1) == '3')
            {
                //如果是注册，先检测数据库中是否有重名的
                //没有重名的，进行增加数据
                char *sql_insert = (char *)malloc(sizeof(char) * 200);
                strcpy(sql_insert, "INSERT INTO user(username, password) VALUES(");
                strcat(sql_insert, "'");
                strcat(sql_insert, name);
                strcat(sql_insert, "', '");
                strcat(sql_insert, password);
                strcat(sql_insert, "')");

                if (users.find(name) == users.end())
                {
                    m_lock.lock();
                    int res = mysql_query(mysql, sql_insert);
                    users.insert(pair<string, string>(name, password));
                    m_lock.unlock();

                    if (!res)
                        strcpy(m_url, "/log.html");
                    else
                        strcpy(m_url, "/registerError.html");
                }
                else
                    strcpy(m_url, "/registerError.html");
            }
            //如果是登录，直接判断
            //若浏览器端输入的用户名和密码在表中可以查找到，返回1，否则返回0
            else if (*(p + 1) == '2')
            {
                if (users.find(name) != users.end() && users[name] == password)
                    strcpy(m_url, "/welcome.html");
                else
                    strcpy(m_url, "/logError.html");
            }
        }
        else if (1 == m_SQLVerify)
        {
            //注册
            if (*(p + 1) == '3')
            {
                //如果是注册，先检测数据库中是否有重名的
                //没有重名的，进行增加数据
                char *sql_insert = (char *)malloc(sizeof(char) * 200);
                strcpy(sql_insert, "INSERT INTO user(username, password) VALUES(");
                strcat(sql_insert, "'");
                strcat(sql_insert, name);
                strcat(sql_insert, "', '");
                strcat(sql_insert, password);
                strcat(sql_insert, "')");

                if (users.find(name) == users.end())
                {
                    m_lock.lock();
                    int res = mysql_query(mysql, sql_insert);
                    users.insert(pair<string, string>(name, password));
                    m_lock.unlock();

                    if (!res)
                    {
                        strcpy(m_url, "/log.html");
                        m_lock.lock();
                        //每次都需要重新更新id_passwd.txt
                        ofstream out("id_passwd.txt", ios::app);
                        out << name << " " << password << endl;
                        out.close();
                        m_lock.unlock();
                    }
                    else
                        strcpy(m_url, "/registerError.html");
                }
                else
                    strcpy(m_url, "/registerError.html");
            }
            //登录
            else if (*(p + 1) == '2')
            {
                pid_t pid;
                int pipefd[2];
                if (pipe(pipefd) < 0)
                {
                    LOG_ERROR("pipe() error:%d", 4);
                    return BAD_REQUEST;
                }
                if ((pid = fork()) < 0)
                {
                    LOG_ERROR("fork() error:%d", 3);
                    return BAD_REQUEST;
                }

                if (pid == 0)
                {
                    //标准输出，文件描述符是1，然后将输出重定向到管道写端
                    dup2(pipefd[1], 1);
                    //关闭管道的读端
                    close(pipefd[0]);
                    //父进程去执行cgi程序，m_real_file,name,password为输入
                    //执行./CGIsql.cgi name password "id_passwd.txt" "1"
                    execl(m_real_file, name, password, "id_passwd.txt", "1", NULL);
                }
                else
                {
                    //子进程关闭写端，打开读端，读取父进程的输出
                    close(pipefd[1]);
                    char result;
                    int ret = recv(pipefd[0], &result,1, 1);

                    if (ret != 1)
                    {
                        LOG_ERROR("管道read error:ret=%d", ret);
                        return BAD_REQUEST;
                    }

                    LOG_INFO("%s", "登录检测");

                    //当用户名和密码正确，则显示welcome界面，否则显示错误界面
                    if (result == '1')
                        strcpy(m_url, "/welcome.html");
                    else
                        strcpy(m_url, "/logError.html");

                    //回收进程资源
                    waitpid(pid, NULL, 0);
                }
            }
        }

        //CGI多进程登录校验,不用数据库连接池
        //子进程完成注册和登录
        else
        {
            //fd[0]:读管道，fd[1]:写管道
            pid_t pid;
            int pipefd[2];
            if (pipe(pipefd) < 0)
            {
                LOG_ERROR("pipe() error:%d", 4);
                return BAD_REQUEST;
            }
            if ((pid = fork()) < 0)
            {
                LOG_ERROR("fork() error:%d", 3);
                return BAD_REQUEST;
            }

            if (pid == 0)
            {
                //标准输出，文件描述符是1，然后将输出重定向到管道写端
                dup2(pipefd[1], 1);

                //关闭管道的读端
                close(pipefd[0]);

                //父进程去执行cgi程序
                execl(m_real_file, &flag, name, password, "2", sql_user, sql_passwd, sql_name, NULL);
            }
            else
            {
                //子进程关闭写端，打开读端，读取父进程的输出
                close(pipefd[1]);
                char result;
                int ret =recv(pipefd[0], &result, 1,1);

                if (ret != 1)
                {
                    LOG_ERROR("管道read error:ret=%d", ret);
                    return BAD_REQUEST;
                }
                if (flag == '2')
                {
                    LOG_INFO("%s", "登录检测");


                    //当用户名和密码正确，则显示welcome界面，否则显示错误界面
                    if (result == '1')
                        strcpy(m_url, "/welcome.html");
                    else
                        strcpy(m_url, "/logError.html");
                }
                else if (flag == '3')
                {
                    LOG_INFO("%s", "注册检测");

                    //当成功注册后，则显示登陆界面，否则显示错误界面
                    if (result == '1')
                        strcpy(m_url, "/log.html");
                    else
                        strcpy(m_url, "/registerError.html");
                }
                //回收进程资源
                waitpid(pid, NULL, 0);
            }
        }
    }

    if (*(p + 1) == '0')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/register.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else if (*(p + 1) == '1')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/log.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else if (*(p + 1) == '5')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/picture.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else if (*(p + 1) == '6')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/video.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else if (*(p + 1) == '7')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/fans.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else
        strncpy(m_real_file + len, m_url, FILENAME_LEN - len - 1);

/////////
    if(stat(m_real_file,&m_file_stat)<0)
    {
        //找不到资源
        return NO_RESOURCE;
    }
    if(!(m_file_stat.st_mode&S_IROTH))
    {
        //OTHER组没有读权限
        return FORBIDDEN_REQUEST;
    }
    if(S_ISDIR(m_file_stat.st_mode))
    {
        //请求的是文件夹
        return BAD_REQUEST;
    }


    int fd=open(m_real_file,O_RDONLY);
    //将文件经过内存映射映射将进程的内存空间与磁盘进行映射
    m_file_address=(char*)mmap(NULL,m_file_stat.st_size,PROT_READ,MAP_PRIVATE,fd,0);
    close(fd);
    return FILE_REQUEST;   
}

void http_con::unmap()
{
    if(m_file_address)
    {
        munmap(m_file_address,m_file_stat.st_size);
        m_file_address=0;
    } 
}

//变参函数,拼接不同格式输出写入输出缓存
bool http_con::add_response(const char* format,...)
{
    if(m_write_idx>=WRITE_BUFFER_SIZE)
    {
        return false;
    }
    va_list arg_list;
    va_start(arg_list,format);
    int len=vsnprintf(m_write_buf+m_write_idx,WRITE_BUFFER_SIZE-1-m_write_idx,format,arg_list);
    //vsnprintf返回值：如果输入字符长度超过size,则返回该字符长度
    //Thus, a return value of size or more means that the output was truncated.
    if(len>=WRITE_BUFFER_SIZE-1-m_write_idx)
    {
        //写入字符串被截断，返回错误
        return false;
    }
    m_write_idx+=len;
    va_end(arg_list);
    LOG_INFO("request:%s", m_write_buf);
    return true;
}

//写入响应状态行
bool http_con::add_status_line(int status,const char* title)
{
    //响应状态行
    //HTTP/1.1 200 OK
    return add_response("%s %d %s\r\n","HTTP/1.1",status,title);
}

//写入响应状态头
bool http_con::add_header_line(int content_len)
{
    add_response("Content-Length: %d\r\n",content_len);
    add_response("Connection: %s\r\n",(m_linger==true)?"Keep-alive":"close");
    add_response("%s","\r\n");
}

bool http_con::add_content(const char* content)
{
    return add_response("%s",content);
}

void http_con::initmysql_result(connection_pool *connPool)
{
    //先从连接池中取一个连接
    MYSQL *mysql = NULL;
    connectionRAII mysqlcon(&mysql, connPool);

    //在user表中检索username，passwd数据，浏览器端输入
    if (mysql_query(mysql, "SELECT username,password FROM user"))
    {
        LOG_ERROR("SELECT error:%s\n", mysql_error(mysql));
    }

    //从表中检索完整的结果集
    MYSQL_RES *result = mysql_store_result(mysql);

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
}

void http_con::initresultFile(connection_pool *connPool)
{
    ofstream out("id_passwd.txt");
    //先从连接池中取一个连接
    MYSQL *mysql = NULL;
    connectionRAII mysqlcon(&mysql, connPool);

    //在user表中检索username，passwd数据，浏览器端输入
    if (mysql_query(mysql, "SELECT username,password FROM user"))
    {
        LOG_ERROR("SELECT error:%s\n", mysql_error(mysql));
    }

    //从表中检索完整的结果集
    MYSQL_RES *result = mysql_store_result(mysql);

    //返回结果集中的列数
    int num_fields = mysql_num_fields(result);

    //返回所有字段结构的数组
    MYSQL_FIELD *fields = mysql_fetch_fields(result);

    //从结果集中获取下一行，将对应的用户名和密码，存入map中
    while (MYSQL_ROW row = mysql_fetch_row(result))
    {
        string temp1(row[0]);
        string temp2(row[1]);
        out << temp1 << " " << temp2 << endl;
        users[temp1] = temp2;
    }

    out.close();
}

