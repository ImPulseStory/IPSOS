.text
.globl setjmp
.type setjmp, @function
setjmp:
    movl 4(%esp), %eax
    movl %ebx, 0(%eax)
    movl %esi, 4(%eax)
    movl %edi, 8(%eax)
    movl %ebp, 12(%eax)
    leal 4(%esp), %edx
    movl %edx, 16(%eax)
    movl (%esp), %edx
    movl %edx, 20(%eax)
    xorl %eax, %eax
    ret
.size setjmp, .-setjmp

.globl longjmp
.type longjmp, @function
longjmp:
    movl 4(%esp), %edx
    movl 8(%esp), %eax
    testl %eax, %eax
    jnz 1f
    movl $1, %eax
1:
    movl 0(%edx), %ebx
    movl 4(%edx), %esi
    movl 8(%edx), %edi
    movl 12(%edx), %ebp
    movl 16(%edx), %ecx
    movl 20(%edx), %edx
    movl %ecx, %esp
    jmp *%edx
.size longjmp, .-longjmp
