#include "reglan.h"

void alteration_free(struct SAlteration *p) {
    if (p->concats && p->capacity > 0) {
        int i;
        for (i = 0; i < p->count; i++)
            concatenation_free(&p->concats[i]);
        free(p->concats);
    }
    p->concats = NULL;
    p->capacity = p->count = 0;
}

void alteration_init(struct SAlteration *p, struct SRegexpr *re) {
    memset(p, 0, sizeof(struct SAlteration));
    
    p->src = re;
    if (re->type == TConcat) {
        PRINT_ERR("Wrong type for node initialization - TConcat\n");
        exit(-1);
    }
    if (re->type == TAlter) {
        int i;
        LIST_INIT(p, concats);
        for (i = 0; i < re->v.alter.count; i++) {
            struct SConcatenation concat;
            struct SRegexpr *sub = &re->v.alter.exprs[i];
            if (sub->type != TConcat) {
                PRINT_ERR("Wrong type of alternative (TConcat expected)\n");
                continue;
            }
            concatenation_init(&concat, sub);
            LIST_APPEND(p, concats, concat);
        }
    }
    
    alteration_reset(p);
}

void alteration_reset(struct SAlteration *p) {
    p->ptr = 0;
    if (p->src->type == TAlter) {
        int i;
        for (i = 0; i < p->count; i++) {
            concatenation_reset(&p->concats[i]);
        }
    }
}

struct SAlteration *alteration_inc(struct SAlteration *p) {
    struct SAlteration *incremented = NULL;
    
    p->ptr++;
    
    switch (p->src->type) {
        case TBackref:
            break;
        case TCharset:
            if (p->ptr >= p->src->v.set.count)
                p->ptr = 0;
            else
                incremented = p;
            break;
        case TWords:
            if (p->ptr >= p->src->v.words.count)
                p->ptr = 0;
            else
                incremented = p;
            break;
        case TConcat:
            PRINT_ERR("Wrong type of Node (Concat)\n");
            exit(-1);
            break;
        case TAlter:
            {
                int i;
                while (p->ptr < p->count) {
                    if (!p->concats[p->ptr].overflowed)
                        return p;
                    p->ptr++;
                }
        
                for (i = 0; i < p->count; i++) {
                    if (!p->concats[i].overflowed) {
                        struct SAlteration *inced;
                        if ((inced = concatenation_inc(&p->concats[i])) != NULL) {
                            if (!incremented) {
                                p->ptr = i;
                                incremented = inced;
                            }
                        }
                    }
                }
        
                if (!incremented) {  // all overflowed
                    p->ptr = 0;
                    for (i = 0; i < p->count; i++)
                        concatenation_reset(&p->concats[i]);
                }
            }
            break;
    }
    return incremented;
}

void alteration_set_offset(struct SAlteration *p, long long offset) {
    long long fl = p->src->full_length;
    
    if (fl == 0) {
        PRINT_DBG("Trying to set offset %lld for zero-length node\n", offset);
        return;
    }
    if (fl != UNLIMITED)
        offset %= fl;
    
    PRINT_DBG("[alteration_set_offset] offset = %lld, full_length = %lld\n", offset, fl);
    
    switch (p->src->type) {
        case TBackref:
            break;
        case TCharset:
        case TWords:
            p->ptr = offset;
            break;
        case TConcat:
            PRINT_ERR("Wrong type of Node (Concat)\n");
            exit(-1);
            break;
        case TAlter:
            {
                int i;
                long long *offsets = (long long*)calloc(p->count, sizeof(long long));
                long long rest;
                
                while (offset > 0) {
                    long long to_skip, skip_each, skip_remainder;
                    long long min_rest = UNLIMITED;
                    int min_i = -1, skipable = 0, n_skip;
                    
                    PRINT_DBG("    offset %lld, rests = {", offset);
                    for (i = 0; i < p->count; i++) {
                        rest = p->concats[i].src->full_length;
                        if (rest != UNLIMITED)
                            rest -= offsets[i];
                        PRINT_DBG("%lld ", rest);
                        if (rest == 0)
                            continue;
                        skipable++;
                        if (min_i == -1 || (min_rest > rest && rest != UNLIMITED)) {
                            min_i = i;
                            min_rest = rest;
                        }
                    }
                    PRINT_DBG("}\n");
                    PRINT_DBG("    min_i = %d [min_rest %lld], skipable = %d\n", min_i, min_rest, skipable);
                    if (skipable == 0) { // nothing to skip at all
                        PRINT_DBG("Nothing can be skipped, but offset %lld requested\n", offset);
                        break;
                    }
                    
                    if (min_rest == UNLIMITED) {
                        to_skip = offset;
                    }
                    else {
                        to_skip = skipable * min_rest;
                        if (to_skip > offset)
                            to_skip = offset;
                    }
                    
                    skip_each = to_skip / skipable;
                    skip_remainder = to_skip % skipable;
                    
                    PRINT_DBG("    to_skip = %lld, skip_each = %lld, skip_remainder = %lld\n", to_skip, skip_each, skip_remainder);
                    
                    n_skip = 0;
                    for (i = 0; i < p->count; i++) {
                        rest = p->concats[i].src->full_length;
                        if (rest != UNLIMITED)
                            rest -= offsets[i];
                        if (rest == 0)
                            continue;
                        offsets[i] += skip_each;
                        offset -= skip_each;
                        if (rest == skip_each) {
                            p->concats[i].overflowed = 1;
                        }
                        if (n_skip == skip_remainder) {
                            p->ptr = i;
                            offset -= skip_remainder;
                        }
                        n_skip++;
                    }
                }
                
                for (i = 0; i < p->count; i++)
                    concatenation_set_offset(&p->concats[i], offsets[i]);
                free(offsets);
                if (p->concats[p->ptr].overflowed) {
                    PRINT_DBG("Alternative overflowed after node skipping\n");
                    alteration_inc(p);
                }
            }
            break;
    }
}

int alteration_value(struct SAlteration *p, char *dst, int max_length) {
    int length = 0;
    p->last_dst = dst;
    switch (p->src->type) {
        case TBackref:
            if (p->src->v.ref.expr && p->src->v.ref.expr->last) {
                length = alteration_value(p->src->v.ref.expr->last, dst, max_length);
            }
            else {
                PRINT_ERR("NULL-backreference #%d\n", p->src->v.ref.num);
            }
            break;
        case TCharset:
            if (max_length > 0) {
                dst[0] = p->src->v.set.chars[p->ptr];
                length = 1;
            }
            else {
                PRINT_ERR("Not enough space for char, max_length = %d\n", max_length);
            }
            break;
        case TWords:
            length = strlen(p->src->v.words.words[p->ptr]);
            if (length > max_length) {
                PRINT_ERR("Not enough space for word, length = %d, max_lengt= %d\n", length, max_length);
                length = max_length;
            }
            memcpy(dst, p->src->v.words.words[p->ptr], length);
            break;
        case TConcat:
            PRINT_ERR("Wrong type of Node (Concat)\n");
            exit(-1);
            break;
        case TAlter:
            length = concatenation_value(&p->concats[p->ptr], dst, max_length);
            break;
    }
    p->last_length = length;
    return length;
}

int alteration_inc_inplace(struct SAlteration *p) {
    if (!p->last_dst)
        return 0;
    switch (p->src->type) {
        case TBackref:
            if (!p->src->v.ref.expr || !p->src->v.ref.expr->last)
                return 0;
            if (!alteration_inc_inplace(p->src->v.ref.expr->last))
                return 0;
            memcpy(p->last_dst, p->src->v.ref.expr->last->last_dst, p->last_length);
            return 1;
        case TCharset:
            if (p->ptr + 1 >= p->src->v.set.count)
                return 0;
            p->ptr++;
            *(p->last_dst) = p->src->v.set.chars[p->ptr];
            return 1;
        case TWords:
            if (p->ptr + 1 >= p->src->v.words.count)
                return 0;
            if (strlen(p->src->v.words.words[p->ptr + 1]) != p->last_length)
                return 0;
            p->ptr++;
            memcpy(p->last_dst, p->src->v.words.words[p->ptr], p->last_length);
            return 1;
        case TConcat:
            PRINT_ERR("Wrong type of Node (Concat)\n");
            exit(-1);
            break;
        case TAlter:
            return 0;
    }
    return 0;
}
