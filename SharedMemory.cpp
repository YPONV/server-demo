#include "SharedMemory.h"

SharedMemory::SharedMemory()
{
}

SharedMemory::~SharedMemory()
{
    if (shmctl(shmid, IPC_RMID, 0) == -1)//删除共享内存
    { 
        printf("shmctl(0x5005) failed\n"); 
    }
    if (semctl(sem_id,0,IPC_RMID) == -1) 
    { 
        printf("destroy semctl()");
    }
}

int SharedMemory::SharedMemory_init(int key1, int size, int shmflg, int key2, int nsems, int semflg)
{
    if( (shmid = shmget((key_t)key1, size, 0640|IPC_CREAT)) == -1)//初始化共享内存
    { 
        printf("shmat(0x5005) failed\n"); 
        return -1; 
    }
    if ( (sem_id=semget(key2, nsems, semflg)) == -1)
    {
        // 如果信号灯不存在，创建它。
        if (errno==2)
        {
            if( (sem_id=semget(key1,1,0640|IPC_CREAT)) == -1)
            { 
                printf("init 1 semget()"); 
                return -1; 
            }
            // 信号灯创建成功后，还需要把它初始化成可用的状态。
            union semun sem_union;
            sem_union.val = 1;
            if (semctl(sem_id,0,SETVAL,sem_union) <  0) 
            { 
                printf("init semctl()"); 
                return false; 
            }
        }
    }
    string Start="OK";
    if(SharedMemory_Wait())
    {
        SharedMemory_Write(Start);
        SharedMemory_Close();
    }
    return true;
}

bool SharedMemory::SharedMemory_Wait()
{
    struct sembuf sem_b;
    sem_b.sem_num = 0;
    sem_b.sem_op = -1;
    sem_b.sem_flg = SEM_UNDO;
    if (semop(sem_id, &sem_b, 1) == -1)//此处会阻塞等待信号灯亮起
    {
        return false;
    }
    return true;
}

bool SharedMemory::SharedMemory_Write(string& Message)
{
    ptext = (char *)shmat(shmid, 0, 0);
    if(ptext == NULL)
    {
        printf("Write error\n");
        return false;
    }
    sprintf(ptext,"%s",Message.c_str());
    return true;
}

string SharedMemory::SharedMemory_Read()
{
    ptext = (char *)shmat(shmid, 0, 0);
    if(ptext == NULL)
    {
        printf("Read error\n");
    }
    int len=strlen(ptext);
    string str="";
    for(int i=0;i<len;i++)
    {
        str+=ptext[i];
    }
    return str;
}

bool SharedMemory::SharedMemory_Close()
{
    shmdt(ptext);//共享内存从当前进程中分离
    struct sembuf sem_b;
    sem_b.sem_num = 0;
    sem_b.sem_op = 1;  
    sem_b.sem_flg = SEM_UNDO;
    if (semop(sem_id, &sem_b, 1) == -1) 
    { 
        printf("post semop()"); 
        return false; 
    }
    return true;
}
