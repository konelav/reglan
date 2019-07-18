#ifndef _REGLAN_H_INCLUDED
#define _REGLAN_H_INCLUDED 1

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define VERSION     "0.2.2"

#define UNLIMITED   (-1)
#define MIN_CHAR    (32)
#define MAX_CHAR    (128)

#define LIST_INIT(pList, Type) { \
    (pList)->Type##_capacity = 4; \
    (pList)->Type##_length = 0; \
    (pList)->Type = calloc((pList)->Type##_capacity, sizeof(*((pList)->Type))); \
}

#define LIST_ENSURE_CAPACITY(pList, Type, Capacity) { \
    if ((pList)->Type##_capacity < (Capacity)) { \
        (pList)->Type##_capacity = (Capacity); \
        (pList)->Type = realloc((pList)->Type, (Capacity) * sizeof(*((pList)->Type))); \
    } \
}

#define LIST_ENLARGE(pList, Type) { \
    if ((pList)->Type##_capacity < 128) \
        (pList)->Type##_capacity *= 2; \
    else \
        (pList)->Type##_capacity += 128; \
    (pList)->Type = realloc((pList)->Type, ((pList)->Type##_capacity) * sizeof(*((pList)->Type))); \
}

#define LIST_APPEND(pList, Type, Value) { \
    if ((pList)->Type##_capacity == (pList)->Type##_length) { \
        LIST_ENLARGE((pList), Type); \
    } \
    (pList)->Type[(pList)->Type##_length] = Value; \
    (pList)->Type##_length += 1; \
}

#ifdef DEBUG
#define PRINT_DBG   printf
#else
#define PRINT_DBG   (void)
#endif

struct SOption;
struct SBranch;
struct SRegExp;
struct SNode;
struct SAlternative;
struct SBackrefs;

typedef struct SOption {
    struct SOption *backref;
    char *chars;
    int chars_length;
    struct SRegExp *regexp;
    int min_count;
    int max_count;
    int ngroup, refnum;
    struct SNode *last_node;
    
    long long full_length;
} Option, *POption;

typedef struct SBranch {
    POption options;
    int options_capacity;
    int options_length;
} Branch, *PBranch;

typedef struct SRegExp {
    PBranch branches;
    int branches_capacity;
    int branches_length;
    char **words;
    int words_count;
} RegExp, *PRegExp;

typedef struct SNode {
    POption backref;
    PRegExp regexp;
    char *str;
    int str_length;
    struct SAlternative *alternatives;
    int alternatives_capacity;
    int alternatives_length;
    int ptr;
    
    char *last_dst;
    int last_length;
    
    long long full_length;
} Node, *PNode;

typedef struct SAlternative {
    PBranch src;
    struct SNode *nodes;
    int nodes_capacity;
    int nodes_length;
    int min_length;
    int *maxs, *added;
    int overflowed;
    
    long long full_length;
} Alternative, *PAlternative;


char *parse_range(char *src, int *min_count, int *max_count);
char *parse_escaped_sequence(char *src, char **set, int *set_length);
char *parse_set(char *src, char **set, int *set_length);
char *parse_dict(char *src, PRegExp words);
char *parse_expr(char *src, PRegExp branches, int total_groups);
void link_backrefs(PRegExp regexp);
void parse(char *src, PRegExp regexp, PNode root);
void regexp_free(PRegExp regexp);

long long ll_add(long long a, long long b);
long long ll_mul(long long a, long long b);

void node_free(PNode node);
void node_init(PNode node, POption backref, char *str, int str_length, PRegExp regexp);
void node_reset(PNode node);
PNode node_inc(PNode node);
void node_set_offset(PNode node, long long offset);
int node_value(PNode node, char *dst, int max_length);
int node_inc_inplace(PNode node);

void alternative_free(PAlternative alternative);
void alternative_init(PAlternative alternative, PBranch branch);
void alternative_reset(PAlternative alternative);
int alternative_set_length(PAlternative alternative, int length);
void alternative_init_nodes(PAlternative alternative);
PNode alternative_inc(PAlternative alternative);
void alternative_set_offset(PAlternative alternative, long long offset);
int alternative_value(PAlternative alternative, char *dst, int max_length);

void option_print(POption option, int indent);
void branch_print(PBranch branch, int indent);
void regexp_print(PRegExp regexp, int indent);
void node_print(PNode node, int indent);
void alternative_print(PAlternative alternative, int indent);

#endif  // _REGLAN_H_INCLUDED
