#ifndef IPSOS_SETJMP_H
#define IPSOS_SETJMP_H
typedef unsigned int jmp_buf[6];
int setjmp(jmp_buf env) __attribute__((returns_twice));
void longjmp(jmp_buf env, int value) __attribute__((noreturn));
#endif
