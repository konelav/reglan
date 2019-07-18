#include "reglan.h"

void option_print(POption option, int indent) {
    int i;
    for (i = 0; i < indent; i++)
        putchar(' ');
    printf("Option := {%d, %d} ", option->min_count, option->max_count);
    if (option->ngroup) {
        printf(" #%d ", option->ngroup);
    }
    if (option->backref) {
        printf(" --> #%d\n", option->refnum);
    }
    else if (option->chars) {
        printf("[%s]\n", option->chars);
    }
    else if (option->regexp) {
        printf("\n");
        regexp_print(option->regexp, indent + 2);
    }
    else {
        printf(" (nil)\n");
    }
}

void branch_print(PBranch branch, int indent) {
    int i;
    for (i = 0; i < indent; i++)
        putchar(' ');
    printf("Branch := {%d}\n", branch->options_length);
    for (i = 0; i < branch->options_length; i++)
        option_print(&branch->options[i], indent + 2);
}

void regexp_print(PRegExp regexp, int indent) {
    int i;
    for (i = 0; i < indent; i++)
        putchar(' ');
    printf("Regexp := ");
    if (regexp->words) {
        printf(" dict[%d]\n", regexp->words_count);
    }
    else {
        printf("{%d}\n", regexp->branches_length);
        for (i = 0; i < regexp->branches_length; i++)
            branch_print(&regexp->branches[i], indent + 2);
    }
}

void node_print(PNode node, int indent) {
    int i;
    for (i = 0; i < indent; i++)
        putchar(' ');
    printf("Node[%lld] := ", node->full_length);
    if (node->backref) {
        printf(" --> #%d\n", node->backref->ngroup);
    }
    else if (node->str) {
        printf("'%s' [%d]\n", node->str, node->ptr);
    }
    else if (node->regexp && node->regexp->words) {
        printf("dict, words[%d] [%d]\n", node->regexp->words_count, node->ptr);
    }
    else if (node->regexp) {
        printf(" {%d} [%d]\n", node->alternatives_length, node->ptr);
        for (i = 0; i < node->alternatives_length; i++)
            alternative_print(&node->alternatives[i], indent + 2);
    }
    else {
        printf(" (nil)\n");
    }
}

void alternative_print(PAlternative alternative, int indent) {
    int i;
    for (i = 0; i < indent; i++)
        putchar(' ');
    printf("Alternative[%lld] := {%d} [%d+] ", alternative->full_length, 
        alternative->nodes_length, alternative->min_length);
    if (alternative->overflowed)
        printf(" [OVR] ");
    for (i = 0; i < alternative->nodes_length; i++) {
        printf("(+%d / %d) ", alternative->added[i], alternative->maxs[i]);
    }
    printf("\n");
    for (i = 0; i < alternative->nodes_length; i++) {
        node_print(&alternative->nodes[i], indent + 2);
    }
}
