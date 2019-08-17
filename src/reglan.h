#ifndef _REGLAN_H_INCLUDED
#define _REGLAN_H_INCLUDED 1

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/** \file reglan.h
 *  \brief Common definitions and module interface
 * 
 * This file provides some macros, typedefs and function prototypes for 
 * use `reglan` libraries (either static or shared).
 */

/** \brief C-string with current reglan version
 * 
 * Version conforms one of the most widely used tradition, i.e. it has 
 * three decimal numbers separated by dot '.':
 *   - the first number called *MAJOR* and corresponds to set of back-compatible 
 *     APIs, i.e. user can expect that if his code works with some version 
 *     of library, then it will also work with any further version with 
 *     the save *MAJOR* component; the only exception for this rule is 
 *     zero major version 0.x.x, which assumes that project is on the 
 *     early stages and API just can't be stable enough (yet);
 *   - the second number called *MINOR* and increases when some feature 
 *     added to library (usually it means that API was extended with some 
 *     function, or in case of `reglan` itself it could mean that supported 
 *     syntax was extended);
 *   - the last number called *RELEASE* or *REVISION* or the like and 
 *     increased when some bugs was fixed (usually API has not changed 
 *     at all).
 */
#define VERSION     "0.4.2"

/** \brief Denotes infinite length
 * 
 * This macro used to mark length of infinite sequences, e.g. maximum 
 * number of repetitions of expression with quantifiers `*`, `+`, or `{n,}`.
 */
#define UNLIMITED   (-1)

/** \brief Maximum positive integer that can be handled by library
 * 
 * This big value used for overflows detection on arithmetic operations.
 */
#define BIGNUM      ((long long)0x7FFFFFFFFFFFFFFF)

/** \brief Lower limit of dot class '.'
 * 
 * Minimum ASCII code for matching dot class '.'.
 */
#define MIN_CHAR    (32)
/** \brief Upper limit of dot class '.'
 * 
 * Maximum ASCII code for matching dot class '.'.
 */
#define MAX_CHAR    (128)

/** \brief Initialize dynamic array with default capacity equals to 4
 * 
 * Do basic initialization of dynamic array of some type with initial
 * capacity set to 4 and size set to 0.
 * 
 * \param pList pointer to structure that must contain fields `capacity`,
 *              `count` and param `Type` (second argument)
 * \param Type  pointer to array with length `capacity` and `count` items in use
 */
#define LIST_INIT(pList, Type) { \
    (pList)->capacity = 4; \
    (pList)->count = 0; \
    (pList)->Type = calloc((pList)->capacity, sizeof(*((pList)->Type))); \
}

/** \brief Extends allocated memory if needed
 * 
 * Checks if given array allocates at least `Capacity` members. If this
 * is not true, reallocates buffer for array so its capacity equals to
 * requested value.
 * 
 * \param pList pointer to structure that contains dynamic array
 * \param Type  name of pointer to dynamic array
 * \param Capacity needed capacity (allocated length) of dynamic array
 */
#define LIST_ENSURE_CAPACITY(pList, Type, Capacity) { \
    if ((pList)->capacity < (Capacity)) { \
        (pList)->capacity = (Capacity); \
        (pList)->Type = realloc((pList)->Type, (Capacity) * sizeof(*((pList)->Type))); \
    } \
}

/** \brief Extends allocated memory
 * 
 * Increases number of elements of dynamic array with simple algorithm: 
 *   - if current capacity is lower that 128, then it is twiced
 *   - otherwise, increase capacity by 128 elements
 * 
 * \param pList pointer to structure that contains dynamic array
 * \param Type  name of pointer to dynamic array
 */
#define LIST_ENLARGE(pList, Type) { \
    if ((pList)->capacity < 128) \
        (pList)->capacity *= 2; \
    else \
        (pList)->capacity += 128; \
    (pList)->Type = realloc((pList)->Type, ((pList)->capacity) * sizeof(*((pList)->Type))); \
}

/** \brief Add new element to list
 * 
 * Ensures that there is free (allocated but not used) space in the given
 * list, then writes data of given element to first unused place.
 * 
 * \param pList pointer to structure that contains dynamic array
 * \param Type  name of pointer to dynamic array
 * \param Value value of new element
 */
#define LIST_APPEND(pList, Type, Value) { \
    if ((pList)->capacity == (pList)->count) { \
        LIST_ENLARGE((pList), Type); \
    } \
    (pList)->Type[(pList)->count] = Value; \
    (pList)->count += 1; \
}

/** \brief Allows debugging messages
 * 
 * If this macro is defined at compile time, then some messages will be
 * printed to stdout during program execution.
 */
#ifdef DEBUG
/** \brief Output debugging message
 * 
 * Print some info to stdout if only macro `DEBUG` was defined.
 */
#define PRINT_DBG(...)   printf(__VA_ARGS__)
#else
#define PRINT_DBG(...)
#endif

/** \brief Disables error messages
 * 
 * If this macro is defined at compile time, then there will be no
 * messages about errors in stdout. This can lead to seemingly strange
 * behaviour, though it is useful for embedding code to other languages.
 */
#ifdef NOERRMSG
#define PRINT_ERR(...)
#else
/** \brief
 * 
 * Print some info to stderr if only macro `NOERRMSG` was not defined.
 */
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

/** \brief Possible types of regular expressions
 * 
 * Every regular expression belongs to some type each of which is 
 * handled in different ways.
 */
enum EExprType {
    TBackref,   /**< Backreference, i.e. exact copy of some other regexpression */
    TCharset,   /**< Exactly one character from given set, such as any embedded class */
    TWords,     /**< One line (without end-of-line characters) from given set of lines (file) */
    TConcat,    /**< Concatenation of one or more other regular expressions, each of which can 
                     be repeated zero or more times according to quantifier */
    TAlter      /**< Exactly one regular expression from given set */
};

/** \brief Parsed backreference to other sub expression
 * 
 * Value of this expression exactly equals to value of referenced expression.
 * If referenced expression is undefined, the value of this expression is
 * empty string.
 */
struct SBackref {
    int num;                /**< Sequenctial number of referenced sub expression, e.g. '\3' has `num` set to 3 */
    struct SRegexpr *expr;  /**< Pointer to information about cloned (referenced) expression */
};

/** \brief Parsed set of possible characters
 * 
 * Value of this expression has length strictly equal to 1 and iterates 
 * through all possible characters in given sequence.
 */
struct SCharset {
    char *chars;    /**< pointer to beginning of set of characters */
    int count;      /**< number of possible characters in set */
};

/** \brief Parsed set of possible words
 * 
 * Value of this expression iterates through all possible words in given
 * sequence.
 * Sequence is loaded from some file. Each line of file gives exactly one
 * word consists of all character of line except `\r` and `\n` at the end.
 */
struct SWords {
    char *fname;    /**< C-string with name of file that is source of set of word */
    char **words;   /**< Array of C-strings with words from file */
    int count;      /**< Number of words in sequence */
};

/** \brief Parsed concatenation of subexprs
 * 
 * Value of this expression equals to concatenation of values of its
 * members, each of which can go zero or more times in a row.
 */
struct SConcat {
    struct SRegexpr *exprs; /**< Pointer to dynamic array of concatenated expressions */
    int capacity, count;    /**< Capacity and length of this dynamic array */
};

/** \brief Parsed alteration of subexprs
 * 
 * Value of this expression iterates through values of possible
 * alternative subexpressions one-by-one.
 */
struct SAlter {
    struct SRegexpr *exprs; /**< Pointer to dynamic array of possible subexpressions */
    int capacity, count;    /**< Capacity and length of this dynamic array */
};

/** \brief Parsed regular (sub)expression (group)
 * 
 * Represents some expression with corresponding quantifier, i.e. 
 * quantity of times this expression can be repeated (at least and at most).
 */
struct SRegexpr {
    enum EExprType type;        /**< Type of subexpression */
    union {
        struct SBackref ref;
        struct SCharset set;
        struct SWords words;
        struct SConcat concat;
        struct SAlter alter;
    } v;                        /**< Value of subexpression, filled according
                                     `type` field */
    int min_count, max_count;   /**< Minimum and maximum number of repetitions */
    int ngroup;                 /**< Global number for this group, equals
                                     to zero if group has no number */
    
    struct SAlteration *last;   /**< Pointer to last generated value for 
                                     this expression (needed for backreferences) */
    long long full_length;      /**< Length of sequence of words that can 
                                     be generated by this subexpression, equals
                                     to `UNLIMITED` if it is infinite or too large
                                     to fit to `long long` type */
};

/** \brief Alteration iterator
 * 
 * Serves state of concrete alteration through all possible alternatives, including
 * iteration of each alternative themselves.
 */
struct SAlteration {
    struct SRegexpr *src;           /**< Pointer to parsed expression that can
                                         be of any type except `TConcat` */
    
    struct SConcatenation *concats; /**< Dynamic array of alternative concatenations
                                         for `src` type `TAlter` */
    int capacity, count;            /**< Capacity and size of this array */
    
    int ptr;                        /**< Index of current alternative choice.
                                         Its meaning depends on the type of `src`:
                                           - for `TBackref` it is ignored and must always equal zero;
                                           - for `TCharset` it is index of character in set;
                                           - for `TWords` it is index of word in list;
                                           - for `TAlter` it is number of concatenation in list. */
    
    char *last_dst;                 /**< Last memory location to which this expression
                                         has written its value. Used for fast instantiation
                                         in case individual positions of each subexpression was
                                         unchanged */
    int last_length;                /**< Length of last value of this expression */
};

/** \brief Concatenation iterator
 * 
 * Serves state of concrete concatenation, i.e. fixed sequence of some expressions,
 * each of which is repeated zero or more times depending on quantifier of
 * its origin.
 */
struct SConcatenation {
    struct SRegexpr *src;       /**< Pointer to parsed expression that must has
                                     type `TConcat` */
    
    struct SAlteration *alters; /**< Dynamic array of subexpressions in sequence */
    int capacity, count;        /**< Capacity and length of this array */
    
    int min_length;             /**< Minimum total count of subexpressions, i.e.
                                     sum of minimum quantifiers of all items in `alters` */
    int *maxs;      /**< Array of numbers of maximum repetitions of corresponding subexpr
                         for current (fixed) total number of subexpressions */
    int *added;     /**< Array of numbers of additional (to the quantifier minimum) instances
                         of corresponding subexpr */
    int overflowed; /**< Flag used by `SAlteration` to decide whether alterate this
                         concatenation sequence or skip it because it is exhausted */
};

/* arith.c */
/** \brief Addition with overflow check
 * 
 * \param a first number
 * \param b second number
 * \return sum `a + b` if it is less than `BIGNUM`, otherwise `UNLIMITED`
 */
long long ll_add(long long a, long long b);
/** \brief Multiplication with overflow check
 * 
 * \param a first number
 * \param b second number
 * \return sum `a * b` if it is less than `BIGNUM`, otherwise `UNLIMITED`
 */
long long ll_mul(long long a, long long b);

/* parse.c */
/** \brief Fills backref pointers according to group numbers
 * 
 * Consists of two recursive operations:
 *   - build list of pointers to `struct SRegexpr` where `i`-th element
 *     points to regexp with `ngroup` equal to `i`;
 *   - set `expr` value of each `struct SBackref` to pointer from list
 *     indexed with its `num` value.
 * 
 * \param p root regular expression
 */
void link_backrefs(struct SRegexpr *p);
/** \brief Calculates total number of words in regular language
 * 
 * Recursively fills `full_length` value of each `struct SRegexpr`.
 * If number of words is infinite or bigger than `BIGNUM`, then
 * `full_length` will be set to value `UNLIMITED`.
 * 
 * \param p root regular expression
 */
void calc_full_length(struct SRegexpr *p);
/** \brief Parse string to regular expression representation
 * 
 * \param src pointer to null-terminated string that contains regular expression
 * \param p (out) structure where to place parsed representation
 * \param root (out) structure where to place language iterator (set to very first word of language)
 */
void parse(const char *src, struct SRegexpr *p, struct SAlteration *root);
/** \brief Free memory allocated by regular expression representation
 * 
 * Recursively free regular expression and all its subexpressions,
 * including sets of characters and words.
 * 
 * \param p root regular expression
 */
void regexp_free(struct SRegexpr *p);

/* alteration.c */
/** \brief Free memory allocated by alteration iterator
 * 
 * Recursively free alteration iterator and all its concatenations (if there are any).
 * 
 * \param p root alteration iterator
 */
void alteration_free(struct SAlteration *p);
void alteration_init(struct SAlteration *p, struct SRegexpr *re);
void alteration_reset(struct SAlteration *p);
/** \brief Move alteration iterator forward to the next word
 * 
 * According to the actual type of alteration does the following:
 *   - for `TBackref` simply does nothing;
 *   - for `TCharset` and `TWords` moves to the next character or word in
 *     set accordingly, starting with first one after last;
 *   - for `TAlter` moves to the next non-overflowed alternative; if end 
 *     of alternatives reached, then tries to increment all non-overflowed ones
 *     and start from beginning.
 * 
 * \param p pointer to itrator to be incremented
 * \return pointer to the most descent iterator that was actually incremented,
 *         or `NULL` if end is reached
 */
struct SAlteration *alteration_inc(struct SAlteration *p);
void alteration_set_offset(struct SAlteration *p, long long offset);
/** \brief Evaluates current iterator value
 * 
 * Places string representation of current word pointed by iterator to
 * the specified location in memory.
 * 
 * \param p pointer to iterator to be instantiated
 * \param dst pointer to buffer to which word should be copied
 * \param max_length maximum length of word (buffer length)
 * \return number of characters in current word (copied to the buffer)
 */
int alteration_value(struct SAlteration *p, char *dst, int max_length);
/** \brief Tries to move iterator fast
 * 
 * This function is intended for optimization. Its argument must be 
 * pointer returned by `alteration_inc()`. If this function succeed,
 * then root iterator argument of corresponding `alteration_inc()` call
 * can be considered incremented also, and its value is placed in exactly
 * same memory buffer that is requested by the last call of `alteration_value()`
 * for this iterator.
 * 
 * The only types of iterators supported by this function are `TCharset`
 * and `TWords`: it just tries to move to the next option, and succeed
 * if and only if the length of next option equals to the length of
 * current option.
 * 
 * \param pointer to iterator to be incremented and instantiated at once
 * \return `1` if successfully incremented, `0` otherwise
 */
int alteration_inc_inplace(struct SAlteration *p);

/* concatenation.c */
/** \brief Free memory allocated by concatenation iterator
 * 
 * Recursively free concatenation iterator and all its alterations (if there are any).
 * 
 * \param p root concatenation iterator
 */
void concatenation_free(struct SConcatenation *p);
void concatenation_init(struct SConcatenation *p, struct SRegexpr *concat);
void concatenation_reset(struct SConcatenation *p);
/** \brief Move concatenation iterator forward to the next word
 * 
 * Tries to increment right-most alteration iterator of concatenated
 * sequence. If it overflows, then tries to increment next-to-the-left
 * iterator and so on. If all iterators overflow, then checks if the same
 * number of *tokens* (repetitions) can be re-distributed between alterations 
 * with given quantifiers, i.e. remove one repetition from some alteration
 * and add repetition to some other alteration. If all possible distributions
 * for current total number of tokens are iterated, then it tries to increase
 * number of tokens by one and starts from beginning. If maximum number of
 * tokens for this concatenation is reached, then marks concatenation as
 * overflowed and sets its length to minimum number of tokens.
 * 
 * \param p pointer to itrator to be incremented
 * \return pointer to the most descent iterator that was actually incremented,
 *         or `NULL` if end is reached
 */
struct SAlteration *concatenation_inc(struct SConcatenation *p);
void concatenation_set_offset(struct SConcatenation *p, long long offset);
int concatenation_value(struct SConcatenation *p, char *dst, int max_length);

/* print.c */
void regexp_print(struct SRegexpr *p, int indent);
void alteration_print(struct SAlteration *p, int indent);
void concatenation_print(struct SConcatenation *p, int indent);

#endif  // _REGLAN_H_INCLUDED
