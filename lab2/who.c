/*
 *  linux/kernel/who.c
 *
 *  (C) 2020  Zhang Xuenan
 */

#include <unistd.h>
#include <errno.h>
#include <linux/kernel.h>
#include <asm/segment.h>
#include <string.h>

volatile char msg[24]; //23 chars and a '\0' as end

int sys_iam(const char *name)
{
    int i, flag;
    char temp[24];
    flag = 0;
    for (i = 0; i < 24; i++)
    {
        temp[i] = get_fs_byte(name + i);
        if (temp[i] == '\0')
        {
            flag = 1;
            break;
        }
    }
    if (flag == 1)
    {
        strcpy(msg, temp);
        return (i);
    }
    return -(EINVAL);
}

int sys_whoami(char *name, unsigned int size)
{
    int i, length;
    length = strlen(msg);
    if (length > size)
    {
        //printk("Size is too small");
        return -(EINVAL);
    }
    for (i = 0; i < length; i++)
    {
        put_fs_byte(msg[i], name + i);
    }
    //printk("Success!%d", i);
    return (i);
}
