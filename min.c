// min.c scripting language: reference implementation (WIP! WIP! WIP!)
// - rlyeh, public domain.

#line 1 "utils.h"

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// -----------------------------------------------------------------------------

#define strcmp(a,b) (0[a] != 0[b] || strcmp((a)+1,(b)+1))

// -----------------------------------------------------------------------------
// thread locals

#ifdef _MSC_VER
#define __thread         __declspec(thread)
#elif defined __TINYC__ && defined _WIN32
#define __thread         __declspec(thread) // compiles fine, but does not work apparently
#elif defined __TINYC__
#define __thread
#endif

// -----------------------------------------------------------------------------
// hashing

uint64_t hash_u64(uint64_t x) {
    x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
    x = x ^ (x >> 31);
    return x;
}
uint64_t hash_flt(double x) {
    union { double d; uint64_t i; } c;
    return c.d = x, hash_u64(c.i);
}
uint64_t hash_str(const char* str) {
    uint64_t hash = 14695981039346656037ULL; // hash(0),mul(131) faster than fnv1a, a few more collisions though
    while( *str ) hash = ( (unsigned char)*str++ ^ hash ) * 0x100000001b3ULL;
    return hash;
}
uint64_t hash_ptr(const void *ptr) {
    uint64_t key = (uint64_t)(uintptr_t)ptr;
    return hash_u64(key); // >> 3? needed?
}

// -----------------------------------------------------------------------------
// memory (vector based allocator; x1.75 enlarge factor)

void* vrealloc( void* p, size_t sz ) {
    if( !sz ) {
        if( p ) {
            size_t *ret = (size_t*)p - 2;
            ret[0] = 0;
            ret[1] = 0;
            realloc( ret, 0 );
        }
        return 0;
    } else {
        size_t *ret;
        if( !p ) {
            ret = (size_t*)realloc( 0, sizeof(size_t) * 2 + sz );
            ret[0] = sz;
            ret[1] = 0;
        } else {
            ret = (size_t*)p - 2;
            size_t osz = ret[0];
            size_t ocp = ret[1];
            if( sz <= (osz + ocp) ) {
                ret[0] = sz;
                ret[1] = ocp - (sz - osz);
            } else {
                ret = (size_t*)realloc( ret, sizeof(size_t) * 2 + sz * 1.75 );
                ret[0] = sz;
                ret[1] = (size_t)(sz * 1.75) - sz;
            }
        }
        return &ret[2];
    }
}
size_t vlen( void* p ) {
    return p ? 0[ (size_t*)p - 2 ] : 0;
}

// -----------------------------------------------------------------------------
// arrays

#ifdef __cplusplus
#define array_cast(x) (decltype x)
#else
#define array_cast(x) (void *)
#endif

#define array(t) t*
#define array_resize(t, n) ( array_c_ = array_count(t), array_n_ = (n), array_realloc_((t),array_n_), (array_n_>array_c_? memset(array_c_+(t),0,(array_n_-array_c_)*sizeof(0[t])) : (void*)0), (t) )
#define array_push(t, ...) ( array_realloc_((t),array_count(t)+1), (t)[ array_count(t) - 1 ] = (__VA_ARGS__) )
#define array_back(t) ( &(t)[ array_count(t)-1 ] ) // ( (t) ? &(t)[ array_count(t)-1 ] : NULL )
#define array_count(t) (int)( (t) ? array_vlen_(t) / sizeof(0[t]) : 0u )
#define array_empty(t) ( !array_count(t) )

#define array_reserve(t, n) ( array_realloc_((t),(n)), array_clear(t) )
#define array_clear(t) ( array_realloc_((t),0) ) // -1
#define array_vlen_(t)  ( vlen(t) - sizeof(0[t]) ) // -1
#define array_realloc_(t,n)  ( (t) = array_cast(t) vrealloc((t), ((n)+1) * sizeof(0[t])) ) // +1
#define array_free(t) ( array_realloc_((t), -1), (t) = 0 ) // -1

#define each_array(t,val_t,v) /* usage: for each_array(t,T,i) print(*i); */ \
    ( int __it = 0, __end = array_count(t); __it < __end; ++__it ) \
        for( val_t *v = &__it[t]; v; v = 0 )

static __thread unsigned array_c_;
static __thread unsigned array_n_;

// -----------------------------------------------------------------------------
// strings

#include <stdarg.h>

#define  va(...) ((printf || printf(__VA_ARGS__), va(__VA_ARGS__)))  // vs2015 check trick

char* (va)(const char *fmt, ...) {
    va_list vl;
    va_start(vl, fmt);

        va_list copy;
        va_copy(copy, vl);
        int sz = /*stbsp_*/vsnprintf( 0, 0, fmt, copy ) + 1;
        va_end(copy);

        int reqlen = sz;
        enum { STACK_ALLOC = 128*1024 };
        static __thread char *buf = 0; if(!buf) buf = realloc(0, STACK_ALLOC); // @leak
        static __thread int cur = 0, len = STACK_ALLOC - 1; //printf("string stack %d/%d\n", cur, STACK_ALLOC);

        assert(reqlen < STACK_ALLOC && "no stack enough, increase STACK_ALLOC variable above");
        char* ptr = buf + (cur *= (cur+reqlen) < len, (cur += reqlen) - reqlen);

        /*stbsp_*/vsnprintf( ptr, sz, fmt, vl );

    va_end(vl);
    return (char *)ptr;
}

// -----------------------------------------------------------------------------
#line 1 "parser.c"

const int VERBOSE = 1; // if true, outputs parsing tree

typedef struct node {
    int type;

    const char *value_begin, *value_end;
    const char *debug;

    struct node *parent;  // "int" > "main" > "(i)" > "{ while(i<10) ++i; }" <<<<<<<<<<<<<< siblings. no parents. {block} has children.
    struct node *sibling; //                           \> "while" > "(i < 10)" > "++i" <<<< siblings and children of {block}. (block) has children.
    struct node *child;   //                                         \> "i" > "<" > "10" << siblings and children of (block). no children.
    struct node *skip;    // shortcut to next statement
} node;

node AST, *env = &AST, *sta = &AST, *parent; // env+sta+parent are temporaries used to populate AST hierarchy (siblings+skip+parent)

int parse_string_extended; // if true, function below will decode <abc> strings in addition to regular 'abc' and "abc" strings
int parse_string(const char *begin, const char *end) {
    const char *s = begin;
    if( s < end ) {
        if( *begin == 'f' ) { // f-interpolated strings
            int ret = parse_string(begin+1, end);
            return ret ? ret + 1 : 0;
        }
        if( *begin == '\'' || *begin == '"' || (parse_string_extended ? *begin == '<' : 0) ) {
            const char *close = *begin != '\'' && *begin != '"' && parse_string_extended ? ">" : begin ;
            do {
                ++s;
                if( *s == '\\' ) {
                    if( s[1] == '\\' ) s += 2;
                    else if( s[1] == *close ) s += 2;
                }
            } while (s <= end && *s != *close);

            return s <= end && *s == *close ? (int)(s + 1 - begin) : 0;
        }
    }
    return 0;
}

int found;

int parse_lexem(const char *begin, const char *end) {
    found = 0;

    const char *s = begin;

#define eos (s > end)

    if( !found ) { // string literals
        int run = parse_string(s, end);
        s += run;
        found = /*!eos &&*/ run ? '\"' : 0;
    }

    if(!found) while( !eos && (*s <= 32) ) { // whitespaces, tabs & linefeeds
        found = ' ';
        ++s;
    }

    if(!found) while( !eos && (*s == '#') ) { // preprocessor
        found = '#';
        ++s;
        while( !eos && (*s >= 32) ) {
            /**/ if( *s == '\\' && (s[1] == '\r' && s[2] == '\n') ) s += 3; // @fixme: should be considered a (b)lank and not (p)reprocessor
            else if( *s == '\\' && (s[1] == '\r' || s[1] == '\n') ) s += 2; // @fixme: should be considered a (b)lank and not (p)reprocessor
            else ++s;
        }
    }

    if(!found) while( !eos && (*s == '/' && s[1] == '/') ) { // C++ comments
        found = '/';
        ++s;
        while( !eos && (*s >= 32) ) {
            ++s;
        }
    }

    if(!found) while( !eos && (*s == '/' && s[1] == '*') ) { // C comments
        found = '/';
        s += 2;
        while( !eos && !(*s == '*' && s[1] == '/') ) ++s;
           if( !eos &&  (*s == '*' && s[1] == '/') ) s += 2;
    }

    if(!found) while( !eos && (isalpha(*s) || *s == '_') ) { // keywords
        found = '_';
        ++s;
        while( !eos && isdigit(*s) ) {
            ++s;
        }
    }

    if(!found) while( !eos && strchr(",", *s) ) { // separators
        found = ',';
        ++s;
    }

    if(!found) while( !eos && strchr(";", *s) ) { // terminators
        found = ';';
        ++s;
    }

#if 0
    if(!found) while( !eos && strchr(".0123456789", *s) ) { // numbers
        found = '0';
        ++s;
        /**/ if( *s == 'f' ) ++s;                  // 3.f
        else if( *s == 'e' ) s += 1 + s[1] == '-'; // 1e9 1e-9
    }
#else
    if(!found) if(!eos) { // numbers
        if( s[0] == '.' || isdigit(*s) ) { // (s[0] >= '0' && s[0] <= '9') ) {
            found = '0';
            ++s; if( s[0] == 'x' ) ++s;
            while( !eos && strchr(".0123456789ABCDEFabcdef", *s) ) {
                ++s;
            }
        }
    }
#endif

    if(!found) while( !eos && strchr("<=>!&|~^+-/*%.?:\\", *s) ) { // operators
        found = '+';
        ++s;
    }

    if(!found) if( !eos ) { // () [] {} sub-blocks
        int depth = 0, endchar = *s == '(' ? ')' : *s == '[' ? ']' : *s == '{' ? '}' : '\0';
        if( endchar ) do {

            int run = parse_string(s, end); // bypass chars/strings with possible endchars on them
            s += run;
            if( eos ) break;

            depth += (*s == *begin) - (*s == endchar);
            found += depth > 0;
            ++s;
        } while( !eos && depth );
        if( found ) found = *begin;
    }

#undef eos

    return found ? (int)(s - begin) : 0;
}

int parse(const char *begin, const char *end) {
    static int tabs = 0;

    const char *s = begin;
    for( int read = parse_lexem(s, end); read; s += read, read = parse_lexem(s, end) ) {

        if( found == ' ' || found == '/' ) continue; // skip (b)lanks and (c)omments from parsing
        if( VERBOSE ) printf("type(%c):len(%02d):%*.s\"%.*s\"\n", found, read, tabs*2,"", read, s);

        // inscribe
        env->type = found;
        env->value_begin = s;
        env->value_end = s + read;
        env->debug = memcpy( calloc(1, read+1), s, read );
        env->parent = parent;

        // parse hierarchy recursively if needed
        int statement = !parent && (found == '#' || found == '{' || found == ';');
        int recursive = found == '#' || found == '{' || found == '(' || found == '[';
        if( recursive ) {
            // save depth
            node *copy_env = env;
            node *copy_parent = parent;

                // ensure subcalls will have current env marked as their parent
                parent = env;

                // inscribe
                env = (env->sibling = calloc(1, sizeof(node)));

                    // recurse
                    ++tabs;
                    parse_string_extended = found == '#';

                        if( found == '#' ) parse( s+1, s+read-1 ); // exclude initial #preprocessor digit from parsing
                        else parse( s+1, s+read-2 );               // exclude initial/ending block{} digits from parsing

                    parse_string_extended = 0;
                    --tabs;

                // relocate sibling list as child
                copy_env->child = copy_env->sibling;
                copy_env->sibling = 0;

            // restore depth
            env = copy_env;
            parent = copy_parent;
        }

        // prepare env for upcoming node
        env->sibling = calloc(1, sizeof(node));

        // linked-list statements when finished
        if( statement ) {
            sta->skip = env->sibling;
            sta = env->sibling;
            if( VERBOSE ) puts("");
        }

        // linked-list env
        env = env->sibling;
    }
    return (int)(s - begin);
}

// -----------------------------------------------------------------------------
#line 1 "env.c"

#include <stdint.h>
#include <inttypes.h>

enum {
    TYPE_VOID,TYPE_NULL,TYPE_BOOL,TYPE_CHAR,TYPE_INT,TYPE_FLOAT,TYPE_STRING,TYPE_FUNCTION,

    TYPE_FUNCTION_C,
    TYPE_FUNCTION_C_DD = TYPE_FUNCTION_C,TYPE_FUNCTION_C_II,TYPE_FUNCTION_C_IS,
};

typedef struct var {
    unsigned type;
    union {
        uintptr_t any;  // 'void*', 'null'
        int64_t i;      // 'integer', 'bool' and 'char'
        double d;       // 'float' and 'double'
        char* s;        // 'string'

        // array(struct var) l; // 'list'
        // map(struct var *, struct var *) m; // 'map'

        union {
            struct var (*call)(int argc, struct var* argv);
            double (*call_c_dd)(double);
            int (*call_c_ii)(int);
            int (*call_c_is)(char*);
        };
    };
} var;

#define Error ((var){ TYPE_NULL, .s = "(null)error" })
#define False ((var){ TYPE_BOOL, .i = 0 })
#define True  ((var){ TYPE_BOOL, .i = 1 })

// symbols
array(var) sym[128] = {0};
var find(const char *k) {
    array(var) *depot = &sym[k[0]-'@'];
    for( int i = 0; i < array_count(*depot); i += 2 ) {
        if( !strcmp(k, (*depot)[i].s) ) {
            return (*depot)[i+1];
        }
    }
    return Error;
}
var insert(const char *k, var v) {
    var found = find(k);
    if( found.type == Error.type ) {
        array(var) *depot = &sym[k[0]-'@'];
        array_push(*depot, ((var){TYPE_STRING, .s=strdup(k)}));
        array_push(*depot, v);
        return v;
    }
    return found;
}
var update(const char *k, var v) {
    array(var) *depot = &sym[k[0]-'@'];
    for( int i = 0; i < array_count(*depot); i += 2 ) {
        if( !strcmp(k, (*depot)[i].s) ) {
            return (*depot)[i+1] = v;
        }
    }
    array_push(*depot, ((var){TYPE_STRING, .s=strdup(k)}));
    array_push(*depot, v);
    return v;
}

// utils
char* as_string(var x) {
//  if(x.type==TYPE_NULL)     return va("%s", "(null)");
    if(x.type==TYPE_BOOL)     return va("%s", x.i ? "true" : "false");
    if(x.type==TYPE_CHAR)     return va("%c", (char)x.i);
    if(x.type==TYPE_INT)      return va("%" PRId64, x.i);
    if(x.type==TYPE_FLOAT)    return va("%f", x.d); // %-9g
    if(x.type==TYPE_STRING)   return va("%s", x.s ? x.s : "(null)");
    if(x.type==TYPE_FUNCTION) return va("%p", x.call);
    if(x.type>=TYPE_FUNCTION_C)  return va("%p", x.call_c_ii);
    return Error.s;
}
debug(var x) {
//  if(x.type==TYPE_NULL)     printf("NULL: %s\n", as_string(x) );
    if(x.type==TYPE_BOOL)     printf("BOOL: %s\n", as_string(x) );
    if(x.type==TYPE_CHAR)     printf("CHAR: '%s'\n", as_string(x) );
    if(x.type==TYPE_INT)      printf("INT: %s\n", as_string(x) );
    if(x.type==TYPE_FLOAT)    printf("FLOAT: %s\n", as_string(x) );
    if(x.type==TYPE_STRING)   printf("STRING: \"%s\"\n", as_string(x) );
    if(x.type==TYPE_FUNCTION) printf("FUNCTION: %s\n", as_string(x) );
    if(x.type>=TYPE_FUNCTION_C)  printf("FUNCTION_C: %s\n", as_string(x) );
}
var len(int argc, var* x) {
    if( argc != 1 ) return Error;
    if(x->type==TYPE_BOOL)     return (var){ TYPE_INT, 1 };
    if(x->type==TYPE_CHAR)     return (var){ TYPE_INT, 4 };
    if(x->type==TYPE_INT)      return (var){ TYPE_INT, sizeof(x->i) };
    if(x->type==TYPE_FLOAT)    return (var){ TYPE_INT, sizeof(x->d) };
    if(x->type==TYPE_STRING)   return (var){ TYPE_INT, !!x->s * strlen(x->s) };
    if(x->type==TYPE_FUNCTION) return (var){ TYPE_INT, sizeof(x->call) };
    if(x->type>=TYPE_FUNCTION_C)  return (var){ TYPE_INT, sizeof(x->call_c_ii) };
    return Error;
}
var hash(int argc, var* x) {
    if( argc != 1 ) return Error;
    if(x->type==TYPE_BOOL)     return (var){ TYPE_INT, x->i };
    if(x->type==TYPE_CHAR)     return (var){ TYPE_INT, hash_u64(x->i) };
    if(x->type==TYPE_INT)      return (var){ TYPE_INT, hash_u64(x->i) };
    if(x->type==TYPE_FLOAT)    return (var){ TYPE_INT, hash_flt(x->i) };
    if(x->type==TYPE_STRING)   return (var){ TYPE_INT, !!x->s * !!x->s[0] * hash_str(x->s) };
    if(x->type==TYPE_FUNCTION) return (var){ TYPE_INT, hash_ptr(x->call) };
    if(x->type>=TYPE_FUNCTION_C)  return (var){ TYPE_INT, hash_ptr(x->call_c_ii) };
    return Error;
}
var index(int argc, var* x) {
    if( argc != 2 ) return Error;
    var *obj = &x[0];
    var *index = &x[1];
    if( index->type == TYPE_INT ) {
        // @todo: if( obj->type == TYPE_LIST ) { ... }
        if( obj->type==TYPE_STRING ) {
            // convert any positive/negative index into a positive form
            int len = !!obj->s * strlen(obj->s), pos = index->i;
            int indexPositive = pos && len ? (pos > 0 ? pos % len : len-1 + ((pos+1) % len)) : 0;
            return (var){ TYPE_INT, .i = indexPositive };
        }
    }
    return Error;
}

// conv
var string_to_var_n(const char *str) { // parse c_string->number (float,double,hex,integer)
    int is_float = !!strpbrk(str, ".fe"); // @fixme: assumes 'f' at eos or 'e' in between
    if( is_float ) return (var) { TYPE_FLOAT, .d = atof(str) };
    int is_hex = str[1] == 'x';
    if( is_hex ) return (var) { TYPE_INT, .i = strtol(str, NULL, 16) };
    return (var) { TYPE_INT, .i = atoi(str) };
}
var string_to_var_c(const char *str) { // parse c_string->char (a,\x20,\u1234)
    if( str[1-1] == '\\' && str[2-1] == 'x' ) return (var){TYPE_CHAR, .i = strtol(str+3-1, NULL, 16) };
    if( str[1-1] == '\\' && str[2-1] == 'u' ) return (var){TYPE_CHAR, .i = atoi(str+3-1) };
    if( str[1-1] != '\0' && str[2-1] == '\'') return (var){TYPE_CHAR, .i = str[1-1] };
    return Error;
}
var string_to_var_s(const char *str, unsigned num_truncated_ending_chars) { // parse c_string->string ('hi',"hi")
    var out = (var){TYPE_STRING, .s = strdup(str) };
    if( num_truncated_ending_chars ) out.s[ strlen(out.s) - num_truncated_ending_chars ] = '\0';
    return out;
}
var string_to_var_sf(const char *str, unsigned num_truncated_ending_chars) { // parse c_string->f-interpolated string (f'{hi}')
    const char *input = str;
    char *output = "";

    while(*input) {
        char *seek = strchr(input, '{'), *eos = seek ? strchr(seek+1, '}') : 0;
        if( seek && eos ) {
            char *symbol = va("%.*s", (int)(eos-seek-1), seek+1);
            var z = find(symbol);
            if (z.type != Error.type )
                output = va("%s%.*s%s", output, (int)(seek-input), input, as_string(z));
            else
                output = va("%s%.*s{%s}", output, (int)(seek - input), input, symbol);
            input = eos + 1;
        } else {
            if( num_truncated_ending_chars )
                output = va("%s%.*s", output, (int)(strlen(input)-num_truncated_ending_chars), input); // remove ending \"
            else
                output = va("%s%s", output, input);
            break;
        }
    }

    var out = (var){TYPE_STRING, .s = strdup(output) };
    return out;
}
var cast(int type, const char *str) {
    if( type == '_' ) { // identifier
        return find( str );
    }
    if( type == '0' ) { // number
        return string_to_var_n(str);
    }
    if( type == '\"' ) { // char or string
        if( str[0] == 'f' ) { // f-interpolated string
            return string_to_var_sf(str+2, 1);
        }
        if( str[0] == '\'') { // char
            var z = string_to_var_c(str+1);
            if(z.type != Error.type) return z;
        }
        // regular \" or \'
        var z = string_to_var_s(str+1, 1);
        return z;
    }
    return Error;
}

// -----------------------------------------------------------------------------
#line 1 "api.c"
#include <math.h>

add_int(const char *k, int64_t value) {
    insert(k, ((var){ TYPE_INT, .i = value }) );
}
add_double(const char *k, double value) {
    insert(k, ((var){ TYPE_FLOAT, .d = value }) );
}
add_string(const char *k, const char *value) {
    insert(k, ((var){ TYPE_STRING, .s = strdup(value) }) );
}
add_function(const char *k, var (func)(int, var*) ) {
    insert(k, ((var){ TYPE_FUNCTION, .call = func }));
}
add_function_C_ii(const char *k, int (func)(int) ) {
    insert(k, ((var){ TYPE_FUNCTION_C_II, .call_c_ii = func }));
}
add_function_C_dd(const char *k, double (func)(double) ) {
    insert(k, ((var){ TYPE_FUNCTION_C_DD, .call_c_dd = func }));
}
add_function_C_is(const char *k, int (func)(const char *) ) {
    insert(k, ((var){ TYPE_FUNCTION_C_IS, .call_c_is = func }));
}
// add_source
// add_dll

init() {
    insert("true", True);
    insert("false", False);
    add_int("_VERSION", 100);
    add_string("_VENDOR", "minc" );
    add_string("_BUILD", __DATE__ " " __TIME__);
    add_function("len", len);
    add_function("hash", hash);
    add_function("index", index);
}

var eval(const node *self) {
    // keywords
    if( !strcmp(self->debug, "if") ) {
        var cond = eval(self->sibling->child);
        if( cond.i ) return eval(self->sibling->sibling);
        if( self->sibling->sibling->sibling->debug && !strcmp(self->sibling->sibling->sibling->debug, "else") ) {
            return eval(self->sibling->sibling->sibling->sibling);
        }
        return False;
    }
    if( !strcmp(self->debug, "while") ) {
        int pass = 0;
        var cond = eval(self->sibling->child);
        while( cond.i ) eval(self->sibling->sibling), cond = eval(self->sibling->child), pass = 1;
        return pass ? True : False;
    }
    if( !strcmp(self->debug, "do") && !strcmp(self->sibling->sibling->debug, "while")  ) {
        do {
            /*puts(as_string(*/eval(self->sibling->child)/*))*/;
        } while( eval(self->sibling->sibling->sibling->child).i );
        return True;
    }
    // call
    if( self->type == '_' && self->sibling->type == '(' ) {
        var fn = find(self->debug);
        if( fn.type != TYPE_FUNCTION && !(fn.type >= TYPE_FUNCTION_C) ) return Error;

        int argc = 0;
        var argv[16] = {0};
        for( node *c = self->sibling->child; c->type; c = c->sibling, ++argc ) {
            int slot = (argc+1)/2; // assert(slot < 16);
            argv[slot] = cast(c->type, c->debug);
        }

        if( fn.type == TYPE_FUNCTION ) {
            return fn.call( (argc+1) / 2, argv );
        }
        if( fn.type == TYPE_FUNCTION_C_II && argc == 1 ) {
            // @todo: expand cases
            if( argv[0].type == TYPE_INT  )   return ((var){ TYPE_INT, .i = fn.call_c_ii(argv[0].i) });
            if( argv[0].type == TYPE_FLOAT)   return ((var){ TYPE_INT, .i = fn.call_c_ii(argv[0].d) });
            if( argv[0].type == TYPE_STRING)  return ((var){ TYPE_INT, .i = fn.call_c_ii(atoi(argv[0].s)) });
            return Error;
        }
        if( fn.type == TYPE_FUNCTION_C_DD && argc == 1 ) {
            // @todo: expand cases
            if( argv[0].type == TYPE_INT  )   return ((var){ TYPE_FLOAT, .d = fn.call_c_dd(argv[0].i) });
            if( argv[0].type == TYPE_FLOAT)   return ((var){ TYPE_FLOAT, .d = fn.call_c_dd(argv[0].d) });
            if( argv[0].type == TYPE_STRING)  return ((var){ TYPE_FLOAT, .d = fn.call_c_dd(atof(argv[0].s)) });
            return Error;
        }
        if( fn.type == TYPE_FUNCTION_C_IS && argc == 1 ) {
            // @todo: expand cases
            if( argv[0].type == TYPE_INT  )   return ((var){ TYPE_INT, .i = fn.call_c_is(va("%d",(int)argv[0].i)) });
            if( argv[0].type == TYPE_FLOAT)   return ((var){ TYPE_INT, .i = fn.call_c_is(va("%f",argv[0].d)) });
            if( argv[0].type == TYPE_STRING)  return ((var){ TYPE_INT, .i = fn.call_c_is(argv[0].s) });
            return Error;
        }
        return Error;
    }
    // binary
    if( self->sibling->type == '+' ) {
        // @fixme: reduce rhs to a single node. a=10-3 becomes 10 otherwise
        // @fixme: specialize: str concat, str trim, no logical ops on floats, etc
        var x,y;
        const node *operator = self->sibling;
        const node *next = self->sibling->sibling;
        if( operator->debug[1] == '=' )
        switch( operator->debug[0] ) {
            break; default: printf("operator%c= not implemented\n", operator->debug[0] );
            break; case '!': return ((var){ TYPE_BOOL, .i = cast(self->type, self->debug).any != cast(next->type, next->debug).any }); // @fixme: allow 1.0 != 2 comparison
            break; case '=': return ((var){ TYPE_BOOL, .i = cast(self->type, self->debug).any == cast(next->type, next->debug).any }); // @fixme: allow 1.0 == 1 comparison
            break; case '<': return ((var){ TYPE_BOOL, .i = cast(self->type, self->debug).any <= cast(next->type, next->debug).any }); // @fixme: allow 1.0 <= 1 comparison
            break; case '>': return ((var){ TYPE_BOOL, .i = cast(self->type, self->debug).any >= cast(next->type, next->debug).any }); // @fixme: allow 1.0 >= 1 comparison
            #define binary_op(ch, ...) \
            break; case ch: x = cast(self->type, self->debug); y = cast(next->type, next->debug); __VA_ARGS__; return update(self->debug, x); // @fixme: other types
            binary_op('+', x.i+=y.i); binary_op('-', x.i-=y.i); binary_op('*', x.i*=y.i); binary_op('/', x.i/=y.i); binary_op('%', x.i%=y.i);
            binary_op('&', x.i&=y.i); binary_op('|', x.i|=y.i); binary_op('^', x.i^=y.i);
            #undef binary_op
        }
        else if( operator->debug[0] == operator->debug[1] )
        switch( operator->debug[0] ) {
            break; default: printf("operator%c%c not implemented\n", operator->debug[0], operator->debug[0] );
            #define binary_op(ch, ...) \
            break; case ch: x = cast(self->type, self->debug); y = cast(next->type, next->debug); __VA_ARGS__; return x; // @fixme: other types
            binary_op('&', x.i = x.i && y.i ); binary_op('|', x.i = x.i || y.i ); binary_op('*', x.i = (int)pow(x.i, y.i) );
            #undef binary_op

            // v++
            break; case '+': x = y = cast(self->type, self->debug); ++x.i; return update(self->debug, x), y; // @fixme: other types
            // v--
            break; case '-': x = y = cast(self->type, self->debug); --x.i; return update(self->debug, x), y; // @fixme: other types
        }
        else
        switch( operator->debug[0] ) {
            break; default: printf("operator%c not implemented\n", operator->debug[0] );
            break; case '=': return update(self->debug, cast(next->type, next->debug));
            break; case '<': return ((var){ TYPE_BOOL, .i = cast(self->type, self->debug).any  < cast(next->type, next->debug).any }); // @fixme: allow 1.0 < 2 comparison
            break; case '>': return ((var){ TYPE_BOOL, .i = cast(self->type, self->debug).any  > cast(next->type, next->debug).any }); // @fixme: allow 2.0 > 1 comparison
            #define binary_op(ch, ...) \
            break; case ch: x = cast(self->type, self->debug); y = cast(next->type, next->debug); __VA_ARGS__; return x; // @fixme: other types
            binary_op('+', x.i+=y.i); binary_op('-', x.i-=y.i); binary_op('*', x.i*=y.i); binary_op('/', x.i/=y.i); binary_op('%', x.i%=y.i);
            binary_op('&', x.i&=y.i); binary_op('|', x.i|=y.i); binary_op('^', x.i^=y.i);
            #undef binary_op
        }
    }
    // unary
    if( self->type == '0' ) {
        return cast(self->type, self->debug);
    }
    if( self->type == '_' ) {
        return find(self->debug);
    }
    if( self->type == '+' ) {
        const char *operator = self->debug;
        var x = cast(self->sibling->type, self->sibling->debug);
        // ++v
        if( operator[0] == '+' && operator[1] == '+' ) return update(self->sibling->debug, ((var){ TYPE_INT, .i = x.i+1 })); // @fixme: allow other types
        // --v
        if( operator[0] == '-' && operator[1] == '-' ) return update(self->sibling->debug, ((var){ TYPE_INT, .i = x.i-1 })); // @fixme: allow other types
        if( operator[0] == '!' ) return ((var){ TYPE_INT, .i = !x.i }); // @fixme: allow other types
        if( operator[0] == '~' ) return ((var){ TYPE_INT, .i = ~x.i }); // @fixme: allow other types
        if( operator[0] == '+' ) return ((var){ TYPE_INT, .i =  x.i }); // @fixme: allow other types
        if( operator[0] == '-' ) return ((var){ TYPE_INT, .i = -x.i }); // @fixme: allow other types
        return Error;
    }
    return Error;
}

// -----------------------------------------------------------------------------
#line 1 "repl.c"
#include <io.h>

#ifdef _WIN32
#define isatty _isatty
#define fileno _fileno
#endif

repl() {
    int is_piped = !isatty(fileno(stdin)); // is stdin a terminal or a pipe/file?
    char *prompt = is_piped ? "" : ">";
    for( printf("%s", prompt); !fflush(stdout) && !feof(stdin); ) {
        char line[256] = {0};
        if( feof(stdin) || fgets(line, sizeof(line), stdin) < 0 ) break;
        int len = strlen(line); while(len && line[len-1] <= 32) line[--len] = '\0';
        if( !line[0] ) continue;

        if( is_piped ) {
            printf(">%s\n", line );
        }

        node *last = env;
        int read = parse(line, line + strlen(line));

        var out = eval(last);
        debug(out);

        printf("%s%s%s", out.type == TYPE_NULL ? "?" : "", out.type == TYPE_VOID ? "" : "\n", prompt);
    }
}

// -----------------------------------------------------------------------------
#line 1 "main.c"
#include <time.h>

var say(int argc, var *argv) {
    for(int i = 0; i < argc; ++i ) {
        puts( as_string(argv[i]) );
    }
    return True;
}

main(int argc, char **argv) {
    time_t before, after;

    init();
    add_double("pi", 3.14159265359 );
    add_function("say", say);
    add_function_C_is("puts", puts);
    add_function_C_dd("sin", sin);
    add_function_C_dd("cos", cos);

    // interactive repl
    if( argc <= 1 ) {
        before = clock();
        repl();
        after = clock();
    }

    // read source file
    if( argc > 1 ) {
        const char *begin = 0, *end = 0;

        // read source file
        for( FILE *fp = fopen(argv[1], "rb"); fp; fclose(fp), fp = 0) {
            size_t len; fseek(fp, 0L, SEEK_END), len = ftell(fp), fseek(fp, 0L, SEEK_SET);
            begin = calloc(1, len + 1); // @leak
            end = begin + len;
            fread( (char*)begin, 1, len, fp );
        }
        if( !begin ) exit(-printf("cant read '%s' file\n", argv[1]));

        // parse source
        before = clock();
        int read = parse(begin, end);
        after = clock();

        printf("parsed %d/%d bytes\n", read, (int)(end + 1 - begin));
    }

    // hover mouse on debugger to inspect AST
    AST;

    printf("%05.2fs\n", 1. * (after - before) / CLOCKS_PER_SEC);
}
