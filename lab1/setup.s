.globl begtext, begdata, begbss, endtext, enddata, endbss
.text
begtext:
.data
begdata:
.bss
begbss:
.text

INITSEG  = 0x9000			! we move boot here - out of the way
SETUPSEG = 0x9020			! setup starts here

entry _start
_start:

! cs是代码段地址
! 其实这个地方的系统堆栈地址sp已经在bootsect中指定了
! 所以在setup中不需要再指定堆栈
	mov	ax,cs
	mov	ds,ax
	mov	es,ax
	
	mov	ah,#0x03		! read cursor pos
	xor	bh,bh
	int	0x10
	
	mov	cx,#30          ! length os mesage
	mov	bx,#0x000c		! page 0, attribute 7 (normal)
	mov	bp,#msg
	mov	ax,#0x1301		! write string, move cursor
	int	0x10

!读取光标
	mov    ax,#INITSEG    
    mov    ds,ax 	   !设置ds=0x9000
    mov    ah,#0x03    !读入光标位置
    xor    bh,bh
    int    0x10        !调用0x10中断
    mov    [0],dx      !将光标位置写入0x90000.

!读入内存大小位置
    mov    ah,#0x88
    int    0x15
    mov    [2],ax
    
! Get video-card data: 获取显卡数据

	mov	ah,#0x0f
	int	0x10
	mov	[4],bx		! bh = display page
	mov	[6],ax		! al = video mode, ah = window width
    
! check for EGA/VGA and some config parameters

	mov	ah,#0x12
	mov	bl,#0x10
	int	0x10
	mov	[8],bh
	mov	[10],bl
	mov	[12],cl
	
!从0x41处拷贝16个字节（磁盘参数表）读取磁盘信息
    mov    ax,#0x0000
    mov    ds,ax
    lds    si,[4*0x41]
    mov    ax,#INITSEG
    mov    es,ax
    mov    di,#0x0080
    mov    cx,#0x10
    rep    
    movsb
	
! 物理地址=段地址x16+偏移地址，df为数据段段寄存器
! 前面修改了ds寄存器（在读取磁盘信息段时候），这里将其设置为0x9000
	mov ax,#INITSEG
	mov ds,ax
	mov ax,#SETUPSEG
	mov	es,ax 
	
!显示 Cursor Position: 字符串
	mov	ah,#0x03		! read cursor pos
	xor	bh,bh
	int	0x10
	mov	cx,#16
	mov	bx,#0x0009		! Green
	mov	bp,#cursor
	mov	ax,#0x1301		! write string, move cursor
	int	0x10
	
!调用 print_hex:
	mov ax,[0]
	call print_hex
	call print_nl
	
!显示 Memory Size: 
	mov	ah,#0x03		! read cursor pos
	xor	bh,bh
	int	0x10
	mov	cx,#12
	mov	bx,#0x0009		! blue
	mov	bp,#mem
	mov	ax,#0x1301		! write string, move cursor
	int	0x10
	
!调用 print_hex:
	mov ax,[2]
	call print_hex
	
!显示相应 cylinders:
	mov	ah,#0x03		! read cursor pos
	xor	bh,bh
	int	0x10
	mov	cx,#25
	mov	bx,#0x0009		! page 0, attribute c 
	mov	bp,#cylinders
	mov	ax,#0x1301		! write string, move cursor
	int	0x10

!显示，注意这里是16B磁盘信息，这里是变址寻址，也就是取出来ds+0x04
	mov ax,[0x80]
	call print_hex
	call print_nl

!显示 head:
	mov	ah,#0x03		! read cursor pos
	xor	bh,bh
	int	0x10
	mov	cx,#8
	mov	bx,#0x0009		! page 0, attribute c 
	mov	bp,#head
	mov	ax,#0x1301		! write string, move cursor
	int	0x10

! 显示 heads数量 
	mov ax,[0x80+0x02]
	call print_hex
	call print_nl

！显示 扇区信息
	mov	ah,#0x03		! read cursor pos
	xor	bh,bh
	int	0x10
	mov	cx,#8
	mov	bx,#0x0009		! page 0, attribute c 
	mov	bp,#sectors
	mov	ax,#0x1301		! write string, move cursor
	int	0x10

！显示 每磁道扇区数
	mov ax,[0x80+0x0e]
	call print_hex
	call print_nl
    
!显示 Video RAM:
	mov	ah,#0x03		! read cursor pos
	xor	bh,bh
	int	0x10
	mov	cx,#10
	mov	bx,#0x000a		! page 0, attribute c 
	mov	bp,#video
	mov	ax,#0x1301		! write string, move cursor
	int	0x10
! show info
    mov ax,[10]
    call print_hex
	call print_nl
    
l:  jmp l

	
!以16进制方式打印ax寄存器里的16位数，修改的实验手册
print_hex:
	mov cx,#4   ! 4个十六进制数字
	mov dx,ax   ! 读取ax指向的值
print_digit:
	rol dx,#4  ! 循环以使低4比特用上 !! 取dx的高4比特移到低4比特处。
	mov ax,#0xe0f  ! ah = 请求的功能值,al = 半字节(4个比特)掩码。
	and al,dl ! 取dl的低4比特值。
	add al,#0x30  ! 给al数字加上十六进制0x30
	cmp al,#0x3a
	jl outp  !是一个不大于十的数字
	add al,#0x07  !是a~f,要多加7
outp:
	int 0x10
	loop print_digit
	ret

!打印回车换行
print_nl:
	mov ax,#0xe0d
	int 0x10
	mov al,#0xa
	int 0x10
	ret
msg:
	.byte 13,10
    .ascii "viewvOS Now In Setup ..."
	.byte 13,10,13,10
	
cursor:
    .ascii "Cursor Position:"
mem:
	.ascii "Memory Size:"
cylinders:
	.ascii "KB"
	.byte 13,10,13,10
	.ascii "HD Info"
	.byte 13,10
	.ascii "Cylinders:"
head:
	.ascii "Headers:"
sectors:
	.ascii "Secotrs:"
video:
    .ascii "Video RAM:"
    
.text
endtext:
.data
enddata:
.bss
endbss:
