#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <map>
#include <string.h>
#include <queue>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/ipc.h>
#include <pthread.h>
#include <sys/shm.h>
#include "project.pb.h"
#include "MsgQueue.h"
#include "CSql.h"
#include "CRedis.h"
using namespace std;
MsgQueue* RMSG;//接收消息
MsgQueue* SMSG;//发送消息
CRedis* redis;
CSql* sql;
int sockfd;
string IntToString(int num)
{
	string str = "";
	if(num == 0)
	{
		str += "0";
		return str;
	}
	while(num)
	{
		str += num%10+'0';
		num/=10;
	}
	reverse(str.begin(),str.end());
	return str;
}
int StringToInt(string str)
{
	int fd = 0;
	for(int i = 0; i < str.size(); i ++ )
	{
		fd = fd * 10 + str[i] - '0';
	}
	return fd;
}
void StringToProtobuf(Account& nAccount, string& Message)
{
	char p[1024];
	memset(p, '\0', sizeof p);
	for(int j = 0; j < Message.size(); j ++){
		p[j] = Message[j];
	}
	nAccount.ParseFromArray(p, Message.size());
}
void AddPack(char *Newdata,char *data, int len)//给消息包加头
{
	int len1=len;
	for(int i = 3; i >= 0; i --)
	{
		if(len1 > 0)
		{
			Newdata[i] = len1 % 10 + '0';
			len1 /= 10;
		}
		else Newdata[i] = '0';
	}
	for(int i = 0; i < len; i ++)
	{
		Newdata[i + 4] = data[i];
	}
}
void SendToGateServer(Account& nAccount)//flag记录是什么事件
{
	char p[1024],pp[1024];
	memset(p,'\0',sizeof p);
	memset(pp,'\0',sizeof pp);
	int sz = nAccount.ByteSize();
	nAccount.SerializeToArray(p, sz);
	AddPack(pp, p, sz);
	write(sockfd, pp, sz + 4);
}
bool Isexist(string str)
{
    string password = "";//用来存储查到的密码
    int ret = redis->Redis_Query(str, password);
    if(password == "")
    {
        password = sql->Sql_Query(str, password);
        if(password == "")
        {
            return true;
        }
        else return false;
    }
    else return false;
}
void Player_check(Account& nAccount)
{
    int flag = 0;
    string password = "";//用来存储查到的密码
    string UID = nAccount.uid();
    int ret = redis->Redis_Query(UID, password);
    if(password == "")//redis未查到密码，查mysql
    {
        flag = sql->Sql_Query(UID, password);
        if(flag == 1 && password == nAccount.password())//mysql内有,存入Redis
        {
            redis->Redis_Write(UID, password);
        }
    }
    if(password == nAccount.password())
    {
        nAccount.set_flag(true);
    }
    else
    {
        nAccount.set_flag(false);
    }
    SendToGateServer(nAccount);
}
void Player_register(Account& nAccount)
{
    string str;
    srand(unsigned(time(nullptr)));
    while(1)
    {
        str="";
        for(int i = 1; i <= 8; i ++ )
        {
            int random = rand()%10;
            str += random + '0';
        }
        if(Isexist(str))//账号合法
        {
			nAccount.set_flag(true);
			string name = nAccount.name();
			string password = nAccount.password();
            sql->Sql_Write(str, name, password);
			redis->Redis_Write(str, password);
			SendToGateServer(nAccount);
            break;
        }
        else sleep(0.03);
    }
}
static void * Rpthread(void *arg)
{
    string Message = "";
	int flag = 0, number = 0, id = 0;
	if( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		perror("socket");
	}
	struct hostent* h;
	if( (h = gethostbyname("10.0.128.212")) == 0)
	{
		printf("gethostbyname failed,\n");
		close(sockfd);
	}
	struct sockaddr_in servaddr;
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(atoi("1111"));
	memcpy(&servaddr.sin_addr, h->h_addr, h->h_length);
	if(connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) != 0)
	{
		perror("connect");
		close(sockfd);
	}
	char buffer[1024];
	while(1)
	{
		int iret;
		memset(buffer, 0, sizeof(buffer));
		if( (iret = recv(sockfd, buffer, sizeof(buffer), 0)) <= 0)
		{
			printf("iret=%d\n",iret);
			break;
		}
		for(int i = 0; i < iret; i ++ )
		{
			if(number > 0 && id == 4)
			{
			    Message += buffer[i];
			    number --;
			}
			if(number == 0 && id == 4)
			{
				Account nAccount;
				StringToProtobuf(nAccount, Message);//将string反序列化
				while(1)
                {
                    if(RMSG->MsgQueue_Wait())
                    {
                        RMSG->que.push(Message);
                        RMSG->MsgQueue_Close();
                        break;
                    }
                    else sleep(0.03);
                }
			    Message = "";
			    id = 0;
			    continue;
 			}
            if(id < 4)
			{
		        number = number*10+buffer[i]-'0';
			    id ++;
			}
		}
		sleep(0.03);	
	}
}
int main()
{
    RMSG = new MsgQueue();//带线程锁的接收消息队列
    SMSG = new MsgQueue();//带线程锁的发送消息队列
    RMSG->MsgQueue_Init();
    SMSG->MsgQueue_Init();
    redis = new CRedis();
    redis->Redis_Connect();
    sql = new CSql();
    sql->Sql_Connect();
    pthread_t tidp;
    if ((pthread_create(&tidp, NULL, Rpthread, NULL)) == -1)
	{
		printf("create error!\n");
	}
    while(1)
    {
        if(RMSG->MsgQueue_Wait())
        {
            while(!RMSG->que.empty())
            {
                string str = RMSG->que.front();
                RMSG->que.pop();
                Account nAccount;
                StringToProtobuf(nAccount, str);
                if(nAccount.move() == 1)//登录
                {
                    Player_check(nAccount);
                }
				else
				{
					Player_register(nAccount);
				}
            }
            RMSG->MsgQueue_Close();
        }
        sleep(0.03);
    }
}