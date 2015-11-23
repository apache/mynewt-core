#include <stdio.h>

size_t memfile_write(FILE *instance, const char *bp, size_t n)
{
    struct MemFile *f = (struct MemFile*)instance;
    size_t i = 0;
    
    while (n--)
    {
        f->bytes_written++;
        if (f->bytes_written <= f->size)
        {
            *f->buffer++ = *bp++;
            i++;
        }
    }
    
    return i;
}

const struct File_methods MemFile_methods = {
    &memfile_write,
    NULL
};

FILE *fmemopen_w(struct MemFile* storage, char *buffer, size_t size)
{
    storage->file.vmt = &MemFile_methods;
    storage->buffer = buffer;
    storage->bytes_written = 0;
    storage->size = size;
    return (FILE*)storage;
}
