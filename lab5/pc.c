#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <linux/sem.h>

#define SIZE 10
#define M 1000

// 系统调用
_syscall2(int, sem_open, const char *, name, unsigned int, value)
_syscall1(int, sem_wait, sem_t *, sem)
_syscall1(int, sem_post, sem_t *, sem)
_syscall1(int, sem_unlink, const char *, name)

int main(int argc, char const *argv[])
{
    pid_t pid;
    int cnt = 0;

    int res = 0;
    int val = -1;
    int pos;
    int i;

    int children = 6;

    sem_t *sem_empty, *sem_full, *sem_mutex, *sem_atom;

    /**
     * O_RDWR   以可读写方式打开文件
     * O_CREAT  若打开文件不存在则自动建立该文件
     * O_TRUNC  若文件存在并且以可写的方式打开时，清空文件
     * -------------------------------------------
     * S_IREAD  00400权限，文件所有者具有可读取权限
     * S_IWRITE 00200权限，文件所有者具有可写入权限
     * 
     * -- viewvos commit @ Zhangxuenan
     */
    int buffer = open("buffer.txt", O_RDWR | O_CREAT | O_TRUNC, S_IREAD | S_IWRITE);

    if (buffer == -1)
    {
        printf("Error! Can not open buffer file!\n");
        exit(EXIT_FAILURE);
    }

    res = lseek(buffer, 0, SEEK_SET);
    if (res == -1)
    {
        perror("lseek error");
        exit(EXIT_FAILURE);
    }

    if ((res = write(buffer, (void *)(&val), sizeof(val))) == -1)
    {
        perror("write error");
        exit(EXIT_FAILURE);
    }

    // 创建缓存区空闲数empty，缓存区满full，文件读写mutex三个信号量
    sem_empty = (sem_t *)sem_open("empty", SIZE);
    if (sem_empty == SEM_FAILED)
    {
        perror("Create semaphore empty failed");
        exit(EXIT_FAILURE);
    }
    sem_full = (sem_t *)sem_open("full", 0);
    if (sem_full == SEM_FAILED)
    {
        perror("Create semaphore full failed!");
        exit(EXIT_FAILURE);
    }
    sem_mutex = (sem_t *)sem_open("mutex", 1);
    if (sem_mutex == SEM_FAILED)
    {
        perror("Create semaphore mutex failed!");
        exit(EXIT_FAILURE);
    }
    sem_atom = (sem_t *)sem_open("atom", 1);
    if (sem_atom == SEM_FAILED)
    {
        perror("Create semaphore atom failed!");
        exit(EXIT_FAILURE);
    }

    // 生产者
    if ((pid = fork()) == -1)
    {
        perror("Productor fork failed!");
        exit(EXIT_FAILURE);
    }
    else if (!pid)
    {
        while (cnt <= M)
        {
            /* P操作 */
            sem_wait(sem_empty);
            sem_wait(sem_mutex);
            pos = (cnt % SIZE) + 1;
            res = lseek(buffer, pos * sizeof(pos), SEEK_SET);
            if (res == -1)
            {
                perror("Productor lseek failed!");
                exit(EXIT_FAILURE);
            }
            if ((res = write(buffer, (void *)(&cnt), sizeof(cnt))) == -1)
            {
                perror("write failed!");
                exit(EXIT_FAILURE);
            }
            /* V操作 */
            sem_post(sem_mutex);
            sem_post(sem_full);
            cnt++;
        }
        exit(EXIT_SUCCESS);
    }
    else
    {
        for (i = 0; i < children - 1; i++)
        {
            if ((pid = fork()) == -1)
            {
                perror("fork consumer n failed");
                exit(EXIT_FAILURE);
            }
            /* Consumer n */
            else if (!pid)
            {
                do
                {
                    sem_wait(sem_atom);
                    /* 读取上次写入的数值 */
                    sem_wait(sem_mutex);
                    res = lseek(buffer, 0, SEEK_SET);
                    if (res == -1)
                    {
                        perror("Consumer 1 lseek failed!");
                        exit(EXIT_FAILURE);
                    }
                    if ((res = read(buffer, (void *)(&val), sizeof(val))) == -1)
                    {
                        perror("Consumer 1 read failed!");
                        exit(EXIT_FAILURE);
                    }
                    sem_post(sem_mutex);
                    if (val < M)
                    {
                        /* P操作 */
                        sem_wait(sem_full);
                        sem_wait(sem_mutex);
                        pos = (val + 1) % SIZE + 1;
                        res = lseek(buffer, pos * sizeof(int), SEEK_SET);
                        if (res == -1)
                        {
                            perror("Consumer 1 lseek failed!");
                            exit(EXIT_FAILURE);
                        }
                        if ((res = read(buffer, (void *)(&val), sizeof(val))) == -1)
                        {
                            perror("Consumer 1 read failed!");
                            exit(EXIT_FAILURE);
                        }
                        printf("%d:%d\n", getpid(), val);
                        fflush(stdout);
                        /* V操作 */
                        sem_post(sem_mutex);
                        sem_post(sem_empty);
                        /* 把两个消费者共享的读取值val写入到文件，用于共享 */
                        sem_wait(sem_mutex);
                        res = lseek(buffer, 0, SEEK_SET);
                        if (res == -1)
                        {
                            perror("Consumer 1 lseek failed!");
                            exit(EXIT_FAILURE);
                        }
                        if ((res = write(buffer, (void *)(&val), sizeof(val))) == -1)
                        {
                            perror("Consumer 1 write failed!");
                            exit(EXIT_FAILURE);
                        }
                        sem_post(sem_mutex);
                    }
                    sem_post(sem_atom);
                } while (val < M);
                exit(EXIT_SUCCESS);
            }
        }
        while (children--)
            wait(NULL);
        close(buffer);
        sem_unlink("empty");
		sem_unlink("full");
		sem_unlink("mutex");
		sem_unlink("atom");
		exit(EXIT_SUCCESS);
        return 0;
    }
