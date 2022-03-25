#include "LoginServer.h"

void LoginServer::add_epoll_event(int epollfd,int fd,int state){
	struct epoll_event ev;
	ev.events = state;
	ev.data.fd = fd;
	epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&ev);
}

void LoginServer::delete_epoll_event(int epollfd,int fd,int state){
	struct epoll_event ev;
	ev.events = state;
	ev.data.fd = fd;
	epoll_ctl(epollfd,EPOLL_CTL_DEL,fd,&ev);
}

void LoginServer::modify_event(int epollfd,int fd,int state){
	struct epoll_event ev;
	ev.events = state;
	ev.data.fd = fd;
	epoll_ctl(epollfd,EPOLL_CTL_MOD,fd,&ev);
}

void LoginServer::handle_accept(int epollfd,int listenfd){
	int connfd;
	struct sockaddr_in connaddr;
	socklen_t  connaddrlen;

	if( (connfd = accept(listenfd, (struct sockaddr*)&connaddr, &connaddrlen)) == -1){
		printf("accept socket error: %s(errno: %d)",strerror(errno),errno);
	}else {
		int flags =fcntl(connfd,F_GETFL,0);
		fcntl(connfd,F_SETFL,flags| O_NONBLOCK);
		printf("accept a new client: %s:%d\n",inet_ntoa(connaddr.sin_addr),connaddr.sin_port);
		add_epoll_event(epollfd,connfd,EPOLLIN);
		connected_fd.insert(connfd);
		
		TCP_MSG* tcp_msg = (TCP_MSG*)malloc(sizeof(TCP_MSG));
		tcp_msg->unhandle_len = 0;
		tcp_msg->unhandle_flag = 0;
		memset(tcp_msg->TCP_buff,0,sizeof(tcp_msg->TCP_buff));
		map_fd_msg[connfd] = *tcp_msg;

		RETURN_MSG* return_msg =new RETURN_MSG;
		return_msg->type =0;
		return_msg->direction = 0;
		return_msg->ue4server = "";
		return_msg->ip = inet_ntoa(connaddr.sin_addr);
		return_msg->port = connaddr.sin_port;
		map_fd_return[connfd] = *return_msg;
	}
}

void LoginServer::boardcast(int epollfd,int now_fd){
	unsigned char * buff = new unsigned char[BUFF_SIZE];
	for (auto fd:connected_fd){
		if (fd == now_fd) continue;
		ProjectX::player ReturnMsg;
		ReturnMsg.set_type(map_fd_return[fd].type);
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
			delete_epoll_event(epollfd,fd,EPOLLOUT);
    		return;
		}
	}
}

void LoginServer::connect_DS(int PlayerId,int epollfd,int port){
	int fd = map_id_fd[PlayerId];
	ProjectX::player ReturnMsg;
	ReturnMsg.set_ue4server(current_ue4server);
	ReturnMsg.set_port(port);
	std::string data;
	ReturnMsg.SerializeToString(&data);
	int len = data.length();
	unsigned char * buff = new unsigned char[len+sizeof(TCP_MSG_HEAD)];
    TCP_MSG_HEAD* head = (TCP_MSG_HEAD*)buff;
    memcpy(buff+sizeof(TCP_MSG_HEAD),data.c_str(),len);
    head->len = len;
    head->type = 4;
		
	if (send(fd,buff,len+sizeof(TCP_MSG_HEAD),0) <0 ){
		printf("send msg error: %s(errno: %d)\n", strerror(errno), errno);
		close(fd);
		delete_epoll_event(epollfd,fd,EPOLLOUT);
		delete(buff);
    	return;
	}
	delete(buff);
}

void LoginServer::handle_tcp_msg(unsigned char* buff,int type,int fd,int epollfd){

	switch (type)
	{
	case 1:{
		ProjectX::login RegisterMsg;
		std::string Registerdata = (char *)buff;
		RegisterMsg.ParseFromString(Registerdata);
		if (db.select_username(RegisterMsg.username())) {
			map_fd_return[fd].type = 1;
		}else {
			db.insertSQL(RegisterMsg.username(),RegisterMsg.password());
			map_fd_return[fd].type = 2;
		}
		modify_event(epollfd,fd,EPOLLOUT);
		break;
	}
	case 2:{
		ProjectX::login LoginMsg;
		std::string Logindata = (char *)buff;
		LoginMsg.ParseFromString(Logindata);
		int playerid;
		if (db.check_password(LoginMsg.username(),LoginMsg.password(),playerid)) {
			map_fd_return[fd].type = 3;
			map_fd_return[fd].playerid = playerid;
			map_id_fd[playerid] = fd;
		}else {
			map_fd_return[fd].type = 4;
		}
		modify_event(epollfd,fd,EPOLLOUT);
		break;
	}
	case 3:{
		join_matchserver(fd);
		break;
	}
	case 4:{
		ProjectX::match MatchMsg;
		std::string Matchdata = (char *)buff;
		MatchMsg.ParseFromString(Matchdata);
		connect_DS(MatchMsg.playerid(),epollfd,MatchMsg.port());
		break;
	}
	default:
		break;
	}
}

void LoginServer::handle_read(int epollfd,int fd,unsigned char* buff){
	
	std::map<int,TCP_MSG>::iterator iter = map_fd_msg.find(fd);
	TCP_MSG* tcp_msg = &iter->second;

	memset(buff,0,BUFF_SIZE);
	int n = recv(fd, buff, BUFF_SIZE, 0);
	
	if (n==0){
		printf("recv shutdown command\n");
		close(fd);
		delete_epoll_event(epollfd,fd,EPOLLIN);
		connected_fd.erase(fd);
		return;
	}
	if (n<0){
		printf("recv messege error: %s(errno: %d)",strerror(errno),errno);
		close(fd);
		delete_epoll_event(epollfd,fd,EPOLLIN);
		connected_fd.erase(fd);
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
				
				handle_tcp_msg(buff + sizeof(TCP_MSG_HEAD),type,fd,epollfd);
				
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

void LoginServer::handle_write(int epollfd,int fd,unsigned char* buff){
	ProjectX::player ReturnMsg;
	ReturnMsg.set_type(map_fd_return[fd].type);
	ReturnMsg.set_uid(fd);
	ReturnMsg.set_ue4server(map_fd_return[fd].ue4server);
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
		delete_epoll_event(epollfd,fd,EPOLLOUT);
    	return;
	}
	modify_event(epollfd,fd,EPOLLIN);
}

void LoginServer::handle_epoll_events(int epollfd,struct epoll_event *events,int num,int listenfd,unsigned char* buff){
	for (int i=0;i<num;i++){
		int fd = events[i].data.fd;
		if ((fd == listenfd) && (events[i].events & EPOLLIN))
			handle_accept(epollfd,listenfd);
		else if (events[i].events & EPOLLIN)
			handle_read(epollfd,fd,buff);
		else if (events[i].events & EPOLLOUT)
			handle_write(epollfd,fd,buff);
	}

}

void LoginServer::connect_matchserver(){

    if( (matchfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        printf("create socket error: %s(errno: %d)\n", strerror(errno),errno);
        return;
    }
    struct sockaddr_in  matchaddr;
	memset(&matchaddr, 0, sizeof(matchaddr));
    matchaddr.sin_family = AF_INET;
    matchaddr.sin_port = htons(2334);
	if( inet_pton(AF_INET, "127.0.0.1", &matchaddr.sin_addr) <= 0){
        return ;
    }
	if( connect(matchfd, (struct sockaddr*)&matchaddr, sizeof(matchaddr)) < 0){
        printf("connect error: %s(errno: %d)\n",strerror(errno),errno);
        return ;
    }
	
}

void LoginServer::join_matchserver(int fd){
	ProjectX::match MatchMsg;
	MatchMsg.set_type(1);
	MatchMsg.set_playerid(map_fd_return[fd].playerid);
	std::string data;
	MatchMsg.SerializeToString(&data);
	int len = data.length();
	unsigned char * buff = new unsigned char [len+sizeof(TCP_MSG_HEAD)];
    TCP_MSG_HEAD* head = (TCP_MSG_HEAD*)buff;
    memcpy(buff+sizeof(TCP_MSG_HEAD),data.c_str(),len);
    head->len = len;
    head->type = 4;
	if (send(matchfd,buff,len+sizeof(TCP_MSG_HEAD),0) <0 ){
		printf("send msg error: %s(errno: %d)\n", strerror(errno), errno);
		delete(buff);
    	return;
	}
	delete(buff);
}

int main(int argc, char** argv){

	LoginServer* loginserver = new LoginServer();
	loginserver->db.initDB("localhost","root","","test_db");
	loginserver->connect_matchserver();
	

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
	
	int len=0,ret;

	int epollfd = epoll_create(FD_SIZE);
	loginserver->add_epoll_event(epollfd,listenfd,EPOLLIN);
	loginserver->add_epoll_event(epollfd,loginserver->matchfd,EPOLLIN);
	struct epoll_event events[EPOLL_EVENTS];

	printf("???\n");
	while(1){
		
		ret = epoll_wait(epollfd,events,EPOLL_EVENTS,-1);
		loginserver->handle_epoll_events(epollfd,events,ret,listenfd,buff);
		
	}

	close(epollfd);
	return 0;
}
		