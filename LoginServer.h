#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<errno.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<sys/epoll.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<unistd.h>
#include<fcntl.h>

#include<string>
#include<iostream>
#include<fstream>
#include<map>
#include"myDB.h"
#include"msg.pb.h"

#define SERVER_PORT 2333
#define BUFF_SIZE 4096
#define FD_SIZE 1000
#define EPOLL_EVENTS 100

class LoginServer{

protected:
    struct TCP_MSG_HEAD{
	    int len;
	    int type;
    };

    struct TCP_MSG{
	    unsigned char TCP_buff[BUFF_SIZE << 1];
	    int unhandle_len;
	    int unhandle_flag;
    };
    std::map<int,TCP_MSG> map_fd_msg;

    struct RETURN_MSG{
        int type;
        std::string ue4server;
        int direction;
        std::string ip;
        int port;
        int playerid;
    };

    std::map<int,RETURN_MSG> map_fd_return;
    std::map<int,int> map_id_fd;
    std::set<int> connected_fd;
    std::string current_ue4server = "106.14.154.159";
    int current_port = 7777;
    int online_num = 0;


public:
    LoginServer(){};
    void add_epoll_event(int epollfd,int fd,int state);
    void delete_epoll_event(int epollfd,int fd,int state);
    void modify_event(int epollfd,int fd,int state);
    void handle_accept(int epollfd,int listenfd);
    void boardcast(int epollfd,int now_fd);
    void handle_tcp_msg(unsigned char* buff,int type,int fd,int epollfd);
    void handle_read(int epollfd,int fd,unsigned char* buff);
    void handle_write(int epollfd,int fd,unsigned char* buff);
    void handle_epoll_events(int epollfd,struct epoll_event *events,int num,int listenfd,unsigned char* buff);
    void connect_matchserver();
    void connect_DS(int PlayerId,int epollfd,int port);
    void join_matchserver(int fd);
    myDB db;
    int matchfd;
};