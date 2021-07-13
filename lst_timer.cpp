/*
 * @Author: zx
 * @Date: 2021-07-08 14:57:03
 * @LastEditTime: 2021-07-08 15:20:01
 * @LastEditors: zx
 * @Description: 定时器实现
 * @FilePath: /zx/桌面/webserver/lst_timer.cpp
 * 可以输入预定的版权声明、个性签名、空行等
 */
#include "lst_timer.h"



Timer_list::Timer_list() {

}

Timer_list::~Timer_list() {
    m_timer_list.clear();
}

void Timer_list::add_timer(util_timer* timer) { //将定时器添加到链表
    if (!timer) return;
    else {
        auto item = m_timer_list.begin();
        while (item != m_timer_list.end()) {
            if (timer->expire < (*item)->expire) {
                m_timer_list.insert(item, timer);
                return;
            }
            item++;
        }
        m_timer_list.emplace_back(timer);
    }
}

void Timer_list::del_timer(util_timer* timer) { //将定时器从链表删除
    if (!timer) return;
    else {
        auto item = m_timer_list.begin();
        while (item != m_timer_list.end()) {
            if (timer == *item) {
                m_timer_list.erase(item);
                return;
            }
            item++;
        }
    }
}

void Timer_list::adjust_timer(util_timer *timer) { //调整定时器在链表中的位置
    del_timer(timer);
    add_timer(timer);
}

void Timer_list::tick() { //SIGALRM信号触发，处理链表上到期的任务
    if (m_timer_list.empty()) return;
    time_t cur = time(nullptr);
    
    //检测当前定时器链表中到期的任务。
    while (!m_timer_list.empty()) {
        util_timer* temp = m_timer_list.front();
        if (cur < temp->expire) break;
        temp->callBackFunc(temp->userData);
        m_timer_list.pop_front();
    }
}

void Utils::init(int timeslot)
{
    m_TIMESLOT = timeslot;
}

//对文件描述符设置非阻塞
int Utils::setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

//将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
void Utils::addfd(int epollfd, int fd, bool one_shot, int TRIGMode)
{
    epoll_event event;
    event.data.fd = fd;

    if (1 == TRIGMode)
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    else
        event.events = EPOLLIN | EPOLLRDHUP;

    if (one_shot)
        event.events |= EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

//信号处理函数
void Utils::sig_handler(int sig)
{
    //为保证函数的可重入性，保留原来的errno
    int save_errno = errno;
    int msg = sig;
    send(u_pipefd[1], (char *)&msg, 1, 0);
    errno = save_errno;
}

//设置信号函数
void Utils::addsig(int sig, void(handler)(int), bool restart)
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if (restart)
        sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

//定时处理任务，重新定时以不断触发SIGALRM信号
void Utils::timer_handler()
{
    m_Timer_list.tick();
    alarm(m_TIMESLOT);
}

void Utils::show_error(int connfd, const char *info)
{
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

int *Utils::u_pipefd = 0;
int Utils::u_epollfd = 0;

void callBackFunc(client_data *user_data)
{
    epoll_ctl(Utils::u_epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    assert(user_data);
    close(user_data->sockfd);
    http_con::m_user_count--;
}
