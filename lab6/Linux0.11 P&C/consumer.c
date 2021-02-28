#define   __LIBRARY__  
#include <unistd.h>  
#include <stdlib.h>  
#include <stdio.h>  
#include <sys/types.h>  

#define BUFFER_SIZE 10 
#define N 500
#define KEY 10

_syscall2(int,sem_open,const char*,name,unsigned int,value);
_syscall1(int,sem_wait,sem_t*,sem);
_syscall1(int,sem_post,sem_t*,sem);
_syscall1(int,sem_unlink,const char*,name);
_syscall2(int,shmget,key_t,key,size_t,size);
_syscall1(void*,shmat,int,shmid); 

int main(){
	int shmid,i;
	int* laddr;
    
	sem_t *empty, *full, *mutex;  
	empty = (sem_t *)sem_open("empty",BUFFER_SIZE);  
    full  = (sem_t *)sem_open("full", 0);  
    mutex = (sem_t *)sem_open("mutex", 1);  
    shmid = shmget(KEY,(BUFFER_SIZE + 1) * sizeof(int));
      
    laddr = (int*)shmat(shmid);  

    for( i = 0 ; i < N; i++){  
        sem_wait(full);  
        sem_wait(mutex);  
        printf("%d\n",laddr[i%BUFFER_SIZE]);  
        fflush(stdout);  
        sem_post(mutex);  
        sem_post(empty);  
    }  
  
    sem_unlink("empty");  
    sem_unlink("full");  
    sem_unlink("mutex");  
    return 0;  
}
