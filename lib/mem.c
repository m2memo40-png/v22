#include "lib/mem.h"
void *memset(void *dst, int value, size_t count) { unsigned char *d = dst; for (size_t i=0; i<count; ++i) d[i]=(unsigned char)value; return dst; }
void *memcpy(void *dst, const void *src, size_t count) { unsigned char *d=dst; const unsigned char *s=src; for (size_t i=0; i<count; ++i) d[i]=s[i]; return dst; }
int memcmp(const void *a, const void *b, size_t count) { const unsigned char *x=a,*y=b; for(size_t i=0;i<count;++i) { if(x[i]!=y[i]) return x[i]<y[i]?-1:1; } return 0; }
