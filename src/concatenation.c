#include "reglan.h"

static int fill_seq(int need_sum, int maxs[], int seq[], int length);
static int inc_seq(int maxs[], int seq[], int length);
static void concatenation_init_alters(struct SConcatenation *p);
static int concatenation_set_length(struct SConcatenation *p, int length);

static int fill_seq(int need_sum, int maxs[], int seq[], int length) {
    int i;
    for (i = 0; i < length; i++) {
        int d = maxs[i];
        if (d > need_sum)
            d = need_sum;
        seq[i] = d;
        need_sum -= d;
    }
    return (need_sum == 0) ? 1 : 0;
}

static int inc_seq(int maxs[], int seq[], int length) {
    int i, sum, s;
    for (i = 0, sum = 0; i < length; i++) {
        sum += seq[i];
    }
    for (s = sum;;) {
        for (i = 0; i < length; i++) {
            if (seq[i] < maxs[i]) {
                seq[i]++;
                s++;
                break;
            }
            s -= seq[i];
            seq[i] = 0;
        }
        if (i == length)  // not incremented
            return 0;
        if (s == sum)
            return 1;
    }
}

static int concatenation_set_length(struct SConcatenation *p, int length) {
    int i;
    int global_max = length - p->min_length;
    for (i = 0; i < p->src->v.concat.count; i++) {
        int max = p->src->v.concat.exprs[i].max_count;
        if (max == UNLIMITED)
            max = global_max;
        else
            max -= p->src->v.concat.exprs[i].min_count;
        p->maxs[i] = max;
    }
    if (!fill_seq(global_max, p->maxs, p->added, p->src->v.concat.count)) {
        return 0;
    }
    concatenation_init_alters(p);
    return (p->count == length) ? 1 : 0;
}

static void concatenation_init_alters(struct SConcatenation *p) {
    int i, j, length;
    for (i = 0; i < p->count; i++) {
        alteration_free(&p->alters[i]);
    }
    for (i = 0, length = 0; i < p->src->v.concat.count; i++) {
        struct SRegexpr *sub = &p->src->v.concat.exprs[i];
        int count = sub->min_count;
        count += p->added[i];
        length += count;
    }
    LIST_ENSURE_CAPACITY(p, alters, length);
    p->count = 0;
    for (i = 0; i < p->src->v.concat.count; i++) {
        struct SRegexpr *sub = &p->src->v.concat.exprs[i];
        int count = sub->min_count;
        count += p->added[i];
        for (j = 0; j < count; j++) {
            struct SAlteration node;
            alteration_init(&node, sub);
            LIST_APPEND(p, alters, node);
        }
        if (count > 0) {
            sub->last =  &p->alters[p->count - 1];
        }
        else {
            sub->last = NULL;
        }
    }
}

static long long concatenation_seq_capacity(struct SConcatenation *p) { // for fixed length
    long long ret = 1;
    int i;
    for (i = 0; i < p->src->v.concat.count; i++) {
        struct SRegexpr *sub = &p->src->v.concat.exprs[i];
        long long fl = sub->full_length;
        int count = sub->min_count + p->added[i];
        if (fl == UNLIMITED && count != 0)
            return UNLIMITED;
        else if (fl != 0) {
            long long opt_length = 1;
            int k;
            for (k = 0; k < count; k++) {
                opt_length = ll_mul(opt_length, fl);
            }
            ret = ll_mul(ret, opt_length);
        }
    }
    return ret;
}

void concatenation_free(struct SConcatenation *p) {
    int i;
    for (i = 0; i < p->count; i++) {
        alteration_free(&p->alters[i]);
    }
    free(p->alters);
    p->alters = NULL;
    p->count = p->capacity = 0;
}

void concatenation_init(struct SConcatenation *p, struct SRegexpr *concat) {
    int i;
    if (concat->type != TConcat) {
        PRINT_ERR("Wrong regexpr type for Concatenation initialization\n");
        exit(-1);
    }
    p->src = concat;
    p->maxs = (int*)calloc(concat->v.concat.count, sizeof(int));
    p->added = (int*)calloc(concat->v.concat.count, sizeof(int));
    p->min_length = 0;
    LIST_INIT(p, alters);
    for (i = 0; i < concat->v.concat.count; i++) {
        struct SRegexpr *sub = &concat->v.concat.exprs[i];
        p->min_length += sub->min_count;
    }
    concatenation_reset(p);
}

void concatenation_reset(struct SConcatenation *p) {
    p->overflowed = 0;
    concatenation_set_length(p, p->min_length);
}

struct SAlteration *concatenation_inc(struct SConcatenation *p) {
    int i;
    for (i = p->count - 1; i >= 0; i--) {
        if (alteration_inc(&p->alters[i]) != NULL)
            return &p->alters[p->count - 1];
    }
    // all alters overflowed, try rearrange current length
    if (inc_seq(p->maxs, p->added, p->src->v.concat.count)) {
        concatenation_init_alters(p);
        return &p->alters[p->count - 1];
    }
    if (concatenation_set_length(p, p->count + 1)) {
        return &p->alters[p->count - 1];
    }
    p->overflowed = 1;
    //concatenation_reset(p);
    return NULL;
}

void concatenation_set_offset(struct SConcatenation *p, long long offset) {
    int i, length;
    
    if (p->src->full_length == 0)
        return;
    if (p->src->full_length != UNLIMITED)
        offset %= p->src->full_length;
    
    PRINT_DBG("[concatenation_set_offset] offset = %lld, full_length = %lld\n", offset, p->src->full_length);
    
    for (length = p->min_length; ; length++) {
        long long capacity;
        int global_max = length - p->min_length;
        for (i = 0; i < p->src->v.concat.count; i++) {
            int max = p->src->v.concat.exprs[i].max_count;
            if (max == UNLIMITED)
                max = global_max;
            else
                max -= p->src->v.concat.exprs[i].min_count;
            p->maxs[i] = max;
        }
        if (!fill_seq(global_max, p->maxs, p->added, p->src->v.concat.count)) {
            PRINT_DBG("can't set length %d, must not ever happens!\n", length);
            return;
        }
        
        for (;;) {
            capacity = concatenation_seq_capacity(p);
            if (capacity == UNLIMITED || capacity > offset)
                break;
            offset -= capacity;
            capacity = 0;
            if (!inc_seq(p->maxs, p->added, p->src->v.concat.count))
                break;
        }
        
        if (capacity == UNLIMITED || capacity > offset) {
            PRINT_DBG("    found capacity = %lld for length = %d\n", capacity, length);
            break;
        }
        else {
            PRINT_DBG("    rest offset = %lld for length = %d, capacity = %lld\n", offset, length, capacity);
        }
    }
    
    concatenation_init_alters(p);
    
    for (i = p->count - 1; i >= 0; i--) {
        struct SAlteration *node = &p->alters[i];
        if (node->src->full_length == 0)
            continue;
        if (node->src->full_length == UNLIMITED) {
            alteration_set_offset(node, offset);
            break;
        }
        alteration_set_offset(node, offset);
        offset /= node->src->full_length;
    }
}

int concatenation_value(struct SConcatenation *p, char *dst, int max_length) {
    int i;
    int written = 0;
    for (i = 0; i < p->count; i++) {
        written += alteration_value(&p->alters[i], dst + written, max_length - written);
    }
    if (written < max_length)
        dst[written] = '\x00';
    return written;
}
