#include <sys/types.h>
#include <unistd.h>
#include "led-matrix-lib/utils.h"

void _exit(int status) {
	//printf("_exit called\r\n");
	NOT_USED(status);
	while(1);
}

void *_sbrk(int increment) {
	//printf("_sbrk called\r\n");
	NOT_USED(increment);
	return 0x0;
}

int _kill(pid_t pid, int sig) {
	NOT_USED(pid);
	NOT_USED(sig);
	return 0;
}

pid_t _getpid(void) {
	return 0;
}

ssize_t _write(int fd, const void *buf, size_t count) {
	NOT_USED(fd);
	NOT_USED(buf);
	NOT_USED(count);
	return 0;
}

int _close(int fd) {
	NOT_USED(fd);
	return 0;
}

int _fstat(int fd, void *buf) {
	NOT_USED(fd);
	NOT_USED(buf);
	return 0;
}

int _isatty(int fd) {
	NOT_USED(fd);
	return 0;
}

off_t _lseek(int fd, off_t offset, int whence) {
	NOT_USED(fd);
	NOT_USED(offset);
	NOT_USED(whence);
	return 0;
}

ssize_t _read(int fd, void *buf, size_t count) {
	NOT_USED(fd);
	NOT_USED(buf);
	NOT_USED(count);
	return 0;
}

void *malloc(size_t size) {
	NOT_USED(size);
	return 0x0;
}

void __cxa_pure_virtual(void) { 
	//printf("__cxa_pure_virtual called\r\n");
	while (1); 
}
