#include "MsgQueue.h"

MsgQueue::MsgQueue()
{
    MsgQueue_Init();
}

MsgQueue::~MsgQueue()
{
    pthread_mutex_destroy(&mtx);
}

void MsgQueue::MsgQueue_Init()
{
    int ret = pthread_mutex_init(&mtx, NULL);
    if(ret != 0)
    {
        printf("Msg_init error\n");
    }
}
bool MsgQueue::MsgQueue_Wait()//尝试加锁,不会阻塞
{
    int ret = pthread_mutex_trylock(&mtx);
    if (0 == ret) // 加锁成功  
    {
        return true;
    } 
    else if(EBUSY == ret)// 锁正在被使用;   
    { 
        return false;
    }
}
bool MsgQueue::MsgQueue_Close()
{
    int ret = pthread_mutex_unlock(&mtx);
    if(ret == 0)
    {
        return true;
    }
    else return false;
}