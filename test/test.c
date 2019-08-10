#include <stdlib.h>
#include <stdio.h>

#include "reglan.h"


int main() {
    struct SRegexpr re;
    struct SAlteration root;
    parse("[1-9]\\d*", &re, &root);
    regexp_print(&re, 0);
    alteration_print(&root, 0);
    return 0;
}
