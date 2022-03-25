#include "MatchServer.h"

void MatchServer::boardcast(int epollfd,int now_fd){
	unsigned char * buff = new unsigned char[BUFF_SIZE];
	for (auto fd:connected_fd){
		if (fd == now_fd) continue;
		ProjectX::player ReturnMsg;
		std::string data;
		ReturnMsg.SerializeToString(&data);
		int len = data.length();
		memset(buff,0,len+sizeof(TCP_MSG_HEAD));
    	TCP_MSG_HEAD* head = (TCP_MSG_HEAD*)buff;
    	memcpy(buff+sizeof(TCP_MSG_HEAD),data.c_str(),len);
    	head->len = len;
    	head->type = 3;
		
		if (send(fd,buff,len+sizeof(TCP_MSG_HEAD),0) <0 ){
			printf("send msg error: %s(errno: %d)\n", strerror(errno), errno);
			close(fd);
    		return;
		}
	}
}
void MatchServer::handle_tcp_msg(unsigned char* buff,int type,int fd){

	ProjectX::match MatchMsg;
	std::string Matchdata = (char *)buff;
	MatchMsg.ParseFromString(Matchdata);
	switch (MatchMsg.type())
	{
	case 1:{
		MatchPlayerInfo player(MatchMsg.playerid(),0,0);
		PutPlayerIntoPool(player);
		break;
	}
	default:
		break;
	}
}

void MatchServer::connect_fd(int listenfd){
	
	struct sockaddr_in connaddr;
	socklen_t  connaddrlen;
	if( (connfd = accept(listenfd, (struct sockaddr*)&connaddr, &connaddrlen)) == -1){
		printf("accept socket error: %s(errno: %d)",strerror(errno),errno);
	}else {
		printf("accept a new client: %s:%d\n",inet_ntoa(connaddr.sin_addr),connaddr.sin_port);
		recv_msg(connfd);
	}
}
void MatchServer::recv_msg(int fd){
	TCP_MSG* tcp_msg = (TCP_MSG*)malloc(sizeof(TCP_MSG));
	tcp_msg->unhandle_len = 0;
	tcp_msg->unhandle_flag = 0;
	memset(tcp_msg->TCP_buff,0,sizeof(tcp_msg->TCP_buff));
	unsigned char * buff = new unsigned char[BUFF_SIZE];
	while (1){
		int n = recv(fd, buff, BUFF_SIZE, 0);
		if (n==0){
			printf("recv shutdown command\n");
			close(fd);
			return;
		}
		if (n<0){
			printf("recv messege error: %s(errno: %d)",strerror(errno),errno);
			close(fd);
			return;
		}
		if (tcp_msg->unhandle_flag == 1){
			memcpy(tcp_msg->TCP_buff+tcp_msg->unhandle_len,buff,n);
			n += tcp_msg->unhandle_len;
			memcpy(buff,tcp_msg->TCP_buff,n);
			tcp_msg->unhandle_flag = 0;
		}
		while (1){
			if (n>=sizeof(TCP_MSG_HEAD)){
				TCP_MSG_HEAD *head = (TCP_MSG_HEAD*)buff;
				int len = head->len;
				int type = head->type;

				if (n >= len + sizeof(TCP_MSG_HEAD)){
					handle_tcp_msg(buff + sizeof(TCP_MSG_HEAD),type,fd);
					if (n == len + sizeof(TCP_MSG_HEAD)) break;
					n -= len + sizeof(TCP_MSG_HEAD);
					buff += len + sizeof(TCP_MSG_HEAD);
				}else {
					memset(tcp_msg->TCP_buff,0,sizeof(tcp_msg->TCP_buff));
					memcpy(tcp_msg->TCP_buff,buff,n);
					tcp_msg->unhandle_len = n;
					tcp_msg->unhandle_flag = 1;
					break;
				}
			}else {
				memset(tcp_msg->TCP_buff,0,sizeof(tcp_msg->TCP_buff));
				memcpy(tcp_msg->TCP_buff,buff,n);
				tcp_msg->unhandle_len = n;
				tcp_msg->unhandle_flag = 1;
				break;
			}
		}
	}
	
}
void MatchServer::InitMatchPool(){
	Pool = new MatchPool();
}

void MatchServer::SetModel(){
	Model.MinCount = 2;
	Model.Count = 10;
	Model.SavePoolCheckTime = 2;
	Model.SavePoolTime = 4;
	Model.SavePoolNum = 2;
}

bool MatchServer::PutPlayerIntoPool(MatchPlayerInfo player){
	std::unique_lock<std::shared_mutex> lck(mutex_);
	if (std::find(Pool->PlayerArr.cbegin(),Pool->PlayerArr.cend(),player)!=Pool->PlayerArr.end()){
		return false;
	}
	Pool->PlayerArr.push_back(player);
	Pool->size++;
	return true;
}

bool MatchServer::PutPlayerArrIntoPool(std::vector<MatchPlayerInfo> pArr){
	bool res = true;
	for (auto p:pArr){
		res = res && PutPlayerIntoPool(p);
	}
	return res;
}

bool MatchServer::RemovePlayerOutPool(int pId){
	std::unique_lock<std::shared_mutex> lck(mutex_);
	for (auto iter=Pool->PlayerArr.cbegin();iter!=Pool->PlayerArr.cend();iter++){
		if (iter->PlayerId == pId){
			Pool->PlayerArr.erase(iter);
			Pool->size--;
			return true;
		}
	}
	return false;
}

bool MatchServer::RemovePlayerArrOutPool(std::vector<MatchPlayerInfo> pArr){
	bool res = true;
	for (auto p:pArr){
		res = res && RemovePlayerOutPool(p.PlayerId);
	}
	return res;
}

void MatchServer::SelectPlayerByCount(int count,std::vector<MatchPlayerInfo> &res){
	//mtx.lock_shared();
	for (auto p:Pool->PlayerArr){
		res.push_back(p);
		RemovePlayerOutPool(p.PlayerId);
		count--;
		if (count<=0) break;
	}
	//mtx.unlock_shared();
}

void MatchServer::SelectPlayerByParam(int p1,int p2,int count,std::vector<MatchPlayerInfo> &res){
	//mtx.lock_shared();
	int times = 1;
	while (times<=3 && count>0){
		for (auto p:Pool->PlayerArr){
			bool flag = true;
			if (times == 1){
				flag = (p1 == p.Param1) && (p2 == p.Param2);
			}else if (times == 2){
				flag = (p1 == p.Param1);
			}
			if (flag) {
				res.push_back(p);
				RemovePlayerOutPool(p.PlayerId);
				count--;
			}
			if (count<=0) break;
		}
		times++;
	}
	//mtx.unlock_shared();
}

void MatchServer::SelectPlayerByWaitTime(int waittime,std::vector<MatchPlayerInfo> &res){
	//mtx.lock_shared();
	for (auto p:Pool->PlayerArr){
		if (p.WaitTime() >= waittime){
			res.push_back(p);
			RemovePlayerOutPool(p.PlayerId);
		}
	}
	//mtx.unlock_shared();
}

void MatchServer::RefushMaxWait(){
	//mtx.lock_shared();
	int maxwait = 0;
	for (auto p:Pool->PlayerArr){
		if (maxwait <= p.WaitTime()){
			maxwait = p.WaitTime();
			Pool->MaxWaitPlayer = &p;
		}
	}
	//mtx.unlock_shared();
}

void MatchServer::HandleMatch(std::vector<MatchPlayerInfo> MatchPlayer){
	if (fork() == 0){
		char *execv_str[] = {(char*)"",(char*)"executed by execv",NULL};
		if (execv("/home/Timesong/server/ProjectXServer.sh",execv_str)<0){
			perror("error on exec");
			exit(0);
        }
	}else {
		for (auto p:MatchPlayer){
			ProjectX::match MatchMsg;
			MatchMsg.set_type(2);
			MatchMsg.set_playerid(p.PlayerId);
			MatchMsg.set_port(current_port);
			std::string data;
			MatchMsg.SerializeToString(&data);
			int len = data.length();
			unsigned char * buff = new unsigned char [len+sizeof(TCP_MSG_HEAD)];
    		TCP_MSG_HEAD* head = (TCP_MSG_HEAD*)buff;
    		memcpy(buff+sizeof(TCP_MSG_HEAD),data.c_str(),len);
    		head->len = len;
    		head->type = 4;
			if (send(connfd,buff,len+sizeof(TCP_MSG_HEAD),0) <0 ){
				printf("send msg error: %s(errno: %d)\n", strerror(errno), errno);
				delete(buff);
    			return;
			}
			delete(buff);
		}
		current_port++;
	}
}

void MatchServer::MatchProcess(){
	while (1){
		if (Pool->size<=0) continue;
		RefushMaxWait();
		std::vector<MatchPlayerInfo> WaitLongPool;
		WaitLongPool.clear();
		if (Pool->size < Model.SavePoolNum && Pool->MaxWaitPlayer->WaitTime()<Model.SavePoolTime){
			sleep(Model.SavePoolCheckTime);
			continue;
		}
		while (Pool->size >= Model.MinCount){
			SelectPlayerByWaitTime(Model.SavePoolTime,WaitLongPool);
			RefushMaxWait();
			std::vector<MatchPlayerInfo> MatchPlayer;
			MatchPlayer.clear();
			if (WaitLongPool.size()>0){
				for (auto iter = WaitLongPool.cbegin();iter!=WaitLongPool.cend();iter++){
					MatchPlayer.push_back(*iter);
					WaitLongPool.erase(iter);
					iter--;
					if (WaitLongPool.size()==0) break;
					if (MatchPlayer.size() == Model.Count) 
						break; 
				}
			}
			if (MatchPlayer.size()<=Model.Count){
				int p1 = Pool->MaxWaitPlayer->Param1;
				int p2 = Pool->MaxWaitPlayer->Param2;
				int needCount = Model.Count - MatchPlayer.size();
				SelectPlayerByParam(p1,p2,needCount,MatchPlayer);
				if (MatchPlayer.size()>=Model.MinCount){
					HandleMatch(MatchPlayer);
				} 
				else {
					PutPlayerArrIntoPool(MatchPlayer);
				}
			}
			PutPlayerArrIntoPool(WaitLongPool);
		}
	}
}

int main(int argc, char** argv){

	MatchServer* matchserver = new MatchServer();
	matchserver->db.initDB("localhost","root","","test_db");
	int listenfd, connfd;
	struct sockaddr_in servaddr;
	unsigned char buff[BUFF_SIZE],new_buff[BUFF_SIZE],ReturnBuff[BUFF_SIZE];
	int  n;

	if( (listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1){
		printf("create socket error: %s(errno: %d)\n",strerror(errno),errno);
		return 0;
	}

    memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(SERVER_PORT);

	if( bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) == -1){
		printf("bind socket error: %s(errno: %d)\n",strerror(errno),errno);
		return 0;
	}
	if( listen(listenfd, 10) == -1){
		printf("listen socket error: %s(errno: %d)\n",strerror(errno),errno);
		return 0;
	}
	matchserver->InitMatchPool();
	matchserver->SetModel();
	std::thread MP(&MatchServer::MatchProcess,matchserver);

	matchserver->connect_fd(listenfd);
	MP.join();
	return 0;
}
		