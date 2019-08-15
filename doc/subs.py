import re

RE_VERSION = re.compile(r"^\s*#\s*define\s+VERSION\s+\"(?P<version>[^\"]*)\"")

version = None
with open('../src/reglan.h', 'r') as freglan:
    for line in freglan.readlines():
        m = RE_VERSION.match(line)
        if m is not None:
            version = m.group("version")
            break

if version is None:
    version = "0"

subs = {
    "VERSION": version,
}

with open('Doxyfile.in', 'r') as fin, \
     open('Doxyfile', 'w') as fout:
    for line in fin.readlines():
        if line.lstrip().startswith('#'):
            continue
        if len(line.strip()) == 0:
            continue
        for var, val in subs.items():
            line = line.replace("${}$".format(var), val)
        fout.write(line)
