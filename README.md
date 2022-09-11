# min: a minimal scripting language
- min is a scripting language, whose language specification & syntax aim to be minimalist.
- min is inspired by [C](https://en.wikipedia.org/wiki/C_(programming_language)), [Python](https://python.org), [Miniscript](https://miniscript.org), [Lua](https://lua.org), [Wren](https://wren.io), and [UCS](https://en.wikipedia.org/wiki/Uniform_Function_Call_Syntax).

## language goals
- relatively powerful.
- hopefully easy to grasp.
- transpilable to other higher languages.
- specification fits in a single paper sheet.
- completely free: MIT0/CC0/Unlicense licensed at your choice.

## existing implementations
- a vanilla, naive, **incomplete WIP C interpreter** can be found in [min.c](min.c) file (CC0/Unlicensed).
  - goals: single-file, no deps, free, meaningful subset of the language, aims to fewer LOC.
  - non-goals: efficent, robust, bulletproof, VM/JIT/AOT, maintained.
- Others: xxx

## language
```python
// min language spec (v0: 2022.9 wip)
// - public domain.

//////////////////////////// types
'\x1' '\u1234'            // 32-bit char (ascii and utf8 notations)
16 0x10                   // 64-bit integer (decimal and hexadecimal notations)
16. 16.0 16.f             // 64-bit float (different notations)
"16" '16'                 // string (both pairing quotes allowed)
true false                // any matching condition, non-zero number or non-empty string/list/map evaluates to `true`; `false` otherwise

//////////////////////////// operators
+ - * ** / %              // arithmetical (** for pow)
& | ~ ^ && || !           // bitwise and logical

//////////////////////////// statements
statement()               // semi-colons at end of line are optional
multiple(); statements()  // semi-colons separate statements within a line

//////////////////////////// conditions
if (2+2 >= 4):            // parentheses in conditions are optional
    puts("Math works!")   //
elif "a" < "b":           // unconstrained `else if` can also be used
    puts("Sort strings")  // code blocks either use Python style ':' plus indents as seen here,
else {                    // or, use C style '{}' as seen here.
    puts("Last chance")   //
}                         //
sign = PI < 0 ? -1 : +1   // ternary

//////////////////////////// loops
s = "Hello"               // any loop construction can use `break` and `continue` for control flow
do:                       //
    s = s + ", hi"        //
while len(s) < 25         //
                          //
while len(s) < 50:        //
    s = s + ", hi"        //
                          //
for i=10; i>0; --i:       //
    puts(i + "...")       //
puts("Go!")               //

//////////////////////////// functions (hint: 'self' or 'self_[anything]' named args are mutables; use 'mut' instead?)
def triple(n=1):          // optional args
    n*3                   // last stack value implicitly returned
triple()                  // 3
triple(5)                 // 15
triple(.n=10)             // 30. named argument.
copy = triple             // capture function
copy(10)                  // also 30

//////////////////////////// strings
hi='hello'                // strings can use "quotes" or 'quotes' at your discretion
string=f"{hi} world"      // f-interpolated string -> 'hello world'
string[2]                 // positive indexing from first element [0] -> 'e'
string[-3]                // negative indexing from last element [-1] -> 'r'
string < string           // comparison (<,>,==,!=)
string + string           // concatenation -> "hello worldhello world"
string - string           // removes all occurences of right string in left string -> ""
.hash() .len() .del([:]) .find(ch) .rfind(ch) .starts(s) .ends(s) .split(s) .slice(i,j) .eval()

//////////////////////////// lists (either vector or linked-list at vendor discretion)
list = [2, 4, 6, 8]       //
list[0]                   // positive indexing from first element [0] -> 2
list[-2]=5                // negative indexing from last element [-1] -> list becomes [2,4,5,8]
.len() .del([:]) .find(lst) .starts(lst) .ends(lst) .join(sep) .slice(i,j)

//////////////////////////// maps (either ordered or unordered at vendor discretion) (EXTENSION)
map = {"a":1,'b':2,c:3}   // keys can be specified with different styles
map.a                     // 1
map->b                    // 2
map["c"]                  // 3
.len() .del([:]) .find(x) .keys(wc) .values(wc) .sort(op) .slice(i,j)

//////////////////////////// set theory (EXTENSION)
list+list or map+map      // union
list-list or map-map      // difference
list^list or map^map      // intersection

//////////////////////////// slices (EXTENSION)
string[i:j]               // slices [i:j] from i up to j (included). defaults to [0:-1]
string[7:9]               // "wo"
string[7:]                // "world"
string[-1:0]              // "dlrow olleh"
list[i:j]                 // slices [i:j] from i up to j (included). defaults to [0:-1]
list[-1:1]                // [8,5,4]
map[i:j]                  // slices [i:j] from i up to j (included). defaults to [(first map key):(last map key)]
map["b":"a"]              // {b:2,a:1}

///////////////////////////////////////////////// classes (EXTENSION)
// classes are maps with a special `typeof`    //
// entry that points to the parent class and   //
// is set automatically at instance time.      //
/////////////////////////////////////////////////
Shape = { sides: 0 }                           // plain data struct. methods below:
def Shape(Shape self, n=1): self.sides=n       // constructor
def ~Shape(Shape self):                        // destructor
def degrees(Shape self): self.sides*180-360    // method
// instance base class //////////////////////////
Square = Shape(4)                              //
Square.typeof                                  // "Shape"
Square.sides                                   // 4
// instance derived class ///////////////////////
x = Square()                                   //
x.typeof                                       // "Square"
x.degrees()                                    // 360

//////////////////////////////////// casts (EXTENSION)
int(val)                          // bool,char,float,string->int specializations already provided.
bool(val)                         // int,char,float,string->bool specializations already provided.
char(val)                         // int,bool,float,string->char specializations already provided.
float(val)                        // int,bool,char,string->float specializations already provided.
string(val)                       // int,bool,char,float->string specializations already provided.
def vec3(vec2 a): vec3(a.x,a.y,0) // as long as conversion functions are provided,
vec3(val2)                        // custom casting from any type to another is possible.

///////////////////////////////// unified call syntax (EXTENSION)
vec2 = { x: 0, y: 0 }          //
def add(vec2 a, vec2 b):       //
    vec2(a.x+b.x, a.y+b.y)     //
v1 = v2 = vec2()               //
v3 = add(v1,v2)                // functional
v3 = v1.add(v2)                // chain

/////////////////////////////// ranges (EXTENSION)
range(4,0)                   // returns a list of elements from i up to j (included) -> [4,3,2,1,0]

/////////////////////////////// in (EXTENSION)
for i in range(-1,3): puts(i) // `in` operator returns iterator on containers (lists,maps,objects,ranges) -> -1 0 1 2 3

////////////////////////// lambdas (EXTENSION)
i = 5                   //
fn = def(n=1): i*2*n    //
fn(10)                  // 100

///////////////////// templates (EXTENSION)
def min(T a, T b): // single capital letters expand to different types as used
    a < b ? a : b  //

/////////////////// typeof (EXTENSION)
typeof(3.0)      // "float"
typeof(object)   // alias to object.typeof

// constness, symbol visibility: @todoc
// preprocessor and pragmas: @todoc
// ffi and C abi convention: @todoc
// embedding, calling from/to C: @todoc
// std modules: math.h, string.h, stdio.h @todoc
// keywords: true/false if/elif/else do/while for/break/continue def typeof in range @todoc
```
