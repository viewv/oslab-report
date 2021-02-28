#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/kernel.h>
#include <fcntl.h>
#include <linux/types.h>
#include <sys/stat.h>
#include <sys/shm.h>

#define BUFFERSIZE    10

struct structBuffer
{
    int int_count;
    int head;
    int tail;
    int data[BUFFERSIZE];
};

sem_t *mutex; //互斥锁

int main()
{
    int get_count=0;

    char errInfo[255];
    char mutexSem[]= "mutex";
    int itemValue = -1;
    int mainshmid;
    key_t key;    
    
    struct structBuffer * buffer;
    
    key = ftok("./tmp.txt",0x03);
    mainshmid=shmget(key,sizeof(struct structBuffer),IPC_CREAT|0666);
    
    if(mainshmid == -1)
    {
        printf("shmget error!\n");
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
    while(get_count<500)
    {
        while(buffer->int_count<=0)
            ;
        if(sem_wait(mutex)!=0)
        {
            printf("in customer %u,sem_post(empty) error!",getpid());
            perror(errInfo);
            break;
        }

        itemValue=buffer->data[buffer->tail];
        buffer->int_count--;
        (buffer->tail)++;

        if(buffer->tail>=BUFFERSIZE)
        {
            buffer->tail=0;
        }
        printf("customer: %u:%d\n",getpid(),itemValue);
        get_count++;
        if(sem_post(mutex)!=0)
        {
            printf("in customer %u,sem_post(empty) error!\n",getpid());
            perror(errInfo);
            break;
        }
    }

    //取消共享内存
    if(shmdt(buffer)!=0)
    {
        printf("in customer shmdt(buffer) error!\n");
        perror(errInfo);
    }
    
    //删除共享内存
    if(shmctl(mainshmid,IPC_RMID,0)==-1)
    {
        printf("in customer shmctl(shmid，IPC_RMID，0) error!\n");
        perror(errInfo);
    }
    //取消互斥锁
    sem_unlink("mutex");
    return 0;
}