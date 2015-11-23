#include "unittests.h"

int main()
{
    int status = 0;
    
    {
        COMMENT("Testing basic snprintf functionality");
        char buf[20];
        
        snprintf(buf, sizeof(buf), "%08d", 55);
        TEST(strcmp(buf, "00000055") == 0);
        
        TEST(snprintf(buf, sizeof(buf), "01234567890123456789") == 20);
        TEST(strcmp(buf, "0123456789012345678") == 0);
    }
        
    if (status != 0)
        fprintf(stdout, "\n\nSome tests FAILED!\n");

    return status;
}
