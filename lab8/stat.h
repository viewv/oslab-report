#ifndef _SYS_STAT_H
#define _SYS_STAT_H

#include <sys/types.h>

struct stat
{
	dev_t st_dev;
	ino_t st_ino;
	umode_t st_mode;
	nlink_t st_nlink;
	uid_t st_uid;
	gid_t st_gid;
	dev_t st_rdev;
	off_t st_size;
	time_t st_atime;
	time_t st_mtime;
	time_t st_ctime;
};
//*文件类型
#define S_IFMT 00170000 //文件类型屏蔽码
#define S_IFREG 0100000 //常规文件
#define S_IFBLK 0060000 //块特殊文件
#define S_IFDIR 0040000 //目录文件
#define S_IFCHR 0020000 //字符设备文件
#define S_IFIFO 0010000 //FIFO特殊文件
/*
* 增加需要的文件系统 S_IFPRC
* 这里不用PROC是因为好像人家上面写的都是IF+三个字母
* --viewvos
*/
#define S_IFPRC 0070000 //Proc文件

//*文件属性位
#define S_ISUID 0004000 //执行时设置用户ID
#define S_ISGID 0002000 //执行时设置组ID
#define S_ISVTX 0001000 //对于目录，受限删除标志

//*测试
#define S_ISREG(m) (((m)&S_IFMT) == S_IFREG)  //测试是否为正常文件
#define S_ISDIR(m) (((m)&S_IFMT) == S_IFDIR)  //是否目录文件
#define S_ISCHR(m) (((m)&S_IFMT) == S_IFCHR)  //是否字符设备文件
#define S_ISBLK(m) (((m)&S_IFMT) == S_IFBLK)  //是否块设备文件
#define S_ISFIFO(m) (((m)&S_IFMT) == S_IFIFO) //是否FIFO特殊文件
//*viewvos PROC->PRC
#define S_ISPRC(m) (((m)&S_IFMT) == S_IFPRC) //是否Proc文件

#define S_IRWXU 00700 //宿主可以进行读、写、执行/搜索，名称最后字母代表User
#define S_IRUSR 00400 //宿主读许可，最后三个字母代表User
#define S_IWUSR 00200 //宿主写许可
#define S_IXUSR 00100 //宿主执行/搜索许可

#define S_IRWXG 00070 //组成员可以读、写、执行/搜索，名称最后字母代表Group
#define S_IRGRP 00040 //组成员读许可，最后三个字母代表Group
#define S_IWGRP 00020 //组成员写许可
#define S_IXGPR 00010 //组成员执行/搜索许可

#define S_IRWXO 00007 //其他人读、写、执行/搜索。名称最后字母O代表Other
#define S_IROTH 00004 //其他人读许可，最后三个字母代表Other
#define S_IWOTH 00002 //其他人写许可
#define S_IXOTH 00001 //其他人执行/搜索许可

extern int chmod(const char *_path, mode_t mode);			  //修改文件属性
extern int fstat(int fildes, struct stat *stat_buf);		  //取指定文件句柄的文件状态信息
extern int mkdir(const char *_path, mode_t mode);			  //创建目录
extern int mkfifo(const char *_path, mode_t mode);			  //创建管道文件
extern int stat(const char *filename, struct stat *stat_buf); //取指定文件名的文件状态信息
extern mode_t umask(mode_t mask);							  //取属性屏蔽码

#endif
