/*
 *  linux/kernel/fork.c
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 *  'fork.c' contains the help-routines for the 'fork' system call
 * (see also system_call.s), and some misc functions ('verify_area').
 * Fork is rather simple, once you get the hang of it, but the memory
 * management can be a bitch. See 'mm/mm.c': 'copy_page_tables()'
 */
#include <errno.h>

#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/segment.h>
#include <asm/system.h>

extern void write_verify(unsigned long address);
extern long first_return_from_kernel(void); 

long last_pid = 0;

void verify_area(void *addr, int size)
{
	unsigned long start;

	start = (unsigned long)addr;
	size += start & 0xfff;
	start &= 0xfffff000;
	start += get_base(current->ldt[2]);
	while (size > 0)
	{
		size -= 4096;
		write_verify(start);
		start += 4096;
	}
}

int copy_mem(int nr, struct task_struct *p)
{
	unsigned long old_data_base, new_data_base, data_limit;
	unsigned long old_code_base, new_code_base, code_limit;

	code_limit = get_limit(0x0f);
	data_limit = get_limit(0x17);
	old_code_base = get_base(current->ldt[1]);
	old_data_base = get_base(current->ldt[2]);
	if (old_data_base != old_code_base)
		panic("We don't support separate I&D");
	if (data_limit < code_limit)
		panic("Bad data_limit");
	new_data_base = new_code_base = nr * 0x4000000;
	p->start_code = new_code_base;
	set_base(p->ldt[1], new_code_base);
	set_base(p->ldt[2], new_data_base);
	if (copy_page_tables(old_data_base, new_data_base, data_limit))
	{
		printk("free_page_tables: from copy_mem\n");
		free_page_tables(new_data_base, data_limit);
		return -ENOMEM;
	}
	return 0;
}

/*
 *  Ok, this is the main fork-routine. It copies the system process
 * information (task[nr]) and sets up the necessary registers. It
 * also copies the data segment in it's entirety.
 */
int copy_process(int nr, long ebp, long edi, long esi, long gs, long none,
				 long ebx, long ecx, long edx,
				 long fs, long es, long ds,
				 long eip, long cs, long eflags, long esp, long ss)
{
	struct task_struct *p;
	int i;
	struct file *f;
	//* viewvos add for kernael stack
	long *kstack;

	//首先系统要在任务数组中找到一个还没有被任何进程使用的空槽，Linux 0.11这里最多允许64个线程
	p = (struct task_struct *)get_free_page();
	if (!p)
		return -EAGAIN; //如果找不到就要报错，没有空槽
	//申请一页内存
	kstack = (long *)(PAGE_SIZE + (long)p);
	task[nr] = p;
	//下面复制当前进程任务数据作为新进程的模版
	*p = *current; /* NOTE! this doesn't copy the supervisor stack */
	//*防止这个时候还没有被完全新建的进程被进程调度程序被调度程序执行
	//*设置为不能中断状态，相当于现在是创建状态
	p->state = TASK_UNINTERRUPTIBLE;
	p->pid = last_pid;
	p->father = current->pid; //*设置当前进程设置为新建进程的父进程
	p->counter = p->priority;
	//* change stack First Copy from father --viewvos
	*(--kstack) = ss & 0xffff;
	*(--kstack) = esp;
	*(--kstack) = eflags;
	*(--kstack) = cs & 0xffff;
	*(--kstack) = eip;
	*(--kstack) = ds & 0xffff;
	*(--kstack) = es & 0xffff;
	*(--kstack) = fs & 0xffff;
	*(--kstack) = gs & 0xffff;
	*(--kstack) = esi;
	*(--kstack) = edi;
	*(--kstack) = edx;
	//* finish copy ass first_return_from_kernel
	*(--kstack) = (long)first_return_from_kernel;
	*(--kstack) = ebp;
	*(--kstack) = ecx;
	*(--kstack) = ebx;
	*(--kstack) = 0;
	p->kernelstack = (long)kstack;
	//* viewvos add OK
	p->signal = 0;
	p->alarm = 0;
	p->leader = 0; /* process leadership doesn't inherit */
	p->utime = p->stime = 0;
	p->cutime = p->cstime = 0;
	p->start_time = jiffies; //当前时钟滴答数
	//* 这里已经设置好了时钟了，就开始创建了
	fprintk(3, "%ld\t%c\t%ld\n", p->pid, 'N', jiffies); //*N创建状态
	p->tss.back_link = 0;
	p->tss.esp0 = PAGE_SIZE + (long)p;
	p->tss.ss0 = 0x10;
	p->tss.eip = eip;
	p->tss.eflags = eflags;
	p->tss.eax = 0;
	p->tss.ecx = ecx;
	p->tss.edx = edx;
	p->tss.ebx = ebx;
	p->tss.esp = esp;
	p->tss.ebp = ebp;
	p->tss.esi = esi;
	p->tss.edi = edi;
	p->tss.es = es & 0xffff;
	p->tss.cs = cs & 0xffff;
	p->tss.ss = ss & 0xffff;
	p->tss.ds = ds & 0xffff;
	p->tss.fs = fs & 0xffff;
	p->tss.gs = gs & 0xffff;
	p->tss.ldt = _LDT(nr);
	p->tss.trace_bitmap = 0x80000000;
	if (last_task_used_math == current)
		__asm__("clts ; fnsave %0" ::"m"(p->tss.i387));
	if (copy_mem(nr, p))
	{
		task[nr] = NULL;
		free_page((long)p);
		return -EAGAIN;
	}
	for (i = 0; i < NR_OPEN; i++)
		if ((f = p->filp[i]))
			f->f_count++;
	if (current->pwd)
		current->pwd->i_count++;
	if (current->root)
		current->root->i_count++;
	if (current->executable)
		current->executable->i_count++;
	set_tss_desc(gdt + (nr << 1) + FIRST_TSS_ENTRY, &(p->tss));
	set_ldt_desc(gdt + (nr << 1) + FIRST_LDT_ENTRY, &(p->ldt));
	//*就绪
	p->state = TASK_RUNNING;							/* do this last, just in case */
	fprintk(3, "%ld\t%c\t%ld\n", p->pid, 'J', jiffies); //*就绪状态
	return last_pid;
}

int find_empty_process(void)
{
	int i;

repeat:
	if ((++last_pid) < 0)
		last_pid = 1;
	for (i = 0; i < NR_TASKS; i++)
		if (task[i] && task[i]->pid == last_pid)
			goto repeat;
	for (i = 1; i < NR_TASKS; i++)
		if (!task[i])
			return i;
	return -EAGAIN;
}
