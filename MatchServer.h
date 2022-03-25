#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<errno.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<sys/epoll.h>
#include<sys/time.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<unistd.h>
#include<fcntl.h>

#include<string>
#include<vector>
#include<map>
#include<thread>
#include<mutex>
#include<shared_mutex>
#include"myDB.h"
#include"msg.pb.h"

#define SERVER_PORT 2334
#define BUFF_SIZE 4096
#define FD_SIZE 1000
#define EPOLL_EVENTS 100

class MatchServer{

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

    };
    std::map<int,RETURN_MSG> map_fd_return;
    std::set<int> connected_fd;

    struct MatchParamModel {
        int SavePoolCheckTime;
        int SavePoolTime;
        int SavePoolNum;
        int MinCount;
        int Count;
    };

    struct MatchPlayerInfo {
        int PlayerId;
        int Param1;
        int Param2;
        int StartTime;
        MatchPlayerInfo(int pId,int p1,int p2){
            PlayerId = pId;
            Param1 = p1;
            Param2 = p2;
            struct timeval tv;
            gettimeofday(&tv, NULL);
            StartTime = tv.tv_sec;
        }
        int WaitTime(){
            int res;
            struct timeval tv;
            gettimeofday(&tv, NULL);
            if (tv.tv_sec>StartTime) 
                res=tv.tv_sec-StartTime;
            return res;
        }
        bool operator == (const MatchPlayerInfo & p)const{
            if (PlayerId == p.PlayerId) return true;
            else return false;
        }
    };

    struct MatchPool {
        std::vector<MatchPlayerInfo> PlayerArr;
        int size;
        MatchPlayerInfo* MaxWaitPlayer;
        MatchPool(){
            PlayerArr.clear();
            size = 0;
        }
    };

    MatchPool* Pool;
    MatchParamModel Model;
    mutable std::shared_mutex mutex_;
    int current_port = 7777;
    int connfd;
    
public:
    MatchServer(){};
    void connect_fd(int listenfd);
    void recv_msg(int fd);
    void boardcast(int epollfd,int now_fd);
    void handle_tcp_msg(unsigned char* buff,int type,int fd);
    bool PutPlayerIntoPool(MatchPlayerInfo player);
    bool PutPlayerArrIntoPool(std::vector<MatchPlayerInfo> pArr);
    bool RemovePlayerOutPool(int pId);
    bool RemovePlayerArrOutPool(std::vector<MatchPlayerInfo> pArr);
    void SelectPlayerByCount(int count,std::vector<MatchPlayerInfo> &res);
    void SelectPlayerByParam(int p1,int p2,int count,std::vector<MatchPlayerInfo> &res);
    void SelectPlayerByWaitTime(int waittime,std::vector<MatchPlayerInfo> &res);
    void MatchProcess();
    void HandleMatch(std::vector<MatchPlayerInfo> MatchPlayer);
    void InitMatchPool();
    void SetModel();
    void RefushMaxWait();
    myDB db;
    
};