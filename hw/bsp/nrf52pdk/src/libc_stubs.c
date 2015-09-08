void * _sbrk(int c);
int _close(int fd);
int _fstat(int fd, void *s);
void _exit(int s);
int _kill(int pid, int sig);
int _write(int fd, void *b, int nb);
int _isatty(int c);
int _lseek(int fd, int off, int w);
int _read(int fd, void *b, int nb);
int _getpid(void);

int 
_close(int fd)
{
    return -1;
}

int
_fstat(int fd, void *s)
{
    return -1;
}


void 
_exit(int s)
{
    while (1) {} 
}

int 
_kill(int pid, int sig)
{
    return -1;
}

int 
_write(int fd, void *b, int nb) 
{
    return -1;
}

int 
_isatty(int c)
{
    return -1;
}

int 
_lseek(int fd, int off, int w)
{
    return -1;
}

int
_read(int fd, void *b, int nb)
{
    return -1;
}

int 
_getpid(void) {
    return -1;
}
