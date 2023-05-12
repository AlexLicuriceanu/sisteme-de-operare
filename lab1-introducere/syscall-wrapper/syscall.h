#ifndef SYSCALL_H_
#define SYSCALL_H_	1

int read(int fd, const void *buf, unsigned long count);
int write(int fd, const void *buf, unsigned long count);
void exit(int status);

#endif
