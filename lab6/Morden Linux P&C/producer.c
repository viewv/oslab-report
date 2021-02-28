#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/kernel.h>
#include <fcntl.h>
#include <linux/types.h>
#include <sys/stat.h>
#include <sys/shm.h>
#include <sys/types.h>

#define BUFFERSIZE    10

struct structBuffer
{
    int int_count;
    int head;
    int tail;
    int data[BUFFERSIZE];
};

sem_t *mutex;//互斥锁

int main()
{
    char errInfo[255];
    char mutexSem[]= "mutex";
    int itemValue = -1;
    key_t key;
    
    
    struct structBuffer * buffer;
    
    int mainshmid;
    key = ftok("./tmp.txt",0x03);
    mainshmid=shmget(key,sizeof(struct structBuffer),IPC_CREAT|0666);
    
    if(mainshmid == -1)
    {
        printf("shmget Error!\n");
        perror(errInfo);
        return -1;
    }
    
    mutex = sem_open(mutexSem,O_CREAT,0644,1);
    if(mutex == SEM_FAILED)
    {
        printf("create semaphore mutex error!\n");
        return 1;
    }

    buffer=(struct structBuffer *)shmat(mainshmid,NULL,0);
    if((long)buffer==-1)
    {
        printf("in producer shmat(mainshmid,NULL,0) error!\n");
        perror(errInfo);
        exit(-1);
    }

    while(itemValue<499)
    {
        itemValue++;
        printf("producer(%u) int_count=>%d  itemValue=> %d\n",getpid(),buffer->int_count,itemValue);
        while(buffer->int_count==10)
            ;
        if(sem_wait(mutex)!=0)
        {
            printf("in producer sem_wait(mutex) error!\n");
            perror(errInfo);
            break;
        }
        
        buffer->int_count+=1;
        buffer->data[buffer->head]=itemValue;
        (buffer->head)++;
        if(buffer->head>=BUFFERSIZE)
        {
            buffer->head=0;
        }
        
        if(sem_post(mutex)!=0)
        {
            printf("in producer sem_post(mutex) error!\n");
            perror(errInfo);
            break;
        }
    }

    if(shmdt(buffer)!=0)
    {
        printf("in producer shmdt(buffer) error!\n");
        perror(errInfo);
    }
    return 0;
}