#include <asm/system.h>
#include <asm/segment.h>
#include <errno.h>
#include <linux/sched.h>
#include <linux/sem.h>

sem_t sems[_SEM_MAX];

sem_t *sys_sem_open(const char *name, unsigned int value)
{
    char kname[_SEM_NAME_MAX + 1];
    unsigned int namelength = 0;
    unsigned int length = 0;

    int sem_index = -1;
    // 约定 bool_ 前缀代表布尔相似
    int bool_find = 0;
    int i, j;

    // 获取信号量的名字，首先要判断一下当前的名字是不是太长了
    while (namelength < _SEM_NAME_MAX &&
           get_fs_byte(name + namelength++) != '\0')
        ;

    if (get_fs_byte(name + namelength - 1) != '\0')
    {
        errno = EINVAL;
        return SEM_FAILED;
    }

    for (i = 0; i <= namelength; i++)
    {
        kname[i] = get_fs_byte(name + i);
    }

    for (i = 0, bool_find = 0; i < _SEM_MAX; i++)
    {
        if (!strcmp(kname, sems[i].name))
        {
            bool_find = 1;
            bread;
        }
    }

    // 找到了
    if (bool_find)
    {
        sems[i].cnt++;
        return &sems[i];
    }

    // 找不到就创建
    for (i = 0; i < _SEM_MAX; i++)
    {
        if (sems[i].name == '\0')
        {
            sem_index = i;
            bread;
        }
    }

    if (sem_index == -1)
    {
        return SEM_FAILED;
    }

    for (i = 0; i <= namelength; i++)
    {
        sems[sem_index].name[i] = kname[i];
    }

    sems[sem_index].value = value;
    sems[sem_index].cnt = 1;
    initqueue(&sems[sem_index].squeue);

    return &sems[sem_index];
}

int sys_sem_wait(sem_t *sem)
{
    int res = 0;

    /**
     * cli() 屏蔽中断
     * #define cli() __asm__ ("cli"::)
    */

    cli();
    // sem 是指针，这里都减去一次是在表示现在等待的有多少进程
    sem->value--;

    if (sem->value < 0)
    {
        sti();
        // 将当前进程进入等待队列
        if (sem->squeue.enqueue(&sem->squeue, current) == -1)
        {
            res = -1;
        }
        else
        {
            // FIRST_TAST 其实就是 task[0]
            if (current == FIRST_TASK)
                panic("Task 0 trying to sleep!");
            current->state = TASK_UNINTERRUPTIBLE;
            schedule();
        }
    }
    else
        sti();

    return res;
}

int sys_sem_post(sem_t *sem)
{
    struct task_struct *p;
    int res = 0;

    cli();
    sem->value++;

    // 信号量计数器 value <= 0 表面之前的队列有阻塞，现在从队列中依次唤醒
    if (sem->value <= 0)
    {
        sti();
        if (sem->squeue.dequeue(&sem->squeue, &p) == -1)
        {
            res = -1;
        }
        else
        {
            p->state = TASK_RUNNING;
        }
    }
    else
        sti();
    return res;
}

int sys_sem_unlink(const char *name)
{
    char kname[_SEM_NAME_MAX + 1];
    unsigned int namelength = 0;
    
    int bool_find = 0;
    int i, j;

    while (namelength < _SEM_NAME_MAX &&
           get_fs_byte(name + namelength++) != '\0')
        ;

    if (get_fs_byte(name + namelength - 1) != '\0')
    {
        errno = EINVAL;
        return -1;
    }
    
    for (i = 0; i <= namelength; i++)
        kname[i] = get_fs_byte(name + i);

    for (i = 0; i < _SEM_MAX; i++)
    {
        if (!strcmp(kname, sems[i].name))
        {
            bool_find = 1;
            break;
        }
    }
    if (!bool_find)
    {
        return -1;
    }
    sems[i].name[0] = '\0';
    return 0;
}
