#include "reglan.h"

static char *type_to_str(enum EExprType type);
static char *ll_to_str(long long num);
static void print_indent(int indent);
static void backref_print(struct SBackref *p, int indent);
static void charset_print(struct SCharset *p, int indent);
static void words_print(struct SWords *p, int indent);
static void concat_print(struct SConcat *p, int indent);
static void alter_print(struct SAlter *p, int indent);


static char *type_to_str(enum EExprType type) {
    static char buf[256];
    switch (type) {
        case TBackref:
            return "backref";
        case TCharset:
            return "charset";
        case TWords:
            return "wordset";
        case TConcat:
            return "concat";
        case TAlter:
            return "alter";
        default:
            sprintf(buf, "UNKNOWN<%d>", (int)type);
            return buf;
    }
}

static char *ll_to_str(long long num) {
    static char buf[256];
    if (num == UNLIMITED)
        return "oo";
    sprintf(buf, "%lld", num);
    return buf;
}

static void print_indent(int indent) {
    int i;
    for (i = 0; i < indent; i++)
        putchar(' ');
}

static void backref_print(struct SBackref *p, int indent) {
    print_indent(indent);
    printf("--> #%d\n", p->num);
}

static void charset_print(struct SCharset *p, int indent) {
    int i;
    print_indent(indent);
    printf("[%d] {", p->count);
    for (i = 0; i < p->count; i++)
        putchar(p->chars[i]);
    printf("}\n");
}

static void words_print(struct SWords *p, int indent) {
    int i, length;
    print_indent(indent);
    printf("[%d] <%s> {", p->count, p->fname);
    length = indent + 15 + strlen(p->fname);
    for (i = 0; i < p->count; i++) {
        if (i > 0) {
            length += 2;
            printf(", ");
        }
        length += strlen(p->words[i]);
        if (length > 80) {
            printf("...");
            break;
        }
        printf("%s", p->words[i]);
    }
    printf("}\n");
}

static void concat_print(struct SConcat *p, int indent) {
    int i;
    for (i = 0; i < p->count; i++)
        regexp_print(&p->exprs[i], indent);
}

static void alter_print(struct SAlter *p, int indent) {
    int i;
    for (i = 0; i < p->count; i++)
        regexp_print(&p->exprs[i], indent);
}

void regexp_print(struct SRegexpr *p, int indent) {
    print_indent(indent);
    printf("<%s>", type_to_str(p->type));
    printf("{%s,", ll_to_str(p->min_count));
    printf("%s} ", ll_to_str(p->max_count));
    printf("[%s]", ll_to_str(p->full_length));
    if (p->ngroup > 0)
        printf(" #%d", p->ngroup);
    switch (p->type) {
        case TBackref:
            backref_print(&p->v.ref, 2);
            break;
        case TCharset:
            charset_print(&p->v.set, 2);
            break;
        case TWords:
            words_print(&p->v.words, 2);
            break;
        case TConcat:
            printf("\n");
            concat_print(&p->v.concat, indent + 2);
            break;
        case TAlter:
            printf("\n");
            alter_print(&p->v.alter, indent + 2);
            break;
            
    }
}

void alteration_print(struct SAlteration *p, int indent) {
    int i;
    print_indent(indent);
    printf("Alteration <%s> := ", type_to_str(p->src->type));
    switch (p->src->type) {
        case TBackref:
            printf(" --> #%d\n", p->src->v.ref.num);
            break;
        case TCharset:
            printf("'%s' [%d]\n", p->src->v.set.chars, p->ptr);
            break;
        case TWords:
            printf("<%s>{%d} [%d]\n", p->src->v.words.fname, p->src->v.words.count, p->ptr);
            break;
        case TConcat:
            printf("<CONCAT?> must not be\n");
            break;
        case TAlter:
            printf(" {%d} [%d]\n", p->count, p->ptr);
            for (i = 0; i < p->count; i++)
                concatenation_print(&p->concats[i], indent + 2);
            break;
        default:
            printf(" (nil)\n");
    }
}

void concatenation_print(struct SConcatenation *p, int indent) {
    int i;
    print_indent(indent);
    printf("Concatenation := {%d} [%d+] ", p->count, p->min_length);
    if (p->overflowed)
        printf(" [OVR] ");
    for (i = 0; i < p->count; i++) {
        printf("(+ %d/%d) ", p->added[i], p->maxs[i]);
    }
    printf("\n");
    for (i = 0; i < p->count; i++) {
        alteration_print(&p->alters[i], indent + 2);
    }
}
