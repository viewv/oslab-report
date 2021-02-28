#include <errno.h>
#include <unistd.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <asm/system.h>

/**
 * viewv os commit 
 * Commit By Zhangxuenan
 * 进程最大也就 64
*/
#define SHM_LIMIT 64

key_t shm[SHM_LIMIT] ={0};

int sys_shmget(key_t key,size_t size){
	if(shm[key] != 0)
		return key;
	if(size > PAGE_SIZE)
		return -EINVAL;
	/*获取一页内存*/
	if(!(shm[key] = get_free_page())){
		shm[key] = 0;
		return -ENOMEM;
	}
	return key;
}

void *sys_shmat(int shmid){
	if(shmid < 0 || shmid >= SHM_LIMIT)
		return -EINVAL;
	put_page(shm[shmid],get_base(current->ldt[1]) + current->brk);
	return (void*)current->brk;
}
