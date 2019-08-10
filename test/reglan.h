#ifndef _REGLAN_H_INCLUDED
#define _REGLAN_H_INCLUDED 1

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define VERSION     "0.3.0"

#define UNLIMITED   (-1)

#define BIGNUM      ((long long)0x7FFFFFFFFFFFFFFF)

#define MIN_CHAR    (32)
#define MAX_CHAR    (128)

#define LIST_INIT(pList, Type) { \
    (pList)->capacity = 4; \
    (pList)->count = 0; \
    (pList)->Type = calloc((pList)->capacity, sizeof(*((pList)->Type))); \
}

#define LIST_ENSURE_CAPACITY(pList, Type, Capacity) { \
    if ((pList)->capacity < (Capacity)) { \
        (pList)->capacity = (Capacity); \
        (pList)->Type = realloc((pList)->Type, (Capacity) * sizeof(*((pList)->Type))); \
    } \
}

#define LIST_ENLARGE(pList, Type) { \
    if ((pList)->capacity < 128) \
        (pList)->capacity *= 2; \
    else \
        (pList)->capacity += 128; \
    (pList)->Type = realloc((pList)->Type, ((pList)->capacity) * sizeof(*((pList)->Type))); \
}

#define LIST_APPEND(pList, Type, Value) { \
    if ((pList)->capacity == (pList)->count) { \
        LIST_ENLARGE((pList), Type); \
    } \
    (pList)->Type[(pList)->count] = Value; \
    (pList)->count += 1; \
}

#ifdef DEBUG
#define PRINT_DBG(...)   printf(__VA_ARGS__)
#else
#define PRINT_DBG(...)
#endif

#ifdef NOERRMSG
#define PRINT_ERR(...)
#else
#define PRINT_ERR(...)   fprintf(stderr, ##__VA_ARGS__)
#endif

struct SBackref;
struct SCharset;
struct SWords;
struct SConcat;
struct SAlter;

struct SRegexpr;

struct SAlteration;
struct SConcatenation;

enum EExprType {
    TBackref,
    TCharset,
    TWords,
    TConcat,
    TAlter
};

struct SBackref {
    int num;
    struct SRegexpr *expr;
};

struct SCharset {
    char *chars;
    int count;
};

struct SWords {
    char *fname;
    char **words;
    int count;
};

struct SConcat {
    struct SRegexpr *exprs;
    int capacity, count;
};

struct SAlter {
    struct SRegexpr *exprs;
    int capacity, count;
};

struct SRegexpr {
    enum EExprType type;
    union {
        struct SBackref ref;
        struct SCharset set;
        struct SWords words;
        struct SConcat concat;
        struct SAlter alter;
    } v;
    int min_count, max_count;
    int ngroup;
    
    struct SAlteration *last;
    long long full_length;
};

struct SAlteration {
    struct SRegexpr *src;
    
    struct SConcatenation *concats;
    int capacity, count;
    
    int ptr;
    
    char *last_dst;
    int last_length;
};

struct SConcatenation {
    struct SRegexpr *src;
    
    struct SAlteration *alters;
    int capacity, count;
    
    int min_length;
    int *maxs, *added;
    int overflowed;
};

/* arith.c */
long long ll_add(long long a, long long b);
long long ll_mul(long long a, long long b);

/* parse.c */
char *parse_expr(char *src, struct SRegexpr *p, int *total_groups);
void link_backrefs(struct SRegexpr *p);
void calc_full_length(struct SRegexpr *p);
void parse(char *src, struct SRegexpr *p, struct SAlteration *root);
void regexp_free(struct SRegexpr *p);

/* alteration.c */
void alteration_free(struct SAlteration *p);
void alteration_init(struct SAlteration *p, struct SRegexpr *re);
void alteration_reset(struct SAlteration *p);
struct SAlteration *alteration_inc(struct SAlteration *p);
void alteration_set_offset(struct SAlteration *p, long long offset);
int alteration_value(struct SAlteration *p, char *dst, int max_length);
int alteration_inc_inplace(struct SAlteration *p);

/* concatenation.c */
void concatenation_free(struct SConcatenation *p);
void concatenation_init(struct SConcatenation *p, struct SRegexpr *concat);
void concatenation_reset(struct SConcatenation *p);
int concatenation_set_length(struct SConcatenation *p, int length);
struct SAlteration *concatenation_inc(struct SConcatenation *p);
void concatenation_set_offset(struct SConcatenation *p, long long offset);
int concatenation_value(struct SConcatenation *p, char *dst, int max_length);

/* print.c */
void regexp_print(struct SRegexpr *p, int indent);
void alteration_print(struct SAlteration *p, int indent);
void concatenation_print(struct SConcatenation *p, int indent);

#endif  // _REGLAN_H_INCLUDED
