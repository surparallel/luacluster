#include "sds.h"
sds sdscatsrlen(sds s, const char* src, size_t srclen, const char* search, size_t searchlen, const char* replace, size_t replacelen);
int sdsull2str(char* s, unsigned long long v);
int double2str(double value, int decimals, char* str, size_t len);
int sdsll2str(char* s, long long value);