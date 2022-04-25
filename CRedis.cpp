#include "CRedis.h"

CRedis::CRedis()
{
    Redis_Connect();
}

CRedis::~CRedis()
{
    freeReplyObject(pm_rr);
    if(pm_rct != NULL)
        redisFree(pm_rct);
}

void CRedis::Redis_Connect()
{
    pm_rct = redisConnect(addr, port);
    if (pm_rct == NULL || pm_rct->err) 
    {
        if (pm_rct) 
        {
            printf("Error: %s\n", pm_rct->errstr);
        } 
        else 
        {
            printf("Can't allocate redis context\n");
        }
    }
}

int CRedis::Redis_Write(string& key, string& value)
{
    string cmd = "set " + key + " " + value;
	pm_rr = (redisReply*)redisCommand(pm_rct, cmd.c_str());
    return handleReply();
}

int CRedis::Redis_Query(string& key, string& value)
{
    string cmd = "get " + key;
    pm_rr = (redisReply*)redisCommand(pm_rct, cmd.c_str());
    int ret = handleReply(&value);
}

int CRedis::Redis_Delete(string& key)
{
    string cmd = "del " + key;
    pm_rr = (redisReply*)redisCommand(pm_rct, cmd.c_str());
    int rows = 0;
    int ret = handleReply(&rows);
    if (ret == M_REDIS_OK)
        return rows;
    else
        return ret;
}

int CRedis::Redis_Updata(string& key, string& value, string& newvalue)
{
    string str ="";//用以储存查到的账号
    int ret = Redis_Query(key, str);
    if(ret != M_REDIS_OK)
    {
        printf("Not Find\n");
        return -1;
    }
    else
    {
        if(str == value)
        {
            Redis_Delete(key);
            Redis_Write(key, newvalue);
            return 0;
        }
        else
        {
            return -2;
        }
    }
}

string CRedis::getErrorMsg()
{
    return error_msg;
}

int CRedis::handleReply(void* value, redisReply*** array)
{
    if (pm_rct->err)
    {
        error_msg = pm_rct->errstr;
        return M_CONTEXT_ERROR;
    }

    if (pm_rr == NULL)
    {
        error_msg = "auth redisReply is NULL";
        return M_REPLY_ERROR;
    }

    switch (pm_rr->type)
    {
        case REDIS_REPLY_ERROR:
            error_msg = pm_rr->str;
            return M_EXE_COMMAND_ERROR;
        case REDIS_REPLY_STATUS:
            if (!strcmp(pm_rr->str, "OK"))
            {
                return M_REDIS_OK;
            }
            else
            {
                error_msg = pm_rr->str;
                return M_EXE_COMMAND_ERROR;
            }
        case REDIS_REPLY_INTEGER://返回是一个整数
            *(int*)value = pm_rr->integer;
            return M_REDIS_OK;
        case REDIS_REPLY_STRING://返回是一个string
            *(string*)value = pm_rr->str;
            return M_REDIS_OK;
        case REDIS_REPLY_NIL://要返回的数据不存在
            *(string*)value = "";
            return M_REDIS_OK;
        case REDIS_REPLY_ARRAY://返回的是一个数组
            *(int*)value = pm_rr->elements;
            *array = pm_rr->element;
            return M_REDIS_OK;
        default:
            error_msg = "unknow reply type";
            return M_EXE_COMMAND_ERROR;
    }
}
