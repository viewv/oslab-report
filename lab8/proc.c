/*
 *  linux/fs/proc.c
 *
 *  2020 Zhang Xuenan
 */

#include <errno.h>
#include <fcntl.h>

#include <stdarg.h>
#include <string.h>

#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/segment.h>


/* set_bit uses setb, as gas doesn't recognize setc */
#define set_bit(bitnr,addr) ({ \
register int __res ; \
__asm__("bt %2,%3;setb %%al":"=a" (__res):"a" (0),"r" (bitnr),"m" (*(addr))); \
__res; })

#define MIN(a, b) (((a) < (b)) ? (a) : (b))

char pbuffer[4096] = {'\0'};

int sprintf(char *buf, const char *fmt, ...)
{
    va_list args; int i;
    va_start(args, fmt);
    i=vsprintf(buf, fmt, args);
    va_end(args);
    return i;
}

int getpsInfo(){
    int loc = 0;
    struct task_struct **p;
    //* 提示信息 
    loc += sprintf(pbuffer+loc,"%s","RUNNING-0, INTERRUPTIBLE-1, UNINTERRUPTIBLE-2, ZOMBIE-3, STOPPED-4\n");
    loc += sprintf(pbuffer+loc,"%s","pid\tuser\tstate\tfather\tcounter\tstart_time\n");
    for (p = &LAST_TASK;p > &FIRST_TASK;--p){
        if (*p){
            loc += sprintf(pbuffer + loc,
                           "%ld\t%ld\t%ld\t%ld\t%ld\t%ld\n",
                           (*p)->pid,(*p)->uid,(*p)->state,
                           (*p)->father,(*p)->counter,(*p)->start_time);
        }
    }
    return loc;
}

int getHdinfo(){
    int read = 0;
    int i, free,step;
    struct super_block *p;
    step = 0;
    //*这个地方非常我写的很不优雅，我不喜欢，应该还是我对结构不熟悉
    while (p = get_super(0x301 + step)){
        if (p){
            free = 0;
            read += sprintf(pbuffer+read,"Hd%d Block Info:\n",step/2);
            read += sprintf(pbuffer+read,"Total blocks on Hd%d: %d\n",step/2,p->s_nzones);
            i = p->s_nzones;
            while (-- i >= 0){
                if (!set_bit(i&8191,p->s_zmap[i>>13]->b_data))
                    free++;
            }
            read += sprintf(pbuffer+read,"Free blocks on Hd%d: %d\n",step/2,free);
            read += sprintf(pbuffer+read,"Used blocks on Hd%d: %d\n",step/2,p->s_nzones-free);
            //*Inodes 信息
            read += sprintf(pbuffer+read,"Hd%d Inodes Info:\n",step/2);
            read += sprintf(pbuffer+read,"Total Inodes on Hd%d: %d\n",step/2,p->s_ninodes);
            free = 0;
            i=p->s_ninodes+1;
            while(--i >= 0)
            {
                if(!set_bit(i&8191,p->s_imap[i>>13]->b_data))
                    free++;
            }
            read += sprintf(pbuffer+read,"Free Inodes on Hd%d: %d\n",step/2,free);
            read += sprintf(pbuffer+read,"Used Inodes on Hd%d: %d\n",step/2,p->s_ninodes-free);
        }
        step += 5;
    }
    return read;
}

int getInodeinfo(){
    int i;
    int read = 0;
    struct super_block * p;
	struct m_inode *mi;
	p=get_super(0x301);  
	i=p->s_ninodes+1;
	i=0;
    //printk("D-ninodes: %d",p->s_ninodes);
	while(++i < p->s_ninodes+1){
		if(set_bit(i&8191,p->s_imap[i>>13]->b_data)){
			mi = iget(0x301,i);
			read += sprintf(pbuffer+read,"inr:%d;zone[0]:%d\n",mi->i_num,mi->i_zone[0]);
			iput(mi);
		}
        //*这里不能读太多，否则我连文件都输出不出去（干，我512都段错误）
        //*只能500了，才能cat /proc/inodeinfo > inode.txt 成功
        if(read >= 500) 
			break;
	}
    return read;
}

/*
* 0	psinfo
* 1 meminfo
* 2 cpuinfo
* 3 hdinfo
* 4 inodeinfo
* May be I will just finish psinfo and hdinfo may be a fake cpuinfo
* may be a real inodeinfo '^', so their will be 0, 3 and 4
* I think that will be a small fix to block_read func. Hope it will work!
*
* One more thing!
* inode->i_zone[0]，这就是mknod()时指定的dev——设备编号
* buf，指向用户空间，就是read()的第二个参数，用来接收数据
* count，就是read()的第三个参数，说明buf指向的缓冲区大小
* &file->f_pos，f_pos是上一次读文件结束时“文件位置指针”的指向。
* 这里必须传指针，因为处理函数需要根据传给buf的数据量修改f_pos的值。
* 处理函数的功能是根据设备编号，把不同的内容写入到用户空间的buf
* 
* 2020 Zhangxuenan
* --viewvos
*/

int proc_read(int dev, unsigned long * pos, char * buf, int count)
{
    int i;
    int chars;
    int read = 0;
    register char *p;
    
	if (dev == 0)
        read = getpsInfo();
    if (dev == 3)
        read = getHdinfo();
    if (dev == 4)
        read = getInodeinfo();
    p = pbuffer + *pos;
    buf += *pos;
    chars = MIN(read,count);
    for (i = 0; i < chars; i++)
    {
        if(*(p+i) == '\0')  
            break; 
        put_fs_byte(*(p+i),buf + i);
    }
    *pos += i;
	return i;
}

