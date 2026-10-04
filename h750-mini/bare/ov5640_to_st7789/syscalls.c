/**
  * @file    syscalls.c
  * @brief   Minimal newlib retarget layer for the bare-metal STM32H750 build.
  *
  * _write() is redirected to USART1 via main.c's __io_putchar() so printf()
  * output from the ov5640_to_st7789 application is visible on the UART.
  */

#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdint.h>

/* Provided by main.c (PUTCHAR_PROTOTYPE, __GNUC__ branch). */
extern int __io_putchar(int ch);

/* Linker-provided end of static data / start of heap. */
extern char end[];
extern char _estack[];

#define HEAP_LIMIT ((char *)((uintptr_t)_estack - 0x1000))   /* leave room for the stack */

#ifdef __cplusplus
extern "C" {
#endif

void _init(void)
{
}
void _fini(void)
{
}

void _exit(int status)
{
    (void)status;
    while (1)
    {
    }
}

int _close(int file)
{
    (void)file;
    return -1;
}

int _fstat(int file, struct stat *st)
{
    (void)file;
    st->st_mode = S_IFCHR;
    return 0;
}

int _isatty(int file)
{
    (void)file;
    return 1;
}

int _lseek(int file, int ptr, int dir)
{
    (void)file;
    (void)ptr;
    (void)dir;
    return 0;
}

int _read(int file, char *ptr, int len)
{
    (void)file;
    (void)ptr;
    (void)len;
    errno = ENOSYS;
    return -1;
}

/* printf()/putchar() output -> USART1. */
int _write(int file, char *ptr, int len)
{
    int i;
    (void)file;
    for (i = 0; i < len; i++)
    {
        __io_putchar((unsigned char)ptr[i]);
    }
    return len;
}

int _getpid(void)
{
    return 1;
}

int _kill(int pid, int sig)
{
    (void)pid;
    (void)sig;
    errno = EINVAL;
    return -1;
}

void *_sbrk(int incr)
{
    static char *heap_end = 0;
    char *prev_heap_end;

    if (heap_end == 0)
    {
        heap_end = end;
    }
    prev_heap_end = heap_end;

    if (heap_end + incr > HEAP_LIMIT)
    {
        errno = ENOMEM;
        return (void *)-1;
    }
    heap_end += incr;
    return prev_heap_end;
}

#ifdef __cplusplus
}
#endif
