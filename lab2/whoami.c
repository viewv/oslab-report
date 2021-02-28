#include <errno.h>
#define __LIBRARY__
#include <unistd.h>
#include <stdio.h>

_syscall2(int, whoami, char *, name, unsigned int, size);

int main(int argc, char const *argv[])
{
    char name[24];
    unsigned int size = 24;
    whoami(name, size);
    printf("%s", name);
    return 0;
}
