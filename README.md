# min: a minimal programming language
- min is a programming language aiming to minimalism and learning.
- min repository contains the language specs and a C reference implementation.
- min language is inspired by [C](https://en.wikipedia.org/wiki/C_(programming_language)), [Python](https://python.org), [Miniscript](https://miniscript.org), [Lua](https://lua.org), [Wren](https://wren.io), and [UCS](https://en.wikipedia.org/wiki/Uniform_Function_Call_Syntax).

## language goals
- spec should fit in a single paper sheet.
- easy to parse.
- easy to interpret.
- easy to transpile to other languages.
- low-level, typed. should work for systems programming.
- imperative, functional, oop paradigms.

## implementations
- C: vanilla and naive compiler can be found in [min.c](min.c) file (CC0/Unlicensed).

## license
- all documentation and language specs are CC0 licensed.

## min language
```python
// statements
single_statement()    // comment
multi(); statement()  // semi-colons

// values
'\x1'    // char
'\u1234' // char
'1'      // string
"1"      // string
1        // integer
1.0      // float
false    // empty strings, empty lists or empty maps are false as well
true

// operators
1+2 ; 2**3                  // 3; 8
+ - * / % **                // arithmetical
& | ~                       // bitwise
&& || ^ ! and or xor not    // logical

// conditions
if 2+2 >= 4:                // optional parentheses
    print("Math works!")
elif "a" < "b":
    print("Sortable strings")
else:
    print("Last chance")

List = ["Hello", "world"]
if "Hello" in List:
    print("found!")

ternary = 1 < 2 ? true : false

// loops
for i in range(10, 0):
    print(i + "...") // ; break ; continue
print("Go!")

s = "Hello"
while s.len() < 50:
    s = s + ", hello"

s = "Hello"
do:
    s = s + ", hello"
until s.len() < 50

// strings and slices
string1='hello world'
string2="hello world"
string3=f'interpolated {string1}'
string < string
string + string
number + string
string + number
string * 10 // x10 times
string[index]
string[index:count]
string[index:count:inc]
string[7]     // w
string[7:2]   // wo
string[7:]    // world
string[::-1]  // dlrow olleh
.len()
.hash()
.del([:])
.find('l')
.starts()
.ends()
.split()
.join()

// lists and slices
list = [2, 4, 6, 8]
list[0]     // 2
list[-1]    // 8
list[1:3]   // [4, 6]
list[2]=5   // list now [2,4,5,8]
list + list // union
list[i]     // get/set element i
list[i:j]   // get slice from i up to j
.len()
.del([:])
.starts()
.ends()
.join()

// maps
map = {1:"one", 2:"two"}
map[1]           // "one"
map[2]           // "two"
map.1            // "one"
map.2            // "two"
map2 = map + map // union
.len()
.del([:])
.keys()
.values()
.sort()

// functions, optional args, named args, lambdas
def triple(n=1):
    return n*3
print(triple())           // 3
print(triple(5))          // 15
fn = triple
print(fn(5))              // also 15
print(fn(.n=10))          // 30 (named argument)
fn = def(n=1): return n*2 // lambda
print(fn(5))              // 10

// classes are maps with a special `isa` entry that points to the parent.
// `isa` member is set automatically whenever you instance a class.
Shape = { sides: 0 }
// constructors and destructors
def init(Shape self, n=1): self.sides=n
def quit(Shape self):
// methods
def degrees(Shape self): return 180*(self.sides-2)
// derived
Square = Shape(4)
// instance
x = Square()
x.sides      // 4
x.isa        // "Shape"
x.degrees()  // 360

// templates (every single capital letter is expanded to a different type)
def min(T a, T b):
    return a < b ? a : b

// ucs (unified call syntax)
def add(vec2 a, vec2 b): return vec2(a.x+b.x, a.y+b.y)
v3 = add(v1,v2)
v3 = v3.add(v2)

// uss (unified struct syntax)
my_struct = {a:1, b:2, "c":3}
my_struct.a    // 1
my_struct[a]   // 1
my_struct["a"] // 1

// constness, symbol visibility: @todoc
// preprocessor and pragmas: @todoc
// ffi and C abi convention: @todoc
// embedding, calling from/to C: @todoc
// std modules: math.h, string.h, stdio.h @todoc
```
