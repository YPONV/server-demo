#include <iostream>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <string.h>
using namespace std;
class SharedMemory
{
public:
    SharedMemory();
    ~SharedMemory();
    union semun  // 用于信号灯操作的共同体。
    {
        int val;
        struct semid_ds *buf;
        unsigned short *arry;
    };
    int SharedMemory_init(int key1, int size, int shmflg, int key2, int nsems, int semflg);
    bool SharedMemory_Wait();
    bool SharedMemory_Write(string& Message);
    string SharedMemory_Read();
    bool SharedMemory_Close();
    int shmid; // 共享内存标识符
    char *ptext=NULL;//指向共享内存地址
    int  sem_id;  // 信号灯描述符

};
