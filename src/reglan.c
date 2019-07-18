#include "reglan.h"

#include <unistd.h>

void usage(char *execname) {
    printf("Usage: %s [-v] [-h] [-u] [-p] [-d] [-c] [-o <offset>] [-n <max_number>] [-b <buffer_size] [<regexpr>]*\n", execname);
    printf("   -v       print version\n");
    printf("   -h, -u   print usage (this info)\n");
    printf("   -p       print parsed expression\n");
    printf("   -d       print debug info, i.e. expression template state for each word\n");
    printf("   -c       do not print generated words, buf print their total count after generation finishs\n");
    printf("   -o <N>   skip <N> words from the beginning (default - do not skip anything)\n");
    printf("   -n <N>   limit generated words count to this value (default - unlimited)\n");
    printf("   -b <S>   set maximum word length to this value (default - 1023 bytes)\n");
}

int main(int argc, char* argv[]) {
    long long i, max_n = UNLIMITED, offset = 0;
    int bufsize = 1024;
    int count = 0, print = 0, debug = 0;
    
    char *buffer;
    
    int opt;
    while ((opt = getopt(argc, argv, "vhupdco:n:b:")) != -1) {
        switch (opt) {
            case 'v':
                printf("%s version %s\n", argv[0], VERSION);
                break;
            case 'h':
            case 'u':
                usage(argv[0]);
                exit(0);
                break;
            case 'p':
                print = 1;
                break;
            case 'd':
                debug = 1;
                break;
            case 'c':
                count = 1;
                break;
            case 'o':
                offset = atoll(optarg);
                break;
            case 'n':
                max_n = atoll(optarg);
                break;
            case 'b':
                bufsize = atoi(optarg);
                break;
            default:
                usage(argv[0]);
        }
    }
    
    buffer = (char*)malloc(bufsize);
    
    for (; optind < argc; optind++) {
        RegExp regexp;
        Node root;
        PNode fast_inc = NULL;
        
        parse(argv[optind], &regexp, &root);
        if (print)
            regexp_print(&regexp, 0);
        
        if (offset)
            node_set_offset(&root, offset);
        
        for (i = 1; max_n == UNLIMITED || i <= max_n; i++) {
            if (debug)
                node_print(&root, 0);
            
            node_value(&root, buffer, bufsize-1);
            
            if (!count)
                printf("%s\n", buffer);
            
            if (fast_inc)
                if (!node_inc_inplace(fast_inc))
                    fast_inc = NULL;
            if (!fast_inc) {
                fast_inc = node_inc(&root);
                if (!fast_inc)
                    break;
            }
        }
        if (count)
            printf("%lld\n", i);
        
        node_free(&root);
        regexp_free(&regexp);
    }
    
    return 0;
}
