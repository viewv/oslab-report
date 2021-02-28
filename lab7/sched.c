/*
 *  linux/kernel/sched.c
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 * 'sched.c' is the main kernel file. It contains scheduling primitives
 * (sleep_on, wakeup, schedule etc) as well as a number of simple system
 * call functions (type getpid(), which just extracts a field from
 * current-task
 */
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/sys.h>
#include <linux/fdreg.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/segment.h>

#include <signal.h>

#define _S(nr) (1 << ((nr)-1))
#define _BLOCKABLE (~(_S(SIGKILL) | _S(SIGSTOP)))

void show_task(int nr, struct task_struct *p)
{
	int i, j = 4096 - sizeof(struct task_struct);

	printk("%d: pid=%d, state=%d, ", nr, p->pid, p->state);
	i = 0;
	while (i < j && !((char *)(p + 1))[i])
		i++;
	printk("%d (of %d) chars free in kernel stack\n\r", i, j);
}
//* 键盘确定显示各个任务的状态函数
void show_stat(void)
{
	int i;

	for (i = 0; i < NR_TASKS; i++)
		if (task[i])
			show_task(i, task[i]);
}

void tty_hide_init(void)
{
	tty_hide = 0;
}
void change_tty_hide(void)
{
	tty_hide = !tty_hide;
	//printk("change tty_hide: %d\n", tty_hide);
}

#define LATCH (1193180 / HZ)

extern void mem_use(void);

extern int timer_interrupt(void);
extern int system_call(void);

union task_union {
	struct task_struct task;
	char stack[PAGE_SIZE];
};

static union task_union init_task = {
	INIT_TASK,
};

long volatile jiffies = 0;
long startup_time = 0;
struct task_struct *current = &(init_task.task);
struct task_struct *last_task_used_math = NULL;

struct task_struct *task[NR_TASKS] = {
	&(init_task.task),
};

long user_stack[PAGE_SIZE >> 2];

struct
{
	long *a;
	short b;
} stack_start = {&user_stack[PAGE_SIZE >> 2], 0x10};
/*
 *  'math_state_restore()' saves the current math information in the
 * old math state array, and gets the new ones from the current task
 */
void math_state_restore()
{
	if (last_task_used_math == current)
		return;
	__asm__("fwait");
	if (last_task_used_math)
	{
		__asm__("fnsave %0" ::"m"(last_task_used_math->tss.i387));
	}
	last_task_used_math = current;
	if (current->used_math)
	{
		__asm__("frstor %0" ::"m"(current->tss.i387));
	}
	else
	{
		__asm__("fninit" ::);
		current->used_math = 1;
	}
}

/*
 *  'schedule()' is the scheduler function. This is GOOD CODE! There
 * probably won't be any reason to change this, as it should work well
 * in all circumstances (ie gives IO-bound processes good response etc).
 * The one thing you might take a look at is the signal-handler code here.
 *
 *   NOTE!!  Task 0 is the 'idle' task, which gets called when no other
 * tasks can run. It can not be killed, and it cannot sleep. The 'state'
 * information in task[0] is never used.
 * 
 * 调度函数
 */
void schedule(void)
{
	int i, next, c;
	struct task_struct **p;
	/*
	* 2020 Zhang Xuenan
	* try to generate a new about next pcb => log 
	* hope will work :-)
	* --viewvos append
	*/
	struct task_struct **tmp;

	/* check alarm, wake up any interruptible tasks that have got a signal */
	/*
	* 检查现在的alarm，alarm是进程的报警定时器，如果进程使用系统调用alarm()设置的字段值
	* （alarm函数会把秒数换成滴答数，加上现在系统的滴答数字存在这里）之后当系统的滴答数大于
	* 这个alarm值的时候，内核就会想这个进程发送一个SIGALRM（14）信号，默认这个信号会终止
	* 程序的执行，也可以使用信号捕捉函数（signal或者sigaction）来捕捉信号之后进行指定操作
	* --viewvos commit
	* signal 字段是当前收到信号的位图，共有32位，每个位置都代表一种信号
	* 信号值 =（位偏移值+1）这里Linux 0.11内核最多便有32种信号
	* 那么代码中的实际上就是算位偏移值之后或到当前信号位置上去，这样就能让信号位图变成这个SIGALRM
	* --viewvos commit
	* blocked字段是进程当前不想处理的信号的阻塞位图，这就明白为什么要用位图了，与signal类似
	* 一位代表一个需要阻塞的状态
	* --viewvos commit
	* 这里的LAST_TASK实际上就是task[NR_TASKS-1],而 NR_TASKS就是最大进程数64，这就是最后一个
	* 进程，而 FIRST_TASK 就是第一个（不知道为啥要这么写）之后就设置进行alarm操作
	* ---viewvos commit
	*/
	for (p = &LAST_TASK; p > &FIRST_TASK; --p)
		if (*p)
		{
			if ((*p)->alarm && (*p)->alarm < jiffies)
			{
				(*p)->signal |= (1 << (SIGALRM - 1));
				(*p)->alarm = 0;
			}
			if (((*p)->signal & ~(_BLOCKABLE & (*p)->blocked)) &&
				(*p)->state == TASK_INTERRUPTIBLE)
			{
				(*p)->state = TASK_RUNNING;
				fprintk(3, "%ld\t%c\t%ld\n", (*p)->pid, 'J', jiffies);
			}
		}

	/* this is the scheduler proper: */
	/*
	* 扫描任务数组，比较每个就绪态任务允许时间递减，滴答计数counter的值来确定当前那个进程
	* 运行时间最少，也就是那个值最大，就表示这个进程运行时间还补偿，就选中这个进程，之后
	* 使用任务切换宏函数切换到这个进程执行。
	*/
	while (1)
	{
		c = -1;
		next = 0;
		i = NR_TASKS;
		p = &task[NR_TASKS];
		while (--i)
		{
			if (!*--p)
				continue;
			if ((*p)->state == TASK_RUNNING && (*p)->counter > c)
				c = (*p)->counter, next = i; //* 经典操作求最大
		}
		if (c)
			break; //*找到c了
		/*
		* 不巧的是处在TASK_RUNNING进程对时间片都用完了，
		* 这个时候内核会根据优先级重新计算每个任务需要的时间片值
		* 会对系统中所有进程（注意所有），包括睡眠中的进程重新计算counter，公式是：
		* counter = (counter/2)+prioroty 其实就是先 >> 1 之后 + 
		* 这样的操作就对正在睡眠对进程有较高对counter值，看见最前面对while(1)了吗？
		* 说明这样对操作最后一定要找到一个进程，最后对switch_to函数来调度切换
		* --viewvos commit
		* 这是一种非常原始对调度算法，其时间复杂度是O(n)的，这样的时间复杂度是没有办法
		* 提升到很快素的的速度，如果有时间我可以尝试一下简单的fix，类似桶排序的优化算了
		*/
		for (p = &LAST_TASK; p > &FIRST_TASK; --p)
			if (*p)
				(*p)->counter = ((*p)->counter >> 1) + (*p)->priority;
	}
	tmp = &task[next];
	if ((*tmp)->pid != current->pid)
	{
		if (current->state == TASK_RUNNING)
		{
			fprintk(3, "%ld\t%c\t%ld\n", current->pid, 'J', jiffies); //*R运行状态
		}
		fprintk(3, "%ld\t%c\t%ld\n", (*tmp)->pid, 'R', jiffies); //*R运行状态
	}
	switch_to(next);
}

int sys_pause(void)
{
	current->state = TASK_INTERRUPTIBLE;
	if (current->pid != 0)
	{
		//*这个地方主要是吧，应该把0也显示出来，可是0状态这里没事等待对状态实在是太多了
		//*一不小心十万多行出去了，所以这里屏蔽了0状态默认刷新系统
		fprintk(3, "%ld\t%c\t%ld\n", current->pid, 'W', jiffies);
	}
	schedule();
	return 0;
}
/*
* 这个函数短但是难理解，sleep_on其实类似阻塞，当有一个任务请求当资源
* 正忙，这个时候就需要让这个进程从内存转移出去等待一段时间，之后切换回来执行
* 而放入等待队列是依赖函数中当tmp指针作为正在等待任务当联系
* 首先p是等待队列当头指针，tmp是函数堆栈上当临时指针，current是当前进程指针
* 有点类似交换两个数字需要中间变量
*/
void sleep_on(struct task_struct **p)
{
	//*进入函数p指向等待队列中等待的任务结构
	struct task_struct *tmp;

	//*当等待队列当p没有等待队伍当时候，就不需要在这里return
	if (!p)
		return;
	//* Really funny! I need Add a ! ZXN
	if (current == &(init_task.task))
		panic("task[0] trying to sleep!");
	tmp = *p;	  //*保存原来等待当任务，其实也是为了保留p指针
	*p = current; //*之后让p指向当前新的需要等待的任务
	//*防调度程序调度走这个current
	current->state = TASK_UNINTERRUPTIBLE;
	fprintk(3, "%ld\t%c\t%ld\n", current->pid, 'W', jiffies);
	//* 开始调度
	schedule();
	if (tmp)
	{
		tmp->state = 0;
		fprintk(3, "%ld\t%c\t%ld\n", tmp->pid, 'J', jiffies);
	}
}

void interruptible_sleep_on(struct task_struct **p)
{
	struct task_struct *tmp;

	if (!p)
		return;
	if (current == &(init_task.task))
		panic("task[0] trying to sleep");
	tmp = *p;
	*p = current;
repeat:
	current->state = TASK_INTERRUPTIBLE;
	fprintk(3, "%ld\t%c\t%ld\n", current->pid, 'W', jiffies);
	schedule();
	if (*p && *p != current)
	{
		(**p).state = 0;
		fprintk(3, "%ld\t%c\t%ld\n", (**p).pid, 'J', jiffies);
		goto repeat;
	}
	*p = NULL;
	if (tmp)
	{
		fprintk(3, "%ld\t%c\t%ld\n", tmp->pid, 'J', jiffies);
		tmp->state = 0;
	}
}

void wake_up(struct task_struct **p)
{
	//*把等待可用资源的指定任务只为就绪状态
	if (p && *p)
	{
		(**p).state = 0;
		fprintk(3, "%ld\t%c\t%ld\n", (**p).pid, 'J', jiffies);
		*p = NULL;
	}
}

/*
 * OK, here are some floppy things that shouldn't be in the kernel
 * proper. They are here because the floppy needs a timer, and this
 * was the easiest way of doing it.
 */
static struct task_struct *wait_motor[4] = {NULL, NULL, NULL, NULL};
static int mon_timer[4] = {0, 0, 0, 0};
static int moff_timer[4] = {0, 0, 0, 0};
unsigned char current_DOR = 0x0C;

int ticks_to_floppy_on(unsigned int nr)
{
	extern unsigned char selected;
	unsigned char mask = 0x10 << nr;

	if (nr > 3)
		panic("floppy_on: nr>3");
	moff_timer[nr] = 10000; /* 100 s = very big :-) */
	cli();					/* use floppy_off to turn it off */
	mask |= current_DOR;
	if (!selected)
	{
		mask &= 0xFC;
		mask |= nr;
	}
	if (mask != current_DOR)
	{
		outb(mask, FD_DOR);
		if ((mask ^ current_DOR) & 0xf0)
			mon_timer[nr] = HZ / 2;
		else if (mon_timer[nr] < 2)
			mon_timer[nr] = 2;
		current_DOR = mask;
	}
	sti();
	return mon_timer[nr];
}

void floppy_on(unsigned int nr)
{
	cli();
	while (ticks_to_floppy_on(nr))
		sleep_on(nr + wait_motor);
	sti();
}

void floppy_off(unsigned int nr)
{
	moff_timer[nr] = 3 * HZ;
}

void do_floppy_timer(void)
{
	int i;
	unsigned char mask = 0x10;

	for (i = 0; i < 4; i++, mask <<= 1)
	{
		if (!(mask & current_DOR))
			continue;
		if (mon_timer[i])
		{
			if (!--mon_timer[i])
				wake_up(i + wait_motor);
		}
		else if (!moff_timer[i])
		{
			current_DOR &= ~mask;
			outb(current_DOR, FD_DOR);
		}
		else
			moff_timer[i]--;
	}
}

#define TIME_REQUESTS 64

static struct timer_list
{
	long jiffies;
	void (*fn)();
	struct timer_list *next;
} timer_list[TIME_REQUESTS], *next_timer = NULL;

void add_timer(long jiffies, void (*fn)(void))
{
	struct timer_list *p;

	if (!fn)
		return;
	cli();
	if (jiffies <= 0)
		(fn)();
	else
	{
		for (p = timer_list; p < timer_list + TIME_REQUESTS; p++)
			if (!p->fn)
				break;
		if (p >= timer_list + TIME_REQUESTS)
			panic("No more time requests free");
		p->fn = fn;
		p->jiffies = jiffies;
		p->next = next_timer;
		next_timer = p;
		while (p->next && p->next->jiffies < p->jiffies)
		{
			p->jiffies -= p->next->jiffies;
			fn = p->fn;
			p->fn = p->next->fn;
			p->next->fn = fn;
			jiffies = p->jiffies;
			p->jiffies = p->next->jiffies;
			p->next->jiffies = jiffies;
			p = p->next;
		}
	}
	sti();
}

void do_timer(long cpl)
{
	extern int beepcount;
	extern void sysbeepstop(void);

	if (beepcount)
		if (!--beepcount)
			sysbeepstop();

	if (cpl)
		current->utime++;
	else
		current->stime++;

	if (next_timer)
	{
		next_timer->jiffies--;
		while (next_timer && next_timer->jiffies <= 0)
		{
			void (*fn)(void);

			fn = next_timer->fn;
			next_timer->fn = NULL;
			next_timer = next_timer->next;
			(fn)();
		}
	}
	if (current_DOR & 0xf0)
		do_floppy_timer();
	if ((--current->counter) > 0)
		return;
	current->counter = 0;
	if (!cpl)
		return;
	schedule();
}

int sys_alarm(long seconds)
{
	int old = current->alarm;

	if (old)
		old = (old - jiffies) / HZ;
	current->alarm = (seconds > 0) ? (jiffies + HZ * seconds) : 0;
	return (old);
}

int sys_getpid(void)
{
	return current->pid;
}

int sys_getppid(void)
{
	return current->father;
}

int sys_getuid(void)
{
	return current->uid;
}

int sys_geteuid(void)
{
	return current->euid;
}

int sys_getgid(void)
{
	return current->gid;
}

int sys_getegid(void)
{
	return current->egid;
}

int sys_nice(long increment)
{
	if (current->priority - increment > 0)
		current->priority -= increment;
	return 0;
}

void sched_init(void)
{
	int i;
	struct desc_struct *p;

	if (sizeof(struct sigaction) != 16)
		panic("Struct sigaction MUST be 16 bytes");
	set_tss_desc(gdt + FIRST_TSS_ENTRY, &(init_task.task.tss));
	set_ldt_desc(gdt + FIRST_LDT_ENTRY, &(init_task.task.ldt));
	p = gdt + 2 + FIRST_TSS_ENTRY;
	for (i = 1; i < NR_TASKS; i++)
	{
		task[i] = NULL;
		p->a = p->b = 0;
		p++;
		p->a = p->b = 0;
		p++;
	}
	/* Clear NT, so that we won't have troubles with that later on */
	__asm__("pushfl ; andl $0xffffbfff,(%esp) ; popfl");
	ltr(0);
	lldt(0);
	outb_p(0x36, 0x43);			/* binary, mode 3, LSB/MSB, ch 0 */
	outb_p(LATCH & 0xff, 0x40); /* LSB */
	outb(LATCH >> 8, 0x40);		/* MSB */
	set_intr_gate(0x20, &timer_interrupt);
	outb(inb_p(0x21) & ~0x01, 0x21);
	set_system_gate(0x80, &system_call);
}
