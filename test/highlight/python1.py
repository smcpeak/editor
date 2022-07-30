#!/usr/bin/env python3
# python1.py
# Test program for the highlighter.

import datetime
import decimal

# Does backslash continue comments? No. \
print("this is not a comment!")

year = 2022
month = 7
day = 30
hour = 2
minute = 59
second = 30
if 1900 < year < 2100 and 1 <= month <= 12 \
   and 1 <= day <= 31 and 0 <= hour < 24 \
   and 0 <= minute < 60 and 0 <= second < 60:   # Looks like a valid date
        print("valid date")


month_names = ['Januari', 'Februari', 'Maart',      # These are the
               'April',   'Mei',      'Juni',       # Dutch names
               'Juli',    'Augustus', 'September',  # for the months
               'Oktober', 'November', 'December']   # of the year

def perm(l):
        # Compute the list of all permutations of l
    if len(l) <= 1:
                  return [l]
    r = []
    for i in range(len(l)):
             s = l[:i] + l[i+1:]
             p = perm(s)
             for x in p:
              r.append(l[i:i+1] + x)
    return r

# The triple-quote syntax treats backslashes literally, except they
# still combine with the next character.  Consequently, something like:
#   """\"""
# is not a valid string literal.

s = r"""\\"""
print(f"two backslashes: {s}")

s = r"""\""""
print(f"backslash quote: {s}")

s = "#"
print(f"hash: {s}")

s = ''
s = 'x'
s = '\''
print(f"tick: {s}")
s = '\'x\''
print(f"tick x tick: {s}")
s = r'x'
s = u'x'
s = R'x'
s = U'x'
s = f'x'
s = F'x'
s = fr'x'
s = Fr'x'
s = fR'x'
s = FR'x'
s = rf'x'
s = rF'x'
s = Rf'x'
s = RF'x'

s = ""
s = "x"
s = "\""
print(f"quote: {s}")
s = "\"x\""
print(f"quote x quote: {s}")
s = r"x"
s = u"x"
s = R"x"
s = U"x"
s = f"x"
s = F"x"
s = fr"x"
s = Fr"x"
s = fR"x"
s = FR"x"
s = rf"x"
s = rF"x"
s = Rf"x"
s = RF"x"

s = '''x'''
s = '''x\ny'''
print(f"x nl y: {s}")
s = '''x
y'''
print(f"x nl y: {s}")
s = r'''x\ny'''
print(f"x backslash n y: {s}")

s = """x"""
s = """x\ny"""
print(f"x nl y: {s}")
s = """x
y"""
print(f"x nl y: {s}")
s = r"""x\ny"""
print(f"x backslash n y: {s}")

s = r'\
y'
print(f"backslash nl y: {s}")

b = b'x'

print(f"{3=}") # 3=3

name = "Fred"
f"He said his name is {name!r}."
"He said his name is 'Fred'."
f"He said his name is {repr(name)}."  # repr() is equivalent to !r
width = 10
precision = 4
value = decimal.Decimal("12.34567")
f"result: {value:{width}.{precision}}"  # nested fields
today = datetime.datetime(year=2017, month=1, day=27)
f"{today:%B %d, %Y}"  # using date format specifier
f"{today=:%B %d, %Y}" # using date format specifier and debugging
number = 1024
f"{number:#0x}"  # using integer format specifier
foo = "bar"
f"{ foo = }" # preserves whitespace
line = "The mill's closed"
f"{line = }"
f"{line = :20}"
f"{line = !r:20}"

#f"abc {a["x"]} def"    # error: outer string literal ended prematurely
a={'x':3}
s = f"abc {a['x']} def"    # workaround: use different quoting
print(s)

newline = ord('\n')
print(f"newline ASCII code: {newline}")

lits = [
    7                               ,
    2147483647                      ,
    0o177                           ,
    0b100110111                     ,
    3                               ,
    79228162514264337593543950336   ,
    0o377                           ,
    0xdeadbeef                      ,
    100_000_000_000                 ,
    0b_1110_0101                    ,
]
for n in lits:
    print(f" {n}", end="")
print()

lits = [
    3.14                            ,
    10.                             ,
    .001                            ,
    1e100                           ,
    3.14e-10                        ,
    0e0                             ,
    3.14_15_93                      ,
]
for n in lits:
    print(f" {n}", end="")
print()

lits = [
    3.14j                           ,
    10.j                            ,
    10j                             ,
    .001j                           ,
    1e100j                          ,
    3.14e-10j                       ,
    3.14_15_93j                     ,
]
for n in lits:
    print(f" {n}", end="")
print()

# EOF
