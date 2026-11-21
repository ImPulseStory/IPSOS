#ifndef IPSOS_UNISTD_H
#define IPSOS_UNISTD_H

#include <stddef.h>
#include <sys/types.h>

#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2
#define F_OK 0
#define X_OK 1
#define W_OK 2
#define R_OK 4

ssize_t read(int fd, void *buffer, size_t count);
ssize_t write(int fd, const void *buffer, size_t count);
int close(int fd);
off_t lseek(int fd, off_t offset, int whence);
int unlink(const char *path);
int access(const char *path, int mode);
int isatty(int fd);
char *getcwd(char *buffer, size_t size);
int chdir(const char *path);
char *realpath(const char *path, char *resolved_path);
int usleep(unsigned int microseconds);
unsigned int sleep(unsigned int seconds);

#endif
