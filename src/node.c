#include "reglan.h"

void node_free(PNode node) {
    int i;
    if (!node->backref && !node->str && node->regexp && !node->regexp->words) {
        for (i = 0; i < node->regexp->branches_length; i++)
            alternative_free(&node->alternatives[i]);
        free(node->alternatives);
        node->alternatives_capacity = 0;
    }
}

void node_init(PNode node, POption backref, char *str, int str_length, PRegExp regexp) {
    int i;
    
    if ((str && regexp) || (backref && str) || (backref && regexp)) {
        fprintf(stderr, "Wrong node_init() arguments: "
            "backref = %p, str = %p <%s>, regexp = %p\n",
            backref, str, str, regexp);
    }
    
    if (!str && !regexp && !backref) {
        PRINT_DBG("Empty node (no charset, no regexp, no backref)\n");
    }
    
    node->last_dst = NULL;
    node->last_length = 0;
    node->backref = backref;
    node->str = str;
    node->str_length = str_length;
    node->regexp = regexp;
    node->full_length = 0;
    
    if (node->backref) {
        node->full_length = 0;
    }
    else if (node->str) {
        node->full_length = node->str_length;
    }
    else if (node->regexp && node->regexp->words) {
        node->full_length = node->regexp->words_count;
    }
    else if (node->regexp) {
        LIST_INIT(node, alternatives);
        for (i = 0; i < node->regexp->branches_length; i++) {
            Alternative alternative;
            alternative_init(&alternative, &node->regexp->branches[i]);
            LIST_APPEND(node, alternatives, alternative);
            node->full_length = ll_add(node->full_length, alternative.full_length);
        }
    }
    
    node_reset(node);
}

void node_reset(PNode node) {
    int i;
    node->ptr = 0;
    if (!node->backref && !node->str && node->regexp && !node->regexp->words) {
        for (i = 0; i < node->alternatives_length; i++) {
            alternative_reset(&node->alternatives[i]);
        }
    }
}

PNode node_inc(PNode node) {
    int i;
    
    node->ptr++;
    
    if (node->backref) {
        return NULL;
    }
    else if (node->str) {
        if (node->ptr >= node->str_length) {
            node->ptr = 0;
            return NULL;
        }
        return node;
    }
    else if (node->regexp && node->regexp->words) {
        if (node->ptr >= node->regexp->words_count) {
            node->ptr = 0;
            return NULL;
        }
        return node;
    }
    else if (node->regexp) {
        PNode incremented = NULL;
        
        while (node->ptr < node->alternatives_length) {
            if (!node->alternatives[node->ptr].overflowed) {
                return node;
            }
            node->ptr++;
        }
        
        for (i = 0; i < node->alternatives_length; i++) {
            if (!node->alternatives[i].overflowed) {
                PNode inced;
                if ((inced = alternative_inc(&node->alternatives[i])) != NULL) {
                    if (!incremented) {
                        node->ptr = i;
                        incremented = inced;
                    }
                }
            }
        }
        
        if (!incremented) {  // all overflowed
            node->ptr = 0;
            for (i = 0; i < node->alternatives_length; i++) {
                alternative_reset(&node->alternatives[i]);
            }
        }
        
        return incremented;
    }
    return NULL;
}

void node_set_offset(PNode node, long long offset) {
    if (node->full_length == 0) {
        PRINT_DBG("Trying to set offset %lld for zero-length node\n", offset);
        return;
    }
    if (node->full_length != UNLIMITED)
        offset %= node->full_length;
    
    PRINT_DBG("[node_set_offset] offset = %lld, full_length = %lld\n", offset, node->full_length);
    
    if (node->backref) {
    }
    else if (node->str) {
        node->ptr = offset;
    }
    else if (node->regexp && node->regexp->words) {
        node->ptr = offset;
    }
    else if (node->regexp) {
        int i;
        long long *offsets = (long long*)calloc(node->alternatives_length, sizeof(long long));
        long long rest;
        
        while (offset > 0) {
            int min_i = -1, skipable = 0;
            long long min_rest = UNLIMITED;
            PRINT_DBG("    offset %lld, rests = {", offset);
            for (i = 0; i < node->alternatives_length; i++) {
                rest = node->alternatives[i].full_length;
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
            {
                long long to_skip, skip_each, skip_remainder;
                int n_skip;
                
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
                for (i = 0; i < node->alternatives_length; i++) {
                    rest = node->alternatives[i].full_length;
                    if (rest != UNLIMITED)
                        rest -= offsets[i];
                    if (rest == 0)
                        continue;
                    offsets[i] += skip_each;
                    offset -= skip_each;
                    if (rest == skip_each) {
                        node->alternatives[i].overflowed = 1;
                    }
                    if (n_skip == skip_remainder) {
                        node->ptr = i;
                        offset -= skip_remainder;
                    }
                    n_skip++;
                }
            }
        }
        for (i = 0; i < node->alternatives_capacity; i++)
            alternative_set_offset(&node->alternatives[i], offsets[i]);
        free(offsets);
        if (node->alternatives[node->ptr].overflowed) {
            PRINT_DBG("Alternative overflowed after node skipping\n");
            node_inc(node);
        }
    }
}

int node_value(PNode node, char *dst, int max_length) {
    int length = 0;
    node->last_dst = dst;
    if (node->backref) {
        if (node->backref->last_node) {
            length = node_value(node->backref->last_node, dst, max_length);
        }
    }
    else if (node->str) {
        if (max_length > 0) {
            dst[0] = node->str[node->ptr];
            length = 1;
        }
    }
    else if (node->regexp && node->regexp->words) {
        length = strlen(node->regexp->words[node->ptr]);
        if (length > max_length)
            length = max_length;
        memcpy(dst, node->regexp->words[node->ptr], length);
    }
    else if (node->regexp) {
        length = alternative_value(&node->alternatives[node->ptr], dst, max_length);
    }
    node->last_length = length;
    return length;
}

int node_inc_inplace(PNode node) {
    if (!node->last_dst)
        return 0;
    if (node->backref) {
        if (!node->backref->last_node)
            return 0;
        if (!node_inc_inplace(node->backref->last_node))
            return 0;
        memcpy(node->last_dst, node->backref->last_node->last_dst, node->last_length);
        return 1;
    }
    else if (node->str) {
        if (node->ptr + 1 >= node->str_length)
            return 0;
        node->ptr++;
        *(node->last_dst) = node->str[node->ptr];
        return 1;
    }
    else if (node->regexp && node->regexp->words) {
        if (node->ptr + 1 >= node->regexp->words_count)
            return 0;
        if (strlen(node->regexp->words[node->ptr + 1]) != node->last_length)
            return 0;
        node->ptr++;
        memcpy(node->last_dst, node->regexp->words[node->ptr], node->last_length);
        return 1;
    }
    else if (node->regexp) {
        return 0;
    }
    return 0;
}
