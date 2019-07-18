#include "reglan.h"

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
    return 0;
}

char *parse_range(char *src, int *min_count, int *max_count) {
    char *end;
    char *comma;
    
    if (!(end = strchr(src, '}'))) {
        PRINT_DBG("Can't find enclosing char }\n");
        return NULL;
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

char *parse_escaped_sequence(char *src, char **set, int *set_length) {
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

char *parse_set(char *src, char **set, int *set_length) {
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
    
    if (negate) {
        PRINT_DBG("Negating charset\n");
        for (i = MIN_CHAR; i < MAX_CHAR; i++)
            all_chars[i] = 1 - all_chars[i];
    }
        
    chars_to_set(all_chars, sizeof(all_chars), set, set_length);
    
    return src + index;
}

char *parse_dict(char *src, PRegExp words) {
    char buffer[1024];
    FILE *fdict;
    char *end;
    int i, j;
    
    words->branches = 0;
    words->branches_length = 0;
    words->branches_capacity = 0;
    
    end = strchr(src, ')');
    if (end == NULL)
        end = src + strlen(src);
    i = end - src;
    if (i >= sizeof(buffer))
        i = sizeof(buffer) - 1;
    strncpy(buffer, src, i);
    buffer[i] = '\x00';
    
    fdict = fopen(buffer, "r");
    
    for (i = 0; ;) {
        int ch = fgetc(fdict);
        if (ch == EOF)
            break;
        if (ch == '\n')
            i++;
    }
    
    words->words_count = i;
    words->words = (char**)malloc(words->words_count * sizeof(char *));
    fseek(fdict, 0, SEEK_SET);
    for (i = 0, j = 0; i < words->words_count;) {
        int ch = fgetc(fdict);
        if (ch == EOF)
            break;
        if (ch == '\n') {
            words->words[i] = (char*)malloc(j + 1);
            memcpy(words->words[i], buffer, j);
            words->words[i][j] = '\x00';
            j = 0;
            i++;
        }
        else if (j < sizeof(buffer)) {
            buffer[j++] = ch;
        }
    }
    
    fclose(fdict);
    
    PRINT_DBG("Loaded dictionary <%s>, total %d word(s)\n", src, words->words_count);
    
    return end;
}

char *parse_expr(char *src, PRegExp branches, int total_groups) {
    int index;
    Branch branch;
    
    LIST_INIT(&branch, options);
    LIST_INIT(branches, branches);
    branches->words = NULL;
    branches->words_count = 0;
    
    for (index = 0; src[index]; index++) {
        int ch = src[index];
        char *end;
        if (ch == '.') {
            Option option;
            memset(&option, 0, sizeof(option));
            make_full_set(&option.chars, &option.chars_length);
            option.min_count = option.max_count = 1;
            LIST_APPEND(&branch, options, option);
            PRINT_DBG("Fullset added\n");
        }
        else if (ch == '[') {
            Option option;
            memset(&option, 0, sizeof(option));
            end = parse_set(src + index, &option.chars, &option.chars_length);
            option.min_count = option.max_count = 1;
            LIST_APPEND(&branch, options, option);
            if (end)
                index = end - src;
            PRINT_DBG("Charset added\n");
        }
        else if (ch == '\\') {
            Option option;
            memset(&option, 0, sizeof(option));
            option.min_count = option.max_count = 1;
            if ('1' <= src[index+1] && src[index+1] <= '9') { // backreference
                option.refnum = src[index+1] - '0';
                PRINT_DBG("Backreference #%d added\n", option.refnum);
                index++;
            }
            else {
                end = parse_escaped_sequence(src + index, &option.chars, &option.chars_length);
                if (end)
                    index = end - src;
                PRINT_DBG("Escaped sequence added\n");
            }
            LIST_APPEND(&branch, options, option);
        }
        else if (ch == '*' && branch.options_length > 0) {
            int last = branch.options_length - 1;
            branch.options[last].min_count = 0;
            branch.options[last].max_count = UNLIMITED;
            PRINT_DBG("Asterisk added\n");
        }
        else if (ch == '+' && branch.options_length > 0) {
            int last = branch.options_length - 1;
            branch.options[last].min_count = 1;
            branch.options[last].max_count = UNLIMITED;
            PRINT_DBG("Plus added\n");
        }
        else if (ch == '?' && branch.options_length > 0) {
            int last = branch.options_length - 1;
            branch.options[last].min_count = 0;
            branch.options[last].max_count = 1;
            PRINT_DBG("Question added\n");
        }
        else if (ch == '{' && branch.options_length > 0) {
            int last = branch.options_length - 1;
            char *end = parse_range(src + index, 
                &branch.options[last].min_count, &branch.options[last].max_count);
            if (end)
                index = end - src;
            PRINT_DBG("Quant-parens added [%d .. %d]\n", branch.options[last].min_count, branch.options[last].max_count);
        }
        else if (ch == '|') {
            LIST_APPEND(branches, branches, branch);
            LIST_INIT(&branch, options);
            PRINT_DBG("Alternative added\n");
        }
        else if (ch == '(') {
            Option option;
            int isgroup = 1, parsed = 0;
            memset(&option, 0, sizeof(option));
            end = NULL;
            option.regexp = (PRegExp)calloc(1, sizeof(RegExp));
            if (src[index+1] == '?') {
                index++;
                switch (src[index+1]) {
                    case ':':
                        isgroup = 0;
                        index++;
                        break;
                    case 'F':
                        end = parse_dict(src + index + 2, option.regexp);
                        parsed = 1;
                        PRINT_DBG("Dictionary added\n");
                        break;
                    default:
                        isgroup = 0;
                        break;
                }
            }
            if (isgroup) {
                total_groups++;
                option.ngroup = total_groups;
            }
            if (!parsed)
                end = parse_expr(src + index + 1, option.regexp, total_groups);
            option.min_count = option.max_count = 1;
            LIST_APPEND(&branch, options, option);
            if (end)
                index = end - src;
            PRINT_DBG("Subexpr added\n");
        }
        else if (ch == ')') {
            PRINT_DBG("End of subexpr detected\n");
            break;
        }
        else {
            Option option;
            memset(&option, 0, sizeof(option));
            option.chars_length = 1;
            option.chars = (char*)calloc(1, 1);
            option.chars[0] = ch;
            option.min_count = option.max_count = 1;
            LIST_APPEND(&branch, options, option);
            PRINT_DBG("Just a char added '%c'\n", ch);
        }
    }
    if (branch.options_length > 0) {
        LIST_APPEND(branches, branches, branch);
    }
    return src + index;
}

static void find_backrefs(PRegExp regexp, POption backrefs[]) {
    int i, j;
    if (regexp->words)
        return;
    for (i = 0; i < regexp->branches_length; i++) {
        PBranch branch = &regexp->branches[i];
        for (j = 0; j < branch->options_length; j++) {
            POption option = &branch->options[j];
            int g = option->ngroup;
            if (1 <= g && g <= 9) {
                if (backrefs[g] && backrefs[g] != option) {
                    PRINT_DBG("Duplicate backref #%d: <%p> =/= <%p>\n", g, backrefs[g], option);
                }
                backrefs[g] = option;
            }
            if (option->regexp)
                find_backrefs(option->regexp, backrefs);
        }
    }
}

static void set_backrefs(PRegExp regexp, POption backrefs[]) {
    int i, j;
    if (regexp->words)
        return;
    for (i = 0; i < regexp->branches_length; i++) {
        PBranch branch = &regexp->branches[i];
        for (j = 0; j < branch->options_length; j++) {
            POption option = &branch->options[j];
            int r = option->refnum;
            if (1 <= r && r <= 9) {
                if (!backrefs[r]) {
                    PRINT_DBG("Undefined backref #%d\n", r);
                    option->backref = NULL;
                }
                else {
                    option->backref = backrefs[r];
                }
            }
            if (option->regexp)
                set_backrefs(option->regexp, backrefs);
        }
    }
}

void link_backrefs(PRegExp regexp) {
    POption backrefs[10];
    memset(backrefs, 0, sizeof(backrefs));
    find_backrefs(regexp, backrefs);
    set_backrefs(regexp, backrefs);
}

void parse(char *src, PRegExp regexp, PNode root) {
    parse_expr(src, regexp, 0);
    link_backrefs(regexp);
    node_init(root, NULL, NULL, 0, regexp);
}

void regexp_free(PRegExp regexp) {
    int i, j;
    if (regexp->words) {
        free(regexp->words);
    }
    else {
        for (i = 0; i < regexp->branches_length; i++) {
            PBranch branch = &regexp->branches[i];
            for (j = 0; j < branch->options_length; j++) {
                POption option = &branch->options[j];
                if (option->regexp)
                    regexp_free(option->regexp);
                else if (option->chars)
                    free(option->chars);
            }
            free(branch->options);
        }
        free(regexp->branches);
    }
}
