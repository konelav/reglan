#include "reglan.h"

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
        if (i == length) // not incremented
            return 0;
        if (s == sum)
            return 1;
    }
}

void alternative_free(PAlternative alternative) {
    int i;
    for (i = 0; i < alternative->nodes_length; i++) {
        node_free(&alternative->nodes[i]);
    }
    free(alternative->nodes);
    alternative->nodes_capacity = 0;
}

void alternative_init(PAlternative alternative, PBranch branch) {
    int i;
    alternative->src = branch;
    alternative->maxs = (int*)calloc(branch->options_length, sizeof(int));
    alternative->added = (int*)calloc(branch->options_length, sizeof(int));
    alternative->min_length = 0;
    alternative->full_length = 1;
    LIST_INIT(alternative, nodes);
    for (i = 0; i < branch->options_length; i++) {
        POption opt = &branch->options[i];
        alternative->min_length += opt->min_count;
        if (alternative->full_length != UNLIMITED) {
            long long fl;
            Node node;
            node_init(&node, opt->backref, opt->chars, opt->chars_length, opt->regexp);
            opt->full_length = fl = node.full_length;
            node_free(&node);
            if (fl == UNLIMITED && opt->max_count != 0)
                alternative->full_length = UNLIMITED;
            else if (fl != 0 && opt->max_count == UNLIMITED)
                alternative->full_length = UNLIMITED;
            else if (fl != 0) {
                long long all_counts_len = 0;
                int j, k;
                for (j = opt->min_count; j <= opt->max_count; j++) {
                    long long fixed_count_length = 1;
                    for (k = 0; k < j; k++)
                        fixed_count_length = ll_mul(fixed_count_length, fl);
                    all_counts_len = ll_add(all_counts_len, fixed_count_length);
                }
                alternative->full_length = ll_mul(alternative->full_length, all_counts_len);
            }
        }
    }
    alternative_reset(alternative);
}

void alternative_reset(PAlternative alternative) {
    alternative->overflowed = 0;
    alternative_set_length(alternative, alternative->min_length);
}

int alternative_set_length(PAlternative alternative, int length) {
    int i;
    int global_max = length - alternative->min_length;
    for (i = 0; i < alternative->src->options_length; i++) {
        int max = alternative->src->options[i].max_count;
        if (max == UNLIMITED)
            max = global_max;
        else
            max -= alternative->src->options[i].min_count;
        alternative->maxs[i] = max;
    }
    if (!fill_seq(global_max, alternative->maxs, alternative->added, 
            alternative->src->options_length)) {
        return 0;
    }
    alternative_init_nodes(alternative);
    return (alternative->nodes_length == length) ? 1 : 0;
}

void alternative_init_nodes(PAlternative alternative) {
    int i, j, length;
    for (i = 0; i < alternative->nodes_length; i++) {
        node_free(&alternative->nodes[i]);
    }
    for (i = 0, length = 0; i < alternative->src->options_length; i++) {
        POption opt = &alternative->src->options[i];
        int count = opt->min_count;
        count += alternative->added[i];
        length += count;
    }
    LIST_ENSURE_CAPACITY(alternative, nodes, length);
    alternative->nodes_length = 0;
    for (i = 0; i < alternative->src->options_length; i++) {
        POption opt = &alternative->src->options[i];
        int count = opt->min_count;
        count += alternative->added[i];
        for (j = 0; j < count; j++) {
            Node node;
            node_init(&node, opt->backref, opt->chars, opt->chars_length, opt->regexp);
            LIST_APPEND(alternative, nodes, node);
        }
        if (count > 0) {
            opt->last_node =  &alternative->nodes[alternative->nodes_length - 1];
        }
        else {
            opt->last_node = NULL;
        }
    }
}

PNode alternative_inc(PAlternative alternative) {
    int i;
    for (i = alternative->nodes_length - 1; i >= 0; i--) {
        if (node_inc(&alternative->nodes[i]) != NULL)
            return &alternative->nodes[alternative->nodes_length - 1];
    }
    // all nodes overflowed, try rearrange current length
    if (inc_seq(alternative->maxs, alternative->added, 
        alternative->src->options_length)) {
        alternative_init_nodes(alternative);
        return &alternative->nodes[alternative->nodes_length - 1];
    }
    if (alternative_set_length(alternative, alternative->nodes_length + 1)) {
        return &alternative->nodes[alternative->nodes_length - 1];
    }
    alternative->overflowed = 1;
    //alternative_reset(alternative);
    return NULL;
}

static long long alternative_seq_capacity(PAlternative alternative) { // for fixed length
    long long ret = 1;
    int i;
    for (i = 0; i < alternative->src->options_length; i++) {
        POption opt = &alternative->src->options[i];
        long long fl = opt->full_length;
        int count = opt->min_count + alternative->added[i];
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

void alternative_set_offset(PAlternative alternative, long long offset) {
    int i, length;
    
    if (alternative->full_length == 0)
        return;
    if (alternative->full_length != UNLIMITED)
        offset %= alternative->full_length;
    
    PRINT_DBG("[alternative_set_offset] offset = %lld, full_length = %lld\n", offset, alternative->full_length);
    
    for (length = alternative->min_length; ; length++) {
        long long capacity;
        int global_max = length - alternative->min_length;
        for (i = 0; i < alternative->src->options_length; i++) {
            int max = alternative->src->options[i].max_count;
            if (max == UNLIMITED)
                max = global_max;
            else
                max -= alternative->src->options[i].min_count;
            alternative->maxs[i] = max;
        }
        if (!fill_seq(global_max, alternative->maxs, alternative->added, 
            alternative->src->options_length)) {
            PRINT_DBG("can't set length %d, must not ever happens!\n", length);
            return;
        }
        
        for (;;) {
            capacity = alternative_seq_capacity(alternative);
            if (capacity == UNLIMITED || capacity > offset)
                break;
            offset -= capacity;
            capacity = 0;
            if (!inc_seq(alternative->maxs, alternative->added, 
                alternative->src->options_length))
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
    
    alternative_init_nodes(alternative);
    
    for (i = alternative->nodes_length - 1; i >= 0; i--) {
        PNode node = &alternative->nodes[i];
        if (node->full_length == 0)
            continue;
        if (node->full_length == UNLIMITED) {
            node_set_offset(node, offset);
            break;
        }
        node_set_offset(node, offset);
        offset /= node->full_length;
    }
}

int alternative_value(PAlternative alternative, char *dst, int max_length) {
    int i;
    int written = 0;
    for (i = 0; i < alternative->nodes_length; i++) {
        written += node_value(&alternative->nodes[i], dst + written, max_length - written);
    }
    if (written < max_length)
        dst[written] = '\x00';
    return written;
}
