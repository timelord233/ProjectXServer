#include<stdio.h>
#include<stdlib.h>
#include<strings.h>
#include<errno.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<string>
#include"msg.pb.h"
#include<iostream>

#define SERVER_PORT 2333 
#define BUFF_SIZE 4096
#define IP_ADDRESS "127.0.0.1"

struct TCP_MSG_HEAD{
	int len;
	int type;
};
int direction_x[4]={0,0,1,-1};
int direction_y[4]={1,-1,0,0}; 

void handle_movemoment(int fd){
    char c;
    int direction;
    scanf("%c",&c);
    while (1){
        scanf("%c",&c);
        switch (c)
        {
        case 'w':
            direction = 1;
            break;
        case 'a':
            direction = 2;
            break;
        case 's':
            direction = 3;
            break;
        case 'd':
            direction = 4;
            break;
        default:
            break;
        }
        demo::player Msg;
        std::string data;
        Msg.set_direction(direction);
        Msg.SerializeToString(&data);
        int len = data.length();
            
        unsigned char buff[len + sizeof(TCP_MSG_HEAD)];
        TCP_MSG_HEAD* head = (TCP_MSG_HEAD*)buff;
        memcpy(buff+sizeof(TCP_MSG_HEAD),data.c_str(),len);
        head->len = len;
        head->type = 3;
        if( send(fd, buff, sizeof(buff), 0) < 0){
            printf("send msg error: %s(errno: %d)\n", strerror(errno), errno);
            return ;
        }
        memset(buff,0,sizeof(buff));
        int n=recv(fd,buff,BUFF_SIZE,0);

        head = (TCP_MSG_HEAD*)buff;
        len = head->len;
        int type = head->type;
            
        unsigned char * tmpbuff = buff;
        tmpbuff += sizeof(TCP_MSG_HEAD);
        demo::player ReturnMsg;
        std::string ReturnData = (char *)tmpbuff;
        ReturnMsg.ParseFromString(ReturnData);
        printf("X: %d,Y: %d\n",ReturnMsg.x(),ReturnMsg.y());
    }  
}
int main(){
    int   sockfd, n;
    struct sockaddr_in  servaddr;

    if( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        printf("create socket error: %s(errno: %d)\n", strerror(errno),errno);
        return 0;
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(SERVER_PORT);
    if( inet_pton(AF_INET, IP_ADDRESS, &servaddr.sin_addr) <= 0){
        printf("inet_pton error for %s\n",IP_ADDRESS);
        return 0;
    }

    if( connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0){
        printf("connect error: %s(errno: %d)\n",strerror(errno),errno);
        return 0;
    }

    printf("Please choose to Register or Login: 1/2   ");
    int t;
    scanf("%d",&t);
    if (t==1){
        //register
        printf("Please Enter Your Username: ");
        char Username[100];
        scanf("%s",Username);
        printf("Please Enter Your Password: ");
        char Password[100];
        scanf("%s",Password);
        
        std::string data;
        demo::login LoginMsg;
        LoginMsg.set_username(Username);
        LoginMsg.set_password(Password);
        int t = LoginMsg.SerializeToString(&data);
        int len = data.length();

        unsigned char buff[len + sizeof(TCP_MSG_HEAD)];
        TCP_MSG_HEAD* head = (TCP_MSG_HEAD*)buff;
        memcpy(buff+sizeof(TCP_MSG_HEAD),data.c_str(),len);
        head->len = len;
        head->type = 1;

        if( send(sockfd, buff, sizeof(buff), 0) < 0){
            printf("send msg error: %s(errno: %d)\n", strerror(errno), errno);
            return 0;
        }
        memset(buff,0,sizeof(buff));
        recv(sockfd,buff,BUFF_SIZE,0);

        head = (TCP_MSG_HEAD*)buff;
		len = head->len;
		int type = head->type;
        
        unsigned char * tmpbuff = buff;
        tmpbuff += sizeof(TCP_MSG_HEAD);
        demo::player ReturnMsg;
        std::string ReturnData = (char *)tmpbuff;
        
		ReturnMsg.ParseFromString(ReturnData);
        if (ReturnMsg.type() == 1){
            printf("UserName already exists!\n");
        }
        else if(ReturnMsg.type()==2){
            printf("Register Succeed!\n");
        }

    }else {
        //login
        printf("Please Enter Your Username: ");
        char Username[100];
        scanf("%s",Username);
        printf("Please Enter Your Password: ");
        char Password[100];
        scanf("%s",Password);
        
        std::string data;
        demo::login LoginMsg;
        LoginMsg.set_username(Username);
        LoginMsg.set_password(Password);
        int t = LoginMsg.SerializeToString(&data);
        int len = data.length();

        unsigned char buff[len + sizeof(TCP_MSG_HEAD)];
        TCP_MSG_HEAD* head = (TCP_MSG_HEAD*)buff;
        memcpy(buff+sizeof(TCP_MSG_HEAD),data.c_str(),len);
        head->len = len;
        head->type = 2;

        if( send(sockfd, buff, sizeof(buff), 0) < 0){
            printf("send msg error: %s(errno: %d)\n", strerror(errno), errno);
            return 0;
        }
        memset(buff,0,sizeof(buff));
        int n=recv(sockfd,buff,BUFF_SIZE,0);

        head = (TCP_MSG_HEAD*)buff;
		len = head->len;
		int type = head->type;
        
        unsigned char * tmpbuff = buff;
        tmpbuff += sizeof(TCP_MSG_HEAD);
        demo::player ReturnMsg;
        std::string ReturnData = (char *)tmpbuff;
        ReturnMsg.ParseFromString(ReturnData);

        if (ReturnMsg.type() == 3){
            printf("Login!\n");
            handle_movemoment(sockfd);
        }
        else {
            printf("Username or Password Is Wrong!\n");
        }
    }

    close(sockfd);
    
    return 0;
}