#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <ctime>
#include <map>
#include <string.h>
#include <queue>
#include <unistd.h>
#include <sys/ipc.h>
#include <pthread.h>
#include <sys/shm.h>
#include "MsgQueue.h"
#include "SharedMemory.h"
#include "CSql.h"
#include "CRedis.h"
using namespace std;
MsgQueue* RMSG;//接收消息
MsgQueue* SMSG;//发送消息
CRedis* redis;
CSql* sql;
struct Player
{
    string UID;
    string name;
    string password;
    string flag;
    string fd;
};
bool Isexist(string str)
{
    string password = "";//用来存储查到的密码
    int ret = redis->Redis_Query(str, password);
    if(password == "")
    {
        return true;
    }
    else return false;
}
void Player_check(Player* player)
{
    int flag = 0;
    string password = "";//用来存储查到的密码
    int ret = redis->Redis_Query(player->UID, password);
    if(password == "")
    {
        flag = sql->Sql_Query(player->UID, player->password);
    }
    while(1)
    {
        if(SMSG->MsgQueue_Wait())
        {
            string str = "Login";
            SMSG->que.push(str);
            SMSG->que.push(player->fd);
            SMSG->que.push(player->UID);
            str = "";
            if(password == player->password || flag)
            {
                str = "YES";
            }
            else
            {
                str = "NO";
            }
            SMSG->que.push(str);
            SMSG->MsgQueue_Close();
            break;//信息发送完毕则退出
        }
        sleep(0.03);
    }
}
void Player_register(Player* player)
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
            if(SMSG->MsgQueue_Wait())
            {
                string str1 = "Register";
                SMSG->que.push(str1);
                SMSG->que.push(player->fd);//fd
                //cout<<str<<endl;
                SMSG->que.push(str);//账号
                str1 = "YES";
                SMSG->que.push(str1);
                redis->Redis_Write(str, player->password);
                sql->Sql_Write(str, player->name, player->password);
                SMSG->MsgQueue_Close();
                break;
            }
        }
        else sleep(0.03);
    }
}
static void * Spthread(void *arg)
{
    SharedMemory* SendMessage = new SharedMemory();
    SendMessage->SharedMemory_init(0x5003, 1024, 0640|IPC_CREAT, 0x5004, 1, 0640);
    while(1)
    {
        if(SMSG->MsgQueue_Wait())
        {
            if(SendMessage->SharedMemory_Wait())
            {
                string str = SendMessage->SharedMemory_Read();
                if(str == "OK")
                {
                    if(!SMSG->que.empty())
                    {
                        str = SMSG->que.front();
                        SMSG->que.pop();
                        SendMessage->SharedMemory_Write(str);
                    }
                }
                SendMessage->SharedMemory_Close();
            }
            SMSG->MsgQueue_Close();
        }
        sleep(0.03);
    }
}
static void * Rpthread(void *arg)
{
    SharedMemory* SendMessage = new SharedMemory();
    SendMessage->SharedMemory_init(0x5001, 1024, 0640|IPC_CREAT, 0x5002, 1, 0640);
    while(1)
    {
        if(RMSG->MsgQueue_Wait())
        {
            if(SendMessage->SharedMemory_Wait())
            {
                string str = SendMessage->SharedMemory_Read();
                if(str != "OK")
                {
                    RMSG->que.push(str);
                    str = "OK";
                    SendMessage->SharedMemory_Write(str);
                }
                SendMessage->SharedMemory_Close();
            }
            RMSG->MsgQueue_Close();
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
    Player* player = new Player();//玩家数据结构体
    redis = new CRedis();
    redis->Redis_Connect();
    sql = new CSql();
    sql->Sql_Connect();
    pthread_t tidp[2];
    if ((pthread_create(&tidp[0], NULL, Rpthread, NULL)) == -1)
	{
		printf("create error!\n");
	}
    if ((pthread_create(&tidp[1], NULL, Spthread, NULL)) == -1)
	{
		printf("create error!\n");
	}
    int index = 0;//用来记录目前消息是否全部接收
    while(1)
    {
        if(RMSG->MsgQueue_Wait())
        {
            while(!RMSG->que.empty())
            {
                string str = RMSG->que.front();
                index ++;
                RMSG->que.pop();
                if(index == 1)
                {
                    player->flag = str;
                }
                else if(index == 2)
                {
                    player->fd = str;
                }
                else if(index == 3)
                {
                    if(player->flag == "Login")
                    {
                        player->UID = str;
                    }
                    else
                    {
                        player->name = str;
                    }
                }
                else if(index == 4)
                {
                    player->password = str;
                    if(player->flag == "Login")
                    {
                        Player_check(player);
                    }
                    else Player_register(player);
                    index = 0;
                }
            }
            RMSG->MsgQueue_Close();
        }
        sleep(0.03);
    }
}