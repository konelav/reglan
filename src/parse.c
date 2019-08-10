#include "reglan.h"

static void chars_to_set(char *chars, int chars_length, char **set, int *set_length);
static void make_full_set(char **set, int *set_length);
static int parse_hex(char ch);
static char *parse_range(char *src, int *min_count, int *max_count);
static char *parse_escaped_sequence(char *src, char **set, int *set_length);
static char *parse_set(char *src, char **set, int *set_length);
static char *parse_words(char *src, char **fname, char ***words, int *count);

static void find_backrefs(struct SRegexpr *p, struct SRegexpr *backrefs[]);
static void set_backrefs(struct SRegexpr *p, struct SRegexpr *backrefs[]);

static void chars_to_set(char *chars, int chars_length, char **set, int *set_length) {
    int i;
    
    *set_length = 0;
    for (i = 0; i < chars_length; i++)
        if (chars[i])
            *set_length += 1;
    
    *set = (char*)calloc(*set_length + 1, 1);
    *set_length = 0;
    for (i = 0; i < chars_length; i++)
        if (chars[i]) {
            (*set)[*set_length] = i;
            *set_length += 1;
        }
}

static void make_full_set(char **set, int *set_length) {
    char all_chars[MAX_CHAR];
    int i;
    
    memset(all_chars, 0, sizeof(all_chars));
    for (i = MIN_CHAR; i < MAX_CHAR; i++)
        all_chars[i] = 1;
    
    chars_to_set(all_chars, sizeof(all_chars), set, set_length);
}

static int parse_hex(char ch) {
    if ('0' <= ch && ch <= '9')
        return (ch - '0');
    if ('A' <= ch && ch <= 'F')
        return (ch - 'A' + 10);
    if ('a' <= ch && ch <= 'f')
        return (ch - 'a' + 10);
    PRINT_ERR("Can't parse hex char '%c'\n", ch);
    exit(-1);
    return 0;
}

static char *parse_range(char *src, int *min_count, int *max_count) {
    char *end;
    char *comma;
    
    if (!(end = strchr(src, '}'))) {
        PRINT_ERR("Can't find enclosing char }\n");
        exit(-1);
    }
    
    *min_count = atoi(src + 1);
    
    comma = strchr(src, ',');
    if (!comma || comma > end)
        *max_count = *min_count;
    else {
        if (comma + 1 == end) // "{<min>,}"
            *max_count = UNLIMITED;
        else
            *max_count = atoi(comma + 1);
    }
    
    PRINT_DBG("Parsed range <%s>: %d .. %d\n", src, *min_count, *max_count);
    
    return end;
}

static char *parse_escaped_sequence(char *src, char **set, int *set_length) {
    int index = 1;
    char ch = src[index];
    switch (src[index]) {
        default:
            break;  // just output specified character
        case 'd':
            parse_set("[0-9]", set, set_length);
            return src + index;
        case 'D':
            parse_set("[^0-9]", set, set_length);
            return src + index;
        case 's':
            parse_set("[ \\t\\n\\r\\f\\v]", set, set_length);
            return src + index;
        case 'S':
            parse_set("[^ \\t\\n\\r\\f\\v]", set, set_length);
            return src + index;
        case 'w':
            parse_set("[a-zA-Z0-9_]", set, set_length);
            return src + index;
        case 'W':
            parse_set("[^a-zA-Z0-9_]", set, set_length);
            return src + index;
        case 'x':
            if (src[index+1] && src[index+2]) {
                ch = (parse_hex(src[index+1]) << 4) |
                     (parse_hex(src[index+2]) << 0) ;
                index += 2;
            }
            break;
        case 't':
            ch = '\t';
            break;
        case 'r':
            ch = '\r';
            break;
        case 'n':
            ch = '\n';
            break;
        case 'f':
            ch = '\f';
            break;
        case 'v':
            ch = '\v';
            break;
    }
    *set_length = 1;
    *set = (char*)calloc(*set_length + 1, 1);
    (*set)[0] = ch;
    
    return src + index;
}

static char *parse_set(char *src, char **set, int *set_length) {
    int index = 1;
    int negate;
    int prev_ch = -1;
    int i;
    char all_chars[MAX_CHAR];
    
    if (src[index] == '^') {
        negate = 1;
        index++;
    }
    else {
        negate = 0;
    }
    
    memset(all_chars, 0, sizeof(all_chars));
    
    for (; src[index]; index++) {
        int ch = src[index];
        if (ch == '\\') {
            char *subset;
            int subset_length, i;
            char *end = parse_escaped_sequence(src + index, &subset, &subset_length);
            for (i = 0; i < subset_length; i++)
                all_chars[(unsigned int)subset[i]] = 1;
            PRINT_DBG("Escaped sequence added: [%d] '%s'\n", subset_length, subset);
            free(subset);
            index = end - src;
        }
        else if (ch == '-' && prev_ch >= 0) {
            ch = src[++index];
            for (i = prev_ch; i <= ch; i++)
                all_chars[i] = 1;
            PRINT_DBG("Char range: '%c' .. '%c'\n", prev_ch, ch);
        }
        else if (ch == ']') {
            break;
        }
        else {
            all_chars[ch] = 1;
        }
        prev_ch = ch;
    }
    
    if (!src[index]) {
        PRINT_ERR("Can't find matching ']'\n");
        exit(-1);
    }
    
    if (negate) {
        PRINT_DBG("Negating charset\n");
        for (i = MIN_CHAR; i < MAX_CHAR; i++)
            all_chars[i] = 1 - all_chars[i];
    }
        
    chars_to_set(all_chars, sizeof(all_chars), set, set_length);
    
    return src + index;
}

static char *parse_words(char *src, char **fname, char ***words, int *count) {
    char buffer[1024];
    FILE *fdict;
    char *end;
    int i, j;
    
    end = strchr(src, ')');
    if (end == NULL)
        end = src + strlen(src);
    i = end - src;
    if (i >= sizeof(buffer))
        i = sizeof(buffer) - 1;
    strncpy(buffer, src, i);
    buffer[i] = '\x00';
    
    *fname = (char*)malloc(i + 1);
    memcpy(*fname, buffer, i + 1);
    
    fdict = fopen(buffer, "r");
    
    for (i = 0; ;) {
        int ch = fgetc(fdict);
        if (ch == EOF)
            break;
        if (ch == '\n')
            i++;
    }
    
    *count = i;
    *words = (char**)malloc(*count * sizeof(char *));
    fseek(fdict, 0, SEEK_SET);
    for (i = 0, j = 0; i < *count;) {
        int ch = fgetc(fdict);
        if (ch == EOF)
            break;
        if (ch == '\n') {
            (*words)[i] = (char*)malloc(j + 1);
            memcpy((*words)[i], buffer, j);
            (*words)[i][j] = '\x00';
            j = 0;
            i++;
        }
        else if (j < sizeof(buffer)) {
            buffer[j++] = ch;
        }
    }
    
    fclose(fdict);
    
    PRINT_DBG("Loaded dictionary <%s>, total %d word(s)\n", src, *count);
    
    return end;
}

char *parse_expr(char *src, struct SRegexpr *p, int *total_groups) {
    int index;
    struct SRegexpr sub;
    
    memset(p, 0, sizeof(struct SRegexpr));
    p->min_count = p->max_count = 1;
    memset(&sub, 0, sizeof(struct SRegexpr));
    sub.min_count = sub.max_count = 1;
    
    p->type = TAlter;
    LIST_INIT(&p->v.alter, exprs);
    
    sub.type = TConcat;
    LIST_INIT(&sub.v.concat, exprs);
    
    for (index = 0; src[index]; index++) {
        int ch = src[index];
        char *end;
        if (ch == '.') {
            struct SRegexpr re;
            memset(&re, 0, sizeof(re));
            re.min_count = re.max_count = 1;
            
            re.type = TCharset;
            make_full_set(&re.v.set.chars, &re.v.set.count);
            re.min_count = re.max_count = 1;
            re.full_length = re.v.set.count;
            LIST_APPEND(&sub.v.concat, exprs, re);
            PRINT_DBG("Fullset added\n");
        }
        else if (ch == '[') {
            struct SRegexpr re;
            memset(&re, 0, sizeof(re));
            re.min_count = re.max_count = 1;
            
            re.type = TCharset;
            end = parse_set(src + index, &re.v.set.chars, &re.v.set.count);
            re.full_length = re.v.set.count;
            LIST_APPEND(&sub.v.concat, exprs, re);
            index = end - src;
            PRINT_DBG("Charset added\n");
        }
        else if (ch == '\\') {
            struct SRegexpr re;
            memset(&re, 0, sizeof(re));
            re.min_count = re.max_count = 1;
            
            if ('1' <= src[index+1] && src[index+1] <= '9') { // backreference
                re.type = TBackref;
                re.v.ref.num = src[index+1] - '0';
                PRINT_DBG("Backreference #%d added\n", re.v.ref.num);
                index++;
            }
            else {
                re.type = TCharset;
                end = parse_escaped_sequence(src + index, &re.v.set.chars, &re.v.set.count);
                index = end - src;
                PRINT_DBG("Escaped sequence added\n");
            }
            LIST_APPEND(&sub.v.concat, exprs, re);
        }
        else if (ch == '*' && sub.v.concat.count > 0) {
            int last = sub.v.concat.count - 1;
            sub.v.concat.exprs[last].min_count = 0;
            sub.v.concat.exprs[last].max_count = UNLIMITED;
            PRINT_DBG("Asterisk added\n");
        }
        else if (ch == '+' && sub.v.concat.count > 0) {
            int last = sub.v.concat.count - 1;
            sub.v.concat.exprs[last].min_count = 1;
            sub.v.concat.exprs[last].max_count = UNLIMITED;
            sub.v.concat.exprs[last].max_count = UNLIMITED;
            PRINT_DBG("Plus added\n");
        }
        else if (ch == '?' && sub.v.concat.count > 0) {
            int last = sub.v.concat.count - 1;
            sub.v.concat.exprs[last].min_count = 0;
            sub.v.concat.exprs[last].max_count = 1;
            PRINT_DBG("Question added\n");
        }
        else if (ch == '{' && sub.v.concat.count > 0) {
            int last = sub.v.concat.count - 1;
            char *end = parse_range(src + index, 
                &sub.v.concat.exprs[last].min_count, 
                &sub.v.concat.exprs[last].max_count);
            index = end - src;
            PRINT_DBG("Quant-parens added [%d .. %d]\n", 
                sub.v.concat.exprs[last].min_count, 
                sub.v.concat.exprs[last].max_count);
        }
        else if (ch == '|') {
            LIST_APPEND(&p->v.alter, exprs, sub);
            LIST_INIT(&sub.v.concat, exprs);
            PRINT_DBG("Alternative added\n");
        }
        else if (ch == '(') {
            int ngroup;
            struct SRegexpr re;
            memset(&re, 0, sizeof(re));
            re.min_count = re.max_count = 1;
            
            ngroup = *total_groups + 1;
            end = NULL;
            if (src[index+1] == '?') {  // special syntax
                index++;
                switch (src[index+1]) {
                    case ':':
                        index++;
                    default:
                        ngroup = 0;
                        break;
                    case 'F':
                        re.type = TWords;
                        end = parse_words(src + index + 2, &re.v.words.fname, 
                            &re.v.words.words, &re.v.words.count);
                        PRINT_DBG("Dictionary added\n");
                        break;
                }
            }
            if (ngroup)
                (*total_groups)++;
            if (!end)
                end = parse_expr(src + index + 1, &re, total_groups);
            re.ngroup = ngroup;
            
            LIST_APPEND(&sub.v.concat, exprs, re);
            index = end - src;
            PRINT_DBG("Subexpr added\n");
        }
        else if (ch == ')') {
            PRINT_DBG("End of subexpr detected\n");
            break;
        }
        else {
            struct SRegexpr re;
            memset(&re, 0, sizeof(re));
            re.min_count = re.max_count = 1;
            re.type = TCharset;
            re.v.set.count = 1;
            re.v.set.chars = (char*)calloc(1, 1);
            re.v.set.chars[0] = ch;
            LIST_APPEND(&sub.v.concat, exprs, re);
            PRINT_DBG("Just a char added '%c'\n", ch);
        }
    }
    if (sub.v.concat.count > 0) {
        LIST_APPEND(&p->v.alter, exprs, sub);
    }
    return src + index;
}

static void find_backrefs(struct SRegexpr *p, struct SRegexpr *backrefs[]) {
    int g = p->ngroup;
    if (1 <= g && g <= 9) {
        if (backrefs[g] && backrefs[g] != p) {
            PRINT_ERR("Duplicate backref #%d: <%p> =/= <%p>\n", g, backrefs[g], p);
        }
        backrefs[g] = p;
    }
    if (p->type == TAlter) {
        int i;
        for (i = 0; i < p->v.alter.count; i++)
            find_backrefs(&p->v.alter.exprs[i], backrefs);
    }
    else if (p->type == TConcat) {
        int i;
        for (i = 0; i < p->v.concat.count; i++)
            find_backrefs(&p->v.concat.exprs[i], backrefs);
    }
}

static void set_backrefs(struct SRegexpr *p, struct SRegexpr *backrefs[]) {
    if (p->type == TBackref) {
        int r = p->v.ref.num;
        if (1 <= r && r <= 9) {
            if (!backrefs[r]) {
                PRINT_ERR("Undefined backref #%d\n", r);
                exit(-1);
            }
            else {
                p->v.ref.expr = backrefs[r];
            }
        }
    }
    else if (p->type == TAlter) {
        int i;
        for (i = 0; i < p->v.alter.count; i++)
            set_backrefs(&p->v.alter.exprs[i], backrefs);
    }
    else if (p->type == TConcat) {
        int i;
        for (i = 0; i < p->v.concat.count; i++)
            set_backrefs(&p->v.concat.exprs[i], backrefs);
    }
}

void link_backrefs(struct SRegexpr *p) {
    struct SRegexpr *backrefs[10];
    memset(backrefs, 0, sizeof(backrefs));
    find_backrefs(p, backrefs);
    set_backrefs(p, backrefs);
}

void calc_full_length(struct SRegexpr *p) {
    switch (p->type) {
        case TBackref:
            p->full_length = 0;
            break;
        case TCharset:
            p->full_length = p->v.set.count;
            break;
        case TWords:
            p->full_length = p->v.words.count;
            break;
        case TConcat:
            {
                int i, j, k;
                p->full_length = 1;
                for (i = 0; i < p->v.concat.count; i++) {
                    struct SRegexpr *sub = &p->v.concat.exprs[i];
                    calc_full_length(sub);
                    if (sub->full_length == UNLIMITED && sub->max_count != 0)
                        p->full_length = UNLIMITED;
                    else if (sub->full_length != 0 && sub->max_count == UNLIMITED)
                        p->full_length = UNLIMITED;
                    else if (sub->full_length != 0) {
                        long long all_counts_len = 0;
                        for (j = sub->min_count; j <= sub->max_count; j++) {
                            long long fixed_count_length = 1;
                            for (k = 0; k < j; k++)
                                fixed_count_length = ll_mul(fixed_count_length, sub->full_length);
                            all_counts_len = ll_add(all_counts_len, fixed_count_length);
                        }
                        p->full_length = ll_mul(p->full_length, all_counts_len);
                    }
                    if (p->full_length == UNLIMITED)
                        break;
                }
            }
            break;
        case TAlter:
            {
                int i;
                p->full_length = 0;
                for (i = 0; i < p->v.alter.count; i++) {
                    struct SRegexpr *sub = &p->v.alter.exprs[i];
                    calc_full_length(sub);
                    p->full_length = ll_add(p->full_length, sub->full_length);
                    if (p->full_length == UNLIMITED)
                        break;
                }
            }
            break;
    }
}

void parse(char *src, struct SRegexpr *p, struct SAlteration *root) {
    int total_groups = 0;
    parse_expr(src, p, &total_groups);
    link_backrefs(p);
    calc_full_length(p);
    alteration_init(root, p);
}

void regexp_free(struct SRegexpr *p) {
    int i;
    switch (p->type) {
        case TBackref:
            break;
        case TCharset:
            free(p->v.set.chars);
            break;
        case TWords:
            free(p->v.words.fname);
            free(p->v.words.words);
            break;
        case TConcat:
            for (i = 0; i < p->v.concat.count; i++)
                regexp_free(&p->v.concat.exprs[i]);
            free(p->v.concat.exprs);
            break;
        case TAlter:
            for (i = 0; i < p->v.alter.count; i++)
                regexp_free(&p->v.alter.exprs[i]);
            free(p->v.alter.exprs);
            break;
    }
}
