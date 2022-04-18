#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <map>
#include <string.h>
#include <queue>
#include <unistd.h>
#include <sys/time.h>
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
using namespace std;
const int MaxRoom = 1024;
int sockfd;
MsgQueue* RMSG;
map<string, bool> LoginMap;//确认是否完成登录
map<int, string> IdMap;//通过fd获取账号
map<int, int> RoomIdMap;//通过fd获取房间号
map<int, bool> RoomMap;//记录房间内所有人的消息是否收到
map<int, long long> LastTime;//房间上次发消息的时间
set<int> GameStartRoom;//目前已经开始的房间
set<int> HallPlayer;//大厅的玩家

map<int, int> LengthMap;//通过fd获取剩余需要读取的消息长度
map<int, string> MessageMap;//通过fd得到缓存的消息
map<int, int> indexMap;//通过fd得知目前消息头读到了第几位

vector< vector<int> > RoomMember;//记录每个房间包含的fd
vector< vector<Account> > RoomMessage;//每个房间内的消息
queue<int> RoomNumber;//可分配房间号

void init()
{
    RMSG = new MsgQueue();
    for(int i = 1; i <= MaxRoom; i ++)
    {
        RoomNumber.push(i);
    }
}
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
string GetRoomString()//将目前有的房间和房间人数存在string里面
{
	string str = "";
	map<int,int> RoomFlag;
	queue<int> que = RoomNumber;
	while(!que.empty())
	{
		RoomFlag[que.front()] = 1;//目前未使用房间
		que.pop();
	}
	for(int i = 1; i <= MaxRoom; i ++)
	{
		if(RoomFlag[i] != 1)
		{
			str += IntToString(i);//房间号
			str += '-';
			str += IntToString(RoomMember[i].size());//房间人数
		}
	}
	return str;
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
void SendToHallPlayer(Account& nAccount)
{
	set<int>::iterator it;
    for(it = HallPlayer.begin() ;it != HallPlayer.end(); it ++ )
    {
		int fd = *it;
		nAccount.set_fd(fd);
		SendToGateServer(nAccount);
	}
}
void RoomMemberRemove(Account nAccount)
{
	vector<int> NewRoom;
	for(int i = 0; i <= RoomMember[nAccount.roomid()].size(); i ++)
	{
		if(RoomMember[nAccount.roomid()][i] != nAccount.fd())
		{
			NewRoom.push_back(RoomMember[nAccount.roomid()][i]);
		}
	}
	RoomMember[nAccount.roomid()].clear();
	RoomMember[nAccount.roomid()] = NewRoom;
}
long long Gettime()
{
    struct timeval tv;
	gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}
void do_SendMessage()//给房间所有人发送消息
{
    set<int>::iterator it;
    for(it = GameStartRoom.begin() ;it != GameStartRoom.end(); it ++ )
    {
        int Roomid = *it;
        bool flag = true;
        for(int i = 0; i <= 3; i ++ )
        {
            if(RoomMap[RoomMember[Roomid][i]] == false)//该消息未到
            {
                flag = false;
            }
        }
        if(!flag)//没收到所有消息
        {
            continue;
        }
        long long nowtime = Gettime();
        if(nowtime - LastTime[Roomid] >= 34)
        {
            for(int i = 0; i <= 3; i ++ )
            {
                for(int j = 0; j <= RoomMessage[Roomid].size(); j ++)
                {
                    SendToGateServer(RoomMessage[Roomid][j]);
                }
                RoomMap[RoomMember[Roomid][i]] = false;
            }
            LastTime[Roomid] = nowtime;
        }
    }
}
static void * Rpthread(void *arg)//接收来自GateServce的消息
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
    init();
    pthread_t tidp;
    if ((pthread_create(&tidp, NULL, Rpthread, NULL)) == -1)
	{
		printf("create 1 error!\n");
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
				if(nAccount.move() == 6 && RoomMember[nAccount.roomid()].size() == 4)//同步消息
                {
                    //房间人数到齐且为操作消息
                    RoomMap[nAccount.fd()] = true;//记录该消息已收到
                    RoomMessage[RoomIdMap[nAccount.fd()]].push_back(nAccount);
                }
				else if(nAccount.move() == 1)//登陆成功，将当前所有的房间发回去
				{
					string str = GetRoomString();//将所有现存房间以及人数发回客户端
					nAccount.set_message(str);
					HallPlayer.insert(nAccount.fd());//将玩家插入大厅set
					SendToHallPlayer(nAccount);
				}
                else if(nAccount.move() == 3)//请求分配房间
                {
                    nAccount.set_roomid(RoomNumber.front());//分配房间
                    RoomIdMap[nAccount.fd()] = nAccount.roomid();
                    RoomNumber.pop();
					HallPlayer.erase(nAccount.fd());//将玩家移出大厅
                    RoomMember[nAccount.roomid()].push_back(nAccount.fd());
                    SendToHallPlayer(nAccount);
                }
				else if(nAccount.move() == 4)//用户加入房间
				{
				 	RoomMember[nAccount.roomid()].push_back(nAccount.fd());
                    RoomIdMap[nAccount.fd()] = nAccount.roomid();
					HallPlayer.erase(nAccount.fd());
                    SendToHallPlayer(nAccount);
                    if(RoomMember[nAccount.roomid()].size() == 4)//将人数到齐的房间存起来
                	{
                        GameStartRoom.insert(nAccount.roomid());
                    }
				}
				else if(nAccount.move() == 5)//用户退出所在房间
				{
					int fd = nAccount.fd();
					string str = nAccount.roomid();
					str = IntToString(str);
					nAccount.set_message(str);//
					SendToHallPlayer(nAccount);//将它退出的房间号发给所有大厅的人
					RoomMemberRemove(nAccount);//将该人从房间移除
					nAccount.set_fd(fd);
					str = GetRoomString();
					nAccount.set_message(str);
					SendToGateServer(nAccount);//将当前房间信息发送给退出的人
					RoomIdMap[nAccount.fd()] = 0;//将fd在的房间号置为0
					HallPlayer.insert(nAccount.fd());//单独发送
				}
            }
            RMSG->MsgQueue_Close();
        }
        sleep(0.002);//休眠两毫秒
		do_SendMessage();
    }
}
