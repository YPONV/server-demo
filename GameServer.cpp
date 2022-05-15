#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <vector>
#include <map>
#include <string.h>
#include <queue>
#include <unistd.h>
#include <sys/time.h>
#include <netinet/tcp.h>
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
const int MaxPlayer = 2;
int sockfd;
MsgQueue* RMSG;
map<int, bool> RoomMap;//记录房间内所有人的消息是否收到
map<int, long long> LastTime;//房间上次发消息的时间
map<int, string> FDGetID;//通过FD获取ID
map<string, int> IDGetRoom;//通过ID获得房间号
map<string, int> IDGetFD;//通过ID获得FD
map<string, int> IDGetLevel;
set<int> GameStartRoom;//目前已经开始的房间
set<int> HallPlayer;//大厅的玩家
vector<int> Roomlevel[MaxRoom];

map<int, int> LengthMap;//通过fd获取剩余需要读取的消息长度
map<int, string> MessageMap;//通过fd得到缓存的消息
map<int, int> indexMap;//通过fd得知目前消息头读到了第几位

vector< vector<int> > RoomMember(MaxRoom);//记录每个房间包含的fd
vector< vector<Account> > RoomMessage(MaxRoom);//每个房间内的消息
vector< vector<string> > RoomAllMessage(MaxRoom);
queue<int> RoomNumber;//可分配房间号

void init()
{
	RMSG = new MsgQueue();
	for (int i = 1; i <= MaxRoom; i++)
	{
		RoomNumber.push(i);
	}
}
string IntToString(int num)
{
	string str = "";
	if (num == 0)
	{
		str += "0";
		return str;
	}
	while (num)
	{
		str += num % 10 + '0';
		num /= 10;
	}
	reverse(str.begin(), str.end());
	return str;
}
int StringToInt(string str)
{
	int number = 0;
	for (int i = 0; i < str.size(); i++)
	{
		number = number * 10 + str[i] - '0';
	}
	return number;
}
void StringToProtobuf(Account& nAccount, string& Message)
{
	char p[1024];
	memset(p, '\0', sizeof p);
	for (int j = 0; j < Message.size(); j++) {
		p[j] = Message[j];
	}
	nAccount.ParseFromArray(p, Message.size());
}
long long Gettime()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}
string GetRoomString()//将目前有的房间和房间人数存在string里面
{
	string str = "";
	map<int, int> RoomFlag;
	queue<int> que = RoomNumber;
	while (!que.empty())
	{
		RoomFlag[que.front()] = 1;//目前未使用房间
		que.pop();
	}
	for (int i = 1; i <= MaxRoom; i++)
	{
		if (RoomFlag[i] != 1)
		{
			str += IntToString(i);//房间号
			str += '-';
			str += IntToString(RoomMember[i].size());//房间人数
			str += '-';
		}
	}
	return str;
}
string GetRoomState(int RID)//将房间状态发送给房间内人物
{
	string str = "", str1 = "00000";
	for (int i = 0; i < 4; i++)
	{
		if (i < RoomMember[RID].size() && RoomMap[RoomMember[RID][i]])
			str += '1';
		else str += '0';
	}
	for (int i = 0; i < Roomlevel[RID].size(); i++)
	{
		int Level = Roomlevel[RID][i];
		str1[Level - 1] ++;
	}
	str += str1;
	return str;
}
void CancelChose(Account& nAccount)
{
	int Level = IDGetLevel[nAccount.uid()];
	int roomid = nAccount.roomid();
	auto it = Roomlevel[roomid].begin();
	for (it; it != Roomlevel[roomid].end(); it++)
	{
		if (*it == Level)
		{
			Roomlevel[roomid].erase(it);
			cout << Roomlevel[roomid].size() << endl;
			return;
		}
	}
}
void AddPack(char* Newdata, char* data, int len)//给消息包加头
{
	int len1 = len;
	for (int i = 3; i >= 0; i--)
	{
		if (len1 > 0)
		{
			Newdata[i] = len1 % 10 + '0';
			len1 /= 10;
		}
		else Newdata[i] = '0';
	}
	for (int i = 0; i < len; i++)
	{
		Newdata[i + 4] = data[i];
	}
}
int SendToGateServer(Account& nAccount)//flag记录是什么事件
{
	char p[1024], pp[1024];
	memset(p, '\0', sizeof p);
	memset(pp, '\0', sizeof pp);
	int sz = nAccount.ByteSize();
	nAccount.SerializeToArray(p, sz);
	AddPack(pp, p, sz);
	char* ptr = pp;
	sz += 4;
	long long T = Gettime();
	while (sz > 0)
	{
		long long now = Gettime();
		if (now - T > 2)
		{
			return -1;
		}
		int written_bytes = send(sockfd, ptr, sz, 0);
		if (written_bytes < 0)
		{
			printf("SendMessage error!\n");
		}
		sz -= written_bytes;
		ptr += written_bytes;
	}
}
void SendAllRoomMessage(Account nAccount)
{
	int roomid = nAccount.roomid();
	for (int i = 0; i < RoomAllMessage[roomid].size(); i++)
	{
		nAccount.set_message(RoomAllMessage[roomid][i]);
		SendToGateServer(nAccount);
	}
}
void SendToHallPlayer(Account nAccount)
{
	set<int>::iterator it;
	for (it = HallPlayer.begin(); it != HallPlayer.end(); it++)
	{
		int fd = *it;
		nAccount.set_fd(fd);//需要发给的fd
		SendToGateServer(nAccount);
	}
}
void SendToRoomPlayer(Account nAccount)
{
	int roomid = nAccount.roomid();
	for (int i = 0; i < RoomMember[roomid].size(); i++)
	{
		nAccount.set_fd(RoomMember[roomid][i]);
		SendToGateServer(nAccount);
	}
}
void RoomMemberRemove(Account& nAccount)
{
	vector<int> NewRoom;
	for (int i = 0; i < RoomMember[nAccount.roomid()].size(); i++)
	{
		if (RoomMember[nAccount.roomid()][i] != nAccount.fd())
		{
			NewRoom.push_back(RoomMember[nAccount.roomid()][i]);
		}
	}
	RoomMember[nAccount.roomid()].clear();
	RoomMember[nAccount.roomid()] = NewRoom;
}
void do_SendMessage()//给房间所有人发送消息
{
	set<int>::iterator it;
	for (it = GameStartRoom.begin(); it != GameStartRoom.end(); it++)
	{
		int Roomid = *it;
		bool flag = true;
		if (RoomMember[Roomid].size() != MaxPlayer)continue;
		for (int i = 0; i < RoomMember[Roomid].size(); i++)
		{
			if (RoomMap[RoomMember[Roomid][i]] == false)//该消息未到
			{
				flag = false;
				break;
			}
		}
		if (!flag)//没收到所有消息
		{
			continue;
		}
		long long nowtime = Gettime();
		if (nowtime - LastTime[Roomid] >= 34)
		{
			string str = "";
			Account nAccount;
			nAccount.set_move(9);
			for (int j = 0; j < RoomMessage[Roomid].size(); j++)
			{
				str += RoomMessage[Roomid][j].message();
			}
			RoomAllMessage[Roomid].push_back(str);//将同步消息存起来
			RoomMessage[Roomid].clear();
			nAccount.set_message(str);
			for (int i = 0; i < RoomMember[Roomid].size(); i++)
			{
				RoomMap[RoomMember[Roomid][i]] = false;
			}
			LastTime[Roomid] = nowtime;
			for (int i = 0; i < RoomMember[Roomid].size(); i++)
			{
				nAccount.set_fd(RoomMember[Roomid][i]);
				SendToGateServer(nAccount);
			}
		}
	}
}
// static void * Rpthread(void *arg)//接收来自GateServce的消息
// {
//     string Message = "";
// 	int flag = 0, number = 0, id = 0;
// 	if( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
// 	{
// 		perror("socket");
// 	}
// 	struct hostent* h;
// 	if( (h = gethostbyname("10.0.128.212")) == 0)
// 	{
// 		printf("gethostbyname failed,\n");
// 		close(sockfd);
// 	}
// 	struct sockaddr_in servaddr;
// 	memset(&servaddr, 0, sizeof(servaddr));
// 	servaddr.sin_family = AF_INET;
// 	servaddr.sin_port = htons(atoi("1111"));
// 	memcpy(&servaddr.sin_addr, h->h_addr, h->h_length);
// 	if(connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) != 0)
// 	{
// 		perror("connect");
// 		close(sockfd);
// 	}
// 	char buffer[1024];
// 	while(1)
// 	{
// 		int iret;
// 		memset(buffer, 0, sizeof(buffer));
// 		if( (iret = recv(sockfd, buffer, sizeof(buffer), 0)) <= 0)
// 		{
// 			printf("iret=%d\n",iret);
// 			break;
// 		}
// 		for(int i = 0; i < iret; i ++ )
// 		{
// 			if(number > 0 && id == 4)
// 			{
// 			    Message += buffer[i];
// 			    number --;
// 			}
// 			if(number == 0 && id == 4)
// 			{
// 				// Account nAccount;
// 				// StringToProtobuf(nAccount, Message);//将string反序列化

//                 RMSG->que.push(Message);
//                 RMSG->MsgQueue_Close();

// 			    Message = "";
// 			    id = 0;
// 			    continue;
//  			}
//             if(id < 4)
// 			{
// 		        number = number*10+buffer[i]-'0';
// 			    id ++;
// 			}
// 		}
// 		sleep(0.0001);	
// 	}
// }
int main()
{
	init();
	// pthread_t tidp;
	// if ((pthread_create(&tidp, NULL, Rpthread, NULL)) == -1)
	// {
	// 	printf("create 1 error!\n");
	// }
	string Message = "";
	int number = 0, id = 0;
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		perror("socket");
	}
	struct hostent* h;
	if ((h = gethostbyname("10.0.128.208")) == 0)
	{
		printf("gethostbyname failed,\n");
		close(sockfd);
	}
	struct sockaddr_in servaddr;
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(atoi("1111"));
	memcpy(&servaddr.sin_addr, h->h_addr, h->h_length);
	if (connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) != 0)
	{
		perror("connect");
		close(sockfd);
	}
	char buffer[1024];
	while (1)
	{
		int iret;
		memset(buffer, 0, sizeof(buffer));
		if ((iret = recv(sockfd, buffer, sizeof(buffer), MSG_DONTWAIT)) == 0)
		{
			printf("iret=%d\n", iret);
			break;
		}
		int a[1] = { 1 };
		setsockopt(sockfd, IPPROTO_TCP, TCP_QUICKACK, a, sizeof(int));
		for (int i = 0; i < iret; i++)
		{
			if (number > 0 && id == 4)
			{
				Message += buffer[i];
				number--;
			}
			if (number == 0 && id == 4)
			{
				Account nAccount;
				StringToProtobuf(nAccount, Message);
				if (nAccount.move() == 9)//同步消息
				{
					//房间人数到齐且为操作消息
					//long long nowtime = Gettime();
					//cout<<nAccount.fd()<<" "<<nowtime<<endl;
					RoomMap[nAccount.fd()] = true;//记录该消息已收到
					RoomMessage[nAccount.roomid()].push_back(nAccount);
				}
				else if (nAccount.move() == 1)//登陆成功，将当前所有的房间发回去
				{
					string UID = nAccount.uid();
					int fd = nAccount.fd();
					if (IDGetRoom[UID] != 0)//非正常退出,将房间信息发过去
					{
						nAccount.set_move(10);//追帧消息
						nAccount.set_roomid(IDGetRoom[UID]);
						SendAllRoomMessage(nAccount);
						FDGetID[fd] = UID;
						IDGetFD[UID] = fd;
					}
					else
					{
						string str = GetRoomString();//将所有现存房间以及人数发回客户端
						nAccount.set_message(str);
						HallPlayer.insert(nAccount.fd());//将玩家插入大厅set
						SendToGateServer(nAccount);
						FDGetID[fd] = UID;
						IDGetFD[UID] = fd;
					}
				}
				else if (nAccount.move() == 3)//请求分配房间
				{
					string UID = nAccount.uid();
					nAccount.set_roomid(RoomNumber.front());//分配房间
					RoomNumber.pop();
					nAccount.set_id(1);
					RoomMember[nAccount.roomid()].push_back(nAccount.fd());
					string str = GetRoomString();//将所有现存房间以及人数发回客户端
					nAccount.set_message(str);
					HallPlayer.erase(nAccount.fd());//从大厅set中删除该角色fd
					SendToHallPlayer(nAccount);
					IDGetRoom[UID] = nAccount.roomid();
					str = GetRoomState(nAccount.roomid());
					nAccount.set_message(str);
					SendToRoomPlayer(nAccount);
				}
				else if (nAccount.move() == 4)//用户加入房间
				{
					//cout<<nAccount.roomid()<<endl;
					string UID = nAccount.uid();
					RoomMember[nAccount.roomid()].push_back(nAccount.fd());
					int id = RoomMember[nAccount.roomid()].size();
					nAccount.set_id(id);
					string str = GetRoomString();//将所有现存房间以及人数发回客户端
					nAccount.set_message(str);
					HallPlayer.erase(nAccount.fd());
					SendToHallPlayer(nAccount);
					str = GetRoomState(nAccount.roomid());
					nAccount.set_message(str);
					SendToRoomPlayer(nAccount);
					IDGetRoom[UID] = nAccount.roomid();
					//HallPlayer.erase(nAccount.fd());//从大厅set中删除该角色fd
				}
				else if (nAccount.move() == 5)//用户退出所在房间
				{
					int Roomid = nAccount.roomid();
					string UID = nAccount.uid();
					RoomMemberRemove(nAccount);//将该人从房间移除
					if (RoomMember[nAccount.roomid()].size() == 0)//该房间没人了
					{
						RoomNumber.push(nAccount.roomid());
					}
					HallPlayer.insert(nAccount.fd());//将fd插入大厅set
					string str = GetRoomString();//将所有现存房间以及人数发回客户端
					nAccount.set_message(str);
					SendToHallPlayer(nAccount);//将它退出后的房间状态发给所有大厅的人
					
					CancelChose(nAccount);
					IDGetLevel[UID] = 0;
					str = GetRoomState(nAccount.roomid());
					nAccount.set_message(str);
					int f = RoomMember[nAccount.roomid()].size();
					nAccount.set_id(f);
					SendToRoomPlayer(nAccount);//将当前房间信息发送给退出房间的人
					RoomMap[nAccount.fd()] = false;//准备状态改为0
					IDGetRoom[UID] = 0;
				}
				else if (nAccount.move() == 6)//用户准备
				{
					RoomMap[nAccount.fd()] = true;
					if (RoomMember[nAccount.roomid()].size() == MaxPlayer)
					{
						bool flag = true;
						for (int i = 0; i < RoomMember[nAccount.roomid()].size(); i++)
						{
							if (RoomMap[RoomMember[nAccount.roomid()][i]] != true)
							{
								flag = false;
							}
						}
						if (flag == true)
						{
							GameStartRoom.insert(nAccount.roomid());//将房间号加入开始游戏的房间
							string str = "S0";
							char f = '0' + MaxPlayer;
							str += f;
							for (int i = 1; i <= 7; i++)
							{
								int random = rand() % 10;
								str += random + '0';
							}
							for (int i = 0; i < RoomMember[nAccount.roomid()].size(); i++)
							{
								RoomMap[RoomMember[nAccount.roomid()][i]] = false;
								string UID = FDGetID[RoomMember[nAccount.roomid()][i]];
								IDGetRoom[UID] = 0;
							}
							for (int i = 0; i < RoomMember[nAccount.roomid()].size(); i++)
							{
								//发送开始游戏的消息
								nAccount.set_move(9);
								str[1] = ('0' + i + 1);
								nAccount.set_message(str);
								nAccount.set_fd(RoomMember[nAccount.roomid()][i]);
								SendToGateServer(nAccount);
							}
							LastTime[nAccount.roomid()] = Gettime();
							Roomlevel[nAccount.roomid()].clear();
						}
						else
						{
							string str = GetRoomState(nAccount.roomid());
							int roomid = nAccount.roomid();
							nAccount.set_id(RoomMember[roomid].size());
							nAccount.set_message(str);
							SendToRoomPlayer(nAccount);
						}
					}
					else
					{
						string str = GetRoomState(nAccount.roomid());
						int roomid = nAccount.roomid();
						nAccount.set_id(RoomMember[roomid].size());
						nAccount.set_message(str);
						SendToRoomPlayer(nAccount);
					}
				}
				else if (nAccount.move() == 7)//取消准备
				{
					if (GameStartRoom.count(nAccount.roomid()) == 0)//可能因为网络延迟游戏已经开始，没开始才置为0
					{
						RoomMap[nAccount.fd()] = false;
						string str = GetRoomState(nAccount.roomid());
						nAccount.set_message(str);
						SendToRoomPlayer(nAccount);
					}
				}
				else if (nAccount.move() == 8)//游戏结束
				{
					int roomid = nAccount.roomid();

				}
				else if (nAccount.move() == 20)
				{
					string UID = nAccount.uid();
					if (IDGetLevel[UID] != 0)
					{
						CancelChose(nAccount);
					}
					string Mess = nAccount.message();
					int Level = Mess[0] - '0';
					IDGetLevel[UID] = Level;
					int roomid = nAccount.roomid();
					Roomlevel[roomid].push_back(Level);
					string str = GetRoomState(roomid);
					cout << str << endl;
					nAccount.set_message(str);
					SendToRoomPlayer(nAccount);
				}
				else if (nAccount.move() == 404)
				{
					int fd = nAccount.fd();
					string UID = FDGetID[fd];
					if (HallPlayer.find(fd) != HallPlayer.end())//在大厅里
					{
						HallPlayer.erase(fd);
						IDGetFD[UID] = 0;
						FDGetID[fd] = "";
					}
					else if (IDGetRoom[UID] != 0)//在房间里
					{
						int Roomid = IDGetRoom[UID];
						nAccount.set_move(5);
						nAccount.set_roomid(Roomid);
						RoomMemberRemove(nAccount);//将该人从房间移除
						if (RoomMember[Roomid].size() == 0)//该房间没人了
						{
							Roomlevel[nAccount.roomid()].clear();
							RoomNumber.push(Roomid);
						}
						string str = GetRoomString();//将所有现存房间以及人数发回客户端
						nAccount.set_message(str);
						SendToHallPlayer(nAccount);//将它退出后的房间状态发给所有大厅的人
						str = GetRoomState(Roomid);
						nAccount.set_message(str);
						int f = RoomMember[Roomid].size();
						nAccount.set_id(f);
						SendToRoomPlayer(nAccount);//将当前房间信息发送给房间的人
						RoomMap[nAccount.fd()] = false;//准备状态改为0
						IDGetLevel[nAccount.uid()] = 0;
						IDGetFD[UID] = 0;
						IDGetRoom[UID] = 0;
						FDGetID[fd] = "";
					}
					else if (IDGetRoom[UID] != 0)//在游戏中
					{
						IDGetFD[UID] = 0;
						FDGetID[fd] = "";
					}
					else//正常退出游戏
					{
					}
				}
				Message = "";
				id = 0;
				continue;
			}
			if (id < 4)
			{
				number = number * 10 + buffer[i] - '0';
				id++;
			}
		}
		do_SendMessage();
		usleep(1000);
	}
	close(sockfd);
}
