#include <iostream>
#include <string.h>
#include <queue>
#include <pthread.h>
using namespace std;
class MsgQueue
{
public:
    MsgQueue();
    ~MsgQueue();
    queue<string>que;
    pthread_mutex_t mtx;

    void MsgQueue_Init();
    bool MsgQueue_Wait();
    bool MsgQueue_Close();
};