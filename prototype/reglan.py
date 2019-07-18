#!/usr/bin/env python

ALL = [chr(c) for c in range(32, 128)]

def _parse_range(s, index):
    end = s.find('}', index)
    body = s[index+1:end]
    if body.find(',') > 0:
        minc, maxc = body.split(',')
        minc = int(minc)
        if maxc == "":
            maxc = -1
        else:
            maxc = int(maxc)
    else:
        minc = maxc = int(body)
    return minc, maxc, end

def _parse_set(s, index):
    index += 1
    if s[index] == '^':
        negate = True
        index += 1
    else:
        negate = False
    ret = []
    while index < len(s):
        c = s[index]
        if c == '\\':
            ret.append(s[index+1])
            index += 1
        elif c == '-':
            from_ch, to_ch = ret[-1], s[index+1]
            index += 1
            ret.extend([ch for ch in ALL 
                if ord(from_ch) <= ord(ch) and ord(ch) <= ord(to_ch)])
        elif c == ']':
            break
        else:
            ret.append(c)
        index += 1
    if negate:
        ret = [c for c in ALL if c not in ret]
    ret = "".join(sorted(set(ret)))
    return ret, index

def _parse_expr(s, index=0):
    branches = []
    nodes = []
    while index < len(s):
        c = s[index]
        if c == '.':
            nodes.append((ALL, 1, 1))
        elif c == '*':
            chars, _, _ = nodes[-1]
            nodes[-1] = (chars, 0, -1)
        elif c == '+':
            chars, _, _ = nodes[-1]
            nodes[-1] = (chars, 1, -1)
        elif c == '?':
            chars, _, _ = nodes[-1]
            nodes[-1] = (chars, 0, 1)
        elif c == '{':
            minc, maxc, index = _parse_range(s, index)
            chars, _, _ = nodes[-1]
            nodes[-1] = (chars, minc, maxc)
        elif c == '\\':
            nodes.append((chars[index + 1], 1, 1))
            index += 1
        elif c == '[':
            chars, index = _parse_set(s, index)
            nodes.append((chars, 1, 1))
        elif c == '|':
            branches.append(nodes)
            nodes = []
        elif c == '(':
            subexpr, index = _parse_expr(s, index+1)
            nodes.append((subexpr, 1, 1))
        elif c == ')':
            break
        else:
            nodes.append((c, 1, 1))
        index += 1
    if len(nodes) > 0:
        branches.append(nodes)
    return branches, index

def _fill_seq_with_sum(need_s, maxs):
    seq = [0] * len(maxs)
    for i in range(len(seq)):
        d = min(need_s, maxs[i])
        seq[i] = d
        need_s -= d
        if need_s == 0:
            return seq

def _next_seq_with_sum(seq, maxs):
    need_s = sum(seq)
    s = sum(seq)
    while True:
        inced = False
        for i in range(len(seq)):
            if seq[i] < maxs[i]:
                seq[i], s = seq[i] + 1, s + 1
                inced = True
                break
            seq[i], s = 0, s - seq[i]
        if not inced:
            break
        if s == need_s:
            return seq

class Node(object):
    def __init__(self, expr, endian=False):
        self.src = expr[:]
        self.endian = endian
        self.reset()
    
    def reset(self):
        if type(self.src) == list:  # list of alternatives
            self.expr = [Alternative(e, self.endian) for e in self.src]
        else:
            self.expr = self.src  # string
        self.ptr = 0  # current alternative
    
    def inc(self):
        self.ptr += 1
        if self.ptr >= len(self.expr):
            self.ptr = 0
            if type(self.expr) == list:
                to_remove = []
                for i, e in enumerate(self.expr):
                    if not e.inc():
                        to_remove.append(i)
                for i in reversed(to_remove):
                    self.expr.pop(i)
                if len(self.expr) > 0:
                    return True
            return False
        return True
    
    def value(self):
        if type(self.expr) == list:
            return self.expr[self.ptr].value()
        else:
            return self.expr[self.ptr]

class Alternative(object):
    def __init__(self, nodes, endian=False):
        self.endian = endian
        self.src = nodes[:]
        self.min_length = sum([min_count for (_, min_count, _) in self.src])
        self.reset()
    
    def reset(self):
        self.set_length(self.min_length)
    
    def set_length(self, length):
        self.length = 0
        global_max = length - self.min_length
        self.max_added = [(max_count if max_count > 0 else global_max) - min_count
                          for (_, min_count, max_count) in self.src]
        self.added = _fill_seq_with_sum(global_max, self.max_added)
        if self.added is None:
            return False
        self.nodes = []
        for i, to_add in enumerate(self.added):
            expr, min_count, max_count = self.src[i]
            nodes = []
            for _ in range(min_count + to_add):
                nodes.append(Node(expr, self.endian))
            self.nodes.append((nodes, expr, min_count, max_count))
            self.length += min_count + to_add
        return self.length == length
    
    def inc_counts(self):  #
        self.added = _next_seq_with_sum(self.added, self.max_added)
        if self.added is None:
            return False
        for i, (nodes, expr, min_count, _) in enumerate(self.nodes):
            new_count = min_count + self.added[i]
            while new_count < len(nodes):
                nodes.pop()
            while new_count > len(nodes):
                nodes.append(Node(expr, self.endian))
        return True
    
    def inc(self):
        scan = self.nodes if self.endian else reversed(self.nodes)
        for nodes, expr, _, _ in scan:
            scan_nodes = nodes if self.endian else reversed(nodes)
            for node in scan_nodes:
                if node.inc():
                    return True
                else:
                    node.reset()
        # all nodes overflowed, try rearrange current length
        if self.inc_counts():
            return True
        # try add next one
        if self.set_length(self.length + 1):
            return True
        # can't add any new node, overflow then
        self.reset()
        return False
    
    def value(self):
        ret = ""
        for nodes, _, _, _ in self.nodes:
            for node in nodes:
                ret += node.value()
        return ret


if __name__ == '__main__':
    import sys
    
    if len(sys.argv) > 1:
        pattern = sys.argv[1]
        if len(sys.argv) > 2:
            if sys.argv[2] == "all":
                limit = None
            else:
                limit = int(sys.argv[2])
        else:
            limit = None
        if len(sys.argv) > 3:
            output = (sys.argv[3] != "silent")
        else:
            output = True
        regexp, _ = _parse_expr(pattern)
        node = Node(regexp)
        i = 0
        while limit is None or i < limit:
            i += 1
            if output:
                print(node.value())
            if not node.inc():
                break
        if not output:
            print(i)
    else:
        tests = [
            "a{1,3}|b{0,2}",
            "[a-c][a-c]",
            "0{1,3}1{1,2}",
            "a{1,3}b{2,4}c{1,4}d{1,2}",
            "[0-2]{1,2}[a-c]{1,2}",
            "a*b{2,4}c*",
            "(a|b|c)[0-9]+(AA|B[a-f]{2}B|CC){1,3}",
            "19[0-9]{2}-(0[1-9]|11|12)-([0-2][1-9]|30|31)"
        ]
        for s in tests:
            regexp, _ = _parse_expr(s)
            print("{} => {}".format(s, regexp))
            node = Node(regexp)
            i = 0
            while True:
                i += 1
                print("  [{}]: '{}'".format(i, node.value()))
                if not node.inc() or i > 1000:
                    break
