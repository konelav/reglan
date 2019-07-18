#include "reglan.h"


#define BIGNUM      ((long long)0x7FFFFFFFFFFFFFFF)


long long ll_add(long long a, long long b) {
    if (a == UNLIMITED || b == UNLIMITED)
        return UNLIMITED;
    if (a == 0)
        return b;
    if (b == 0)
        return a;
    if (a > BIGNUM - b) { // a+b > BIGNUM
        PRINT_DBG("[OVR_ADD] %lld > %lld - %lld\n", a, BIGNUM, b);
        return UNLIMITED;
    }
    return a + b;
}

long long ll_mul(long long a, long long b) {
    if (a == 0 || b == 0)
        return 0;
    if (a == UNLIMITED || b == UNLIMITED)
        return UNLIMITED;
    if (a > BIGNUM / b) { // a*b > BIGNUM
        PRINT_DBG("[OVR_MUL] %lld > %lld / %lld\n", a, BIGNUM, b);
        return UNLIMITED;
    }
    return a * b;
}
