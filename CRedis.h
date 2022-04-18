#include <iostream>
#include <stdio.h>
#include <string.h>
#include <hiredis/hiredis.h>
using namespace std;

class CRedis
{
public:
    CRedis();
    ~CRedis();

enum
{
    M_REDIS_OK = 0, //执行成功
    M_CONNECT_FAIL = -1, //连接redis失败
    M_CONTEXT_ERROR = -2, //RedisContext返回错误
    M_REPLY_ERROR = -3, //redisReply错误
    M_EXE_COMMAND_ERROR = -4 //redis命令执行错误
};

    redisContext* pm_rct; //redis结构体
    redisReply* pm_rr; //返回结构体
    string error_msg; //错误信息

    const char * addr="127.0.0.1";
	const int port=6379;

    void Redis_Connect();
    int Redis_Write(string& key, string& value);//成功为0，<0失败
    int Redis_Query(string& key, string& value);//成功为0，<0失败
    int Redis_Updata(string& key, string& value, string& newvalue);//-1表示没有这个账号，-2表示原密码错误，0表示修改成功
    int Redis_Delete(string& key);//返回成功删除的行数，可能为0
    string getErrorMsg();


    int handleReply(void* value = NULL, redisReply ***array = NULL);
};