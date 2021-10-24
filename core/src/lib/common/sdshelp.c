#include "sdshelp.h"
#include "plateform.h"

sds sdscatsrlen(sds s, const char* src, size_t srclen, const char* search, size_t searchlen, const char* replace, size_t replacelen) {
    size_t initlen = sdslen(s);
    const char* f = src;
    size_t slen = 0;
    size_t i = initlen;/* Position of the next byte to write to dest str. */

    while (*f) {

        if ((srclen - slen) >= searchlen && 0 == memcmp(f, search, searchlen)) {

            if (sdsavail(s) < replacelen) {
                s = sdsMakeRoomFor(s, replacelen);
            }

            memcpy(s + i, replace, replacelen);
            sdsinclen(s, replacelen);
            f += searchlen;
            i += replacelen;
            slen += searchlen;   
        }
        else {
            /* Make sure there is always space for at least 1 char. */
            if (sdsavail(s) == 0) {
                s = sdsMakeRoomFor(s, 1);
            }

            s[i++] = *f;
            sdsinclen(s, 1);
            slen += 1;
            f++;
        }
    }

    /* Add null-term */
    s[i] = '\0';
    return s;
}