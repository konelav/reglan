#!/usr/bin/env python

import re
import random
import os
import time
import subprocess

N_RANDOM_TESTS = 10
MAX_PRINT_SET = 30
MAX_CHECK_SET = 30000
MAX_CHECK_SIZE = 30000000
EXEC_PATH = "./reglan"


FIXED_TESTS = [
    (r"abc", 1, ['abc']),
    (r"(abc|def|ghi)", 3, ['abc', 'def', 'ghi']),
    (r"[0-1]{1,2}", 6, ['0', '1', '00', '01', '10', '11']),
    (r"([0-1]{3}|[a-c]{2}|[d-e]{1})", 2**3 + 3**2 + 2**1, ['000', 'aa', 'd', '001', 'ab', 'e', '010', 'ac' ,'011' ,'ba' ,'100' ,'bb' ,'101' ,'bc' ,'110' ,'ca' ,'111' ,'cb' ,'cc']),
    (r"([0-9]{3})\1{2,3}", 2000, None),
    (r"(abc)de(?:fg(hi|jk))\2", 2, ['abcdefghihi', 'abcdefgjkjk']),
    (r"(?Fnames.txt){8}", 6**8, None)
]


def generate_chars(n):
    ret = set()
    while len(ret) < n:
        c = chr(random.randint(ord('a'), ord('z')))
        ret.add(c)
    return "".join(sorted(ret))

def generate_regexp(max_alt=4, max_nodes=3, min_chars=2, max_chars=5, max_rep=2):
    n_alts = random.randint(1, max_alt)
    if n_alts == 1:
        n_nodes = random.randint(1, max_nodes)
        ret = ""
        count = 1
        for _ in range(n_nodes):
            n_chars = random.randint(min_chars, max_chars)
            n_maxrep = random.randint(1, max_rep)
            n_minrep = random.randint(0, n_maxrep)
            subcount = 0
            for rep in range(n_minrep, n_maxrep+1):
                subcount += n_chars ** rep
            chars = generate_chars(n_chars)
            ret += "[{}]{{{},{}}}".format(chars, n_minrep, n_maxrep)
            count *= subcount
        return ret, count
    count = 0
    ret = ""
    for patt, subcount in [generate_regexp(max_alt-1, max_nodes, min_chars, max_chars, max_rep)
                           for _ in range(n_alts)]:
        if len(ret) > 0:
            ret += "|"
        ret += patt
        count += subcount
    n_maxrep = random.randint(1, max_rep)
    n_minrep = random.randint(0, n_maxrep)
    totalcount = 0
    for rep in range(n_minrep, n_maxrep+1):
        totalcount += count ** rep
    ret = "({}){{{},{}}}".format(ret, n_minrep, n_maxrep)
    return ret, totalcount

def invoke(pattern, max_n, silent=True, offset=None):
    args = [EXEC_PATH, "-n", str(max_n)]
    if silent:
        args.append("-c")
    if offset is not None and offset > 0:
        args.extend(["-o", str(offset)])
    args.append(pattern)
    ret = subprocess.check_output(args)
    lines = ret[:-1].split('\n')
    if silent:
        return int(lines[0])
    else:
        return (lines)


ALL_TESTS = []
while len(ALL_TESTS) < N_RANDOM_TESTS:
    regexp, count = generate_regexp()
    if count > MAX_CHECK_SIZE:
        continue
    ALL_TESTS.append((regexp, count, None))

ALL_TESTS = FIXED_TESTS + ALL_TESTS

set_checks = 0
set_fails = 0
size_checks = 0
size_fails = 0
ntest = 0
total_generated = 0

t0 = time.time()
for regexp, count, wlist in ALL_TESTS:
    words, size = None, None
    
    if count < MAX_CHECK_SET or (wlist is not None):
        offset = random.randint(0, count // 2)
        words = invoke(regexp, count*10, False, offset)
        if wlist is not None:
            wlist = wlist[offset:]
        size = len(words)
    elif count <= MAX_CHECK_SIZE:
        offset = random.randint(1, count // 2)
        size = invoke(regexp, count*10, True, offset)
    else:
        continue
    
    ntest += 1
    print("TEST #{}".format(ntest))
    print("    pattern: {}, offset {}".format(regexp, offset))
    print("    language size: {}".format(count))
    
    if words is not None:
        if len(words) < MAX_PRINT_SET:
            print("    words: {}".format(words))
        if wlist is not None and len(wlist) < MAX_PRINT_SET:
            print("    wlist: {}".format(wlist))
        set_checks += 1
        r = re.compile(regexp)
        wrong_words = 0
        for w in words:
            if r.match(w) is None:
                wrong_words += 1
        print("    returned {} wrong words           [{}]".format(wrong_words,
            "OK" if wrong_words == 0 else "FAIL"))
        if wrong_words != 0:
            set_fails += 1
        if wlist is not None:
            set_checks += 1
            d = set(wlist) ^ set(words)
            print("    symmetric difference of words and wlist: {}   [{}]".format(
                list(d), "OK" if len(d) == 0 else "FAIL"))
            if len(d) > 0:
                set_fails += 1
    if size is not None:
        size_checks += 1
        print("    returned size = {} + {}, expected {}   [{}]".format(
            size, offset, count, "OK" if size + offset == count else "FAIL"))
        if size + offset != count:
            size_fails += 1
    
    total_generated += size
dt = time.time() - t0

print("{} test(s)".format(len(ALL_TESTS)))
print("{} size checks, {} fails".format(size_checks, size_fails))
print("{} set checks, {} fails".format(set_checks, set_fails))
print("Generated {} strings in {:.3f} sec [{:.1f} s/s]".format(
    total_generated, dt, total_generated / dt))
