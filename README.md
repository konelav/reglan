Regular Language Generator
==========================

Disclaimer
----------

This is just my pet-project for practice different skills with some 
simple program.


Introduction
------------

The most common task that can be solved with regular expressions, or 
*regexp*s, is finding whether or not given string (or *word*) match (or 
*belongs to*) specified regexp (or *language*). It can be said that any 
regexp *defines* some set of *words*. Sometimes it is useful to somehow 
enumerate the whole set and iterate through it. The most obvious reason 
to do that is bruteforcing something. 

(Sort of such functionality can be found in `John the Ripper` software, 
but it's rules pretty differ from regular expression syntax)


Quick start with examples
-------------------------

Just write regular expression and see set of words it generates:


    $ ./reglan "abc"
    abc
    $ ./reglan "(abc|def|ghi)"
    abc
    def
    ghi
    $ ./reglan "[0-1]{1,2}"
    0
    1
    00
    01
    10
    11
    $ ./reglan "a*b?c+"
    c
    ac
    bc
    cc
    aac
    abc
    acc
    bcc
    ccc
    aaac
    aabc
    aacc
    abcc
    accc
    bccc
    cccc
    aaaac
    ............


Features by examples
--------------------

Backreferences:

    $ ./reglan "([a-z]+)@\1\.(com|net|org)"
    a@a.com
    a@a.net
    a@a.org
    b@b.com
    b@b.net
    b@b.org
    c@c.com
    .............
    rybn@rybn.net
    rybn@rybn.org
    rybo@rybo.com
    rybo@rybo.net
    rybo@rybo.org
    rybp@rybp.com
    rybp@rybp.net
    .............


References to "dictionary", i.e. file that contains one possible word 
per line:

    $ ./reglan "(19[6-9][0-9])_(?F./names.txt)"
    1960_jack
    1960_john
    1960_james
    1960_max
    1960_bill
    1960_scott
    1961_jack
    1961_john
    1961_james
    1961_max
    1961_bill
    1961_scott
    1962_jack
    ...........
    1998_max
    1998_bill
    1998_scott
    1999_jack
    1999_john
    1999_james
    1999_max
    1999_bill
    1999_scott
    $ 


Skip arbitrary number of words:

    $ ./reglan -o 1234567890 -n 3 "[1-9]\d*"
    1234567891
    1234567892
    1234567893
    $ ./reglan -o 65536 -n 3 "[0-9a-f]{8}"
    00010000
    00010001
    00010002
    $ 


Combine with `John the Ripper`:

    $ ./reglan "(19[4-9][0-9]|20[01][0-9])-(0[1-9]|1[0-2])-(0[1-9]|[1-2][0-9]|3[0-1])" | sudo john --stdin /etc/shadow


Regexp short reference
----------------------

  - `'.'` - any character
  - `'[abc]'` - any of specified characters
  - `'[0-9]'` - any character within range
  - `'[^abc0-9]'` - any character that is not specified or within range
  - `'*'` - any number of repetitions
  - `'+'` - one or more repetition
  - `'?'` - zero or one repetition
  - `'{m,n}'` - from `m` to `n` repetitions, inclusively
  - `'{m,}'` - `m` or more repetitions
  - `'{m}'` - exaclty `m` repetitions
  - `'\\'` - escape character
  - `'\x0a'` - char code
  - `'\t', '\r', '\n', '\f', '\v\'` - whitespace characters
  - `'\d', '\D', '\s', '\S', '\w', '\W'` - charset classes
  - `'ab|cd'` - alternative subexprs
  - `'(...)'` - grouping subexprs
  - `'(?:...)'` - grouping subexprs without numeration (matter for 
  backrefs)
  - `'(?F<path>)'` - any line from file `<path>`
  - `\N` - backreference to subexpression with number `N` (between `1` 
  and `9`), strictly equals to corresponding subexression in each word


Usage
-----

    $ ./reglan -u
    Usage: ./reglan [-v] [-h] [-u] [-p] [-d] [-c] [-o <offset>] [-n <max_number>] [-b <buffer_size>] [<regexpr>]*
       -v       print version
       -h, -u   print usage (this info)
       -p       print parsed expression
       -d       print debug info, i.e. expression template state for each word
       -c       do not print generated words, buf print their total count after generation finishs
       -o <N>   skip <N> words from the beginning (default - do not skip anything)
       -n <N>   limit generated words count to this value (default - unlimited)
       -b <S>   set maximum word length to this value (default - 1023 bytes)


Unless `-c` is given, `reglan` will output every word in strict order, 
one per line, until either all possible words printed or `<max_number>` 
limit reached, for every `<regexpr>` separately.


Known issues
------------

  - Unicode is not supported (yet)
  - Syntactically wrong input leads to undefined behaviour
  - Undefined backreferences will be silently dropped
  - Although it is guaranteed (I hope) that every word of specified 
  language will be printed **at least** once, it is not guaranteed that 
  it will be printed **at most** once, i.e. some words can be printed 
  multiple times; e.g. regexp `'a?a?'` gives list of four words 
  `['', 'a', 'a', 'aa']`
  - internally `long long` is used for storing word numbers, hence the 
  limit on `-s <N>` and `-n <N>` arguments


Building
--------

It shoud be enough to run `make` from `src` directory. Only standard 
library is needed, no third-party dependencies.


Testing
-------

There is simple shellscript that do the following:

  - run `make all` from `src` directory
  - copy all binaries (`reglan`, `libreglan.a`, `libreglan.so`) and 
  header `reglan.h` to `test` directory
  - run simple python-based test script `test.py` which exptects that 
  executable `reglan` is near; it generates some random finite regular 
  languages and checks that `reglan` outputs only proper words and that 
  total amount of words is correct
  - compiles simple `test.c` program twice: statically linked with 
  `libreglan.a` and dynamically linkes with `libreglan.so`, then checks 
  that produced executable runs correctly


Python extension
----------------

Python extension can be built:

    $ cd python && python setup.py build && sudo python setup.py install
    $ python
    Type "help", "copyright", "credits" or "license" for more information.
    >>> import reglan
    >>> list(reglan.reglan("[012][34][56]"))
    ['035', '036', '046', '046', '136', '136', '146', '146', '236', '236', '246', '246']
    >>> for word in reglan.reglan("a*b?c{2,4}")[100:105]:
    ...     print(word)
    ... 
    aaaaaaaaaaaaaaaaabcc
    aaaaaaaaaaaaaaaaaccc
    aaaaaaaaaaaaaaaabccc
    aaaaaaaaaaaaaaaacccc
    aaaaaaaaaaaaaaabcccc


TODO
----

  - better documentation, including:
    + comments everywhere
    + doxygen comments
    + man and/or info format
  - add flexibility to extended syntax:
    + (?<ext-opts>:<expr>)
  - aliases, some way to define "subexpressions" and then reuse them;
  - some way to reproduce `john` functionality, e.g. uppercase-lowercase 
  word, capitalize first/last letter etc.
  - unicode support;
  - automatically increase buffer for output word as needed;
  - test with different environments, including Windows;
  - add wrappers for different languages/platforms:
    + nodejs, ruby, perl, php
    + java, .net/mono
    + mysql, mssql, postgres
  - packages:
    + deb
    + rpm
  - improve performance:
    + algorithm / structures
    + OpenMP
    + SSE/AVX
  - educational-aimed GUI
