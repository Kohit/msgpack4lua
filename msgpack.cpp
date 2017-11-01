extern "C" {  
    #include "include/lua.h"  
    #include "include/lauxlib.h"  
}
#include <string>
#include <cstdlib>
#include <stdint.h>
#include <cstring>
#include <cmath>

#define CHAR(x) static_cast<char>(x)
#define U8(x) static_cast<uint8_t>(x)

static const int max_depth = 80;

/* macros fome msgpack-c */
#if defined(unix) || defined(__unix) || defined(__APPLE__) || defined(__OpenBSD__)
    #include <arpa/inet.h>  /* __BYTE_ORDER */
    #   if defined(linux)
    #       include <byteswap.h>
    #   endif
#endif

#define MSGPACK_ENDIAN_LITTLE_BYTE /****little-endian only****/
#ifdef MSGPACK_ENDIAN_LITTLE_BYTE

    #   if defined(unix) || defined(__unix) || defined(__APPLE__) || defined(__OpenBSD__)
    #       define _msgpack_be16(x) ntohs(x)
    #   else
    #       if defined(ntohs)
    #           define _msgpack_be16(x) ntohs(x)
    #       elif defined(_byteswap_ushort) || (defined(_MSC_VER) && _MSC_VER >= 1400)
    #           define _msgpack_be16(x) ((uint16_t)_byteswap_ushort((unsigned short)x))
    #       else
    #           define _msgpack_be16(x) ( \
                    ((((uint16_t)x) <<  8) ) | \
                    ((((uint16_t)x) >>  8) ) )
    #        endif
    #   endif

    #   if defined(unix) || defined(__unix) || defined(__APPLE__) || defined(__OpenBSD__)
    #       define _msgpack_be32(x) ntohl(x)
    #   else
    #       if defined(ntohl)
    #           define _msgpack_be32(x) ntohl(x)
    #       elif defined(_byteswap_ulong) || (defined(_MSC_VER) && _MSC_VER >= 1400)
    #           define _msgpack_be32(x) ((uint32_t)_byteswap_ulong((unsigned long)x))
    #       else
    #           define _msgpack_be32(x) \
                    ( ((((uint32_t)x) << 24)               ) | \
                    ((((uint32_t)x) <<  8) & 0x00ff0000U ) | \
                    ((((uint32_t)x) >>  8) & 0x0000ff00U ) | \
                    ((((uint32_t)x) >> 24)               ) )
    #       endif
    #   endif

    #   if defined(_byteswap_uint64) || (defined(_MSC_VER) && _MSC_VER >= 1400)
    #        define _msgpack_be64(x) (_byteswap_uint64(x))
    #   elif defined(bswap_64)
    #        define _msgpack_be64(x) bswap_64(x)
    #   elif defined(__DARWIN_OSSwapInt64)
    #        define _msgpack_be64(x) __DARWIN_OSSwapInt64(x)
    #   else
    #        define _msgpack_be64(x) \
                ( ((((uint64_t)x) << 56)                         ) | \
                ((((uint64_t)x) << 40) & 0x00ff000000000000ULL ) | \
                ((((uint64_t)x) << 24) & 0x0000ff0000000000ULL ) | \
                ((((uint64_t)x) <<  8) & 0x000000ff00000000ULL ) | \
                ((((uint64_t)x) >>  8) & 0x00000000ff000000ULL ) | \
                ((((uint64_t)x) >> 24) & 0x0000000000ff0000ULL ) | \
                ((((uint64_t)x) >> 40) & 0x000000000000ff00ULL ) | \
                ((((uint64_t)x) >> 56)                         ) )
    #   endif

#elif defined(MSGPACK_ENDIAN_BIG_BYTE)
    #   define _msgpack_be16(x) (x)
    #   define _msgpack_be32(x) (x)
    #   define _msgpack_be64(x) (x)
#else
    #   error msgpack-c supports only big endian and little endian
#endif /* MSGPACK_ENDIAN_LITTLE_BYTE */
/*
performance:
byteswap + assign > byteswap + memcpy > bit shift + memcpy > bit shift + string.append(each byte)
*/

// no safe check, make sure buf have enough size
#define store16(buf, tag, x) \
    do{ buf[0] = CHAR(tag); *(uint16_t *)&buf[1] = _msgpack_be16(static_cast<uint16_t>(x)); } while(0)
#define store32(buf, tag, x) \
    do{ buf[0] = CHAR(tag); *(uint32_t *)&buf[1] = _msgpack_be32(static_cast<uint32_t>(x)); } while(0)
#define store64(buf, tag, x) \
	do{ buf[0] = CHAR(tag); *(uint64_t *)&buf[1] = _msgpack_be64(static_cast<uint64_t>(x)); } while(0)
/* 
- do {} while(0) won't effect performance
- type cast won't effect performance
store16:
- movb tag, buf[0]    -- buf[0] = tag
- lea buf[0] eax    -- eax = &buf[0]
- add 1 eax         -- &buf[1]
- change endian
- movw dx (eax) -- buf[1] = dx

((uint16_t)x) <<  8):
load x
movzwl ax eax   --(uint32)x
 x << 8
store
*/

#define expect(psr, n) \
    do{ if(psr.len < psr.idx + n) error(L, "missing bytes(%d) at %d of %d", n, psr.idx, psr.len); } while(0)
#define read_byte(psr, n) \
    do{ expect(psr, 1); n = U8(psr.str[psr.idx++]); } while(0)
#define read_str(psr, n, buf) \
    do{ expect(psr, n); buf = &(psr.str[psr.idx]); psr.idx += n; } while(0)

#define load16(psr, n) \
    do{ expect(psr, 2); uint16_t* be = (uint16_t *)&(psr.str[psr.idx]); n = static_cast<uint16_t>(_msgpack_be16(*be)); psr.idx += 2; } while(0)
#define load32(psr, n) \
    do{ expect(psr, 4); uint32_t* be = (uint32_t *)&(psr.str[psr.idx]); n = static_cast<uint32_t>(_msgpack_be32(*be)); psr.idx += 4; } while(0)
#define load64(psr, n) \
    do{ expect(psr, 8); uint64_t* be = (uint64_t *)&(psr.str[psr.idx]); n = static_cast<uint64_t>(_msgpack_be64(*be)); psr.idx += 8; } while(0)
// store16: (int16) x = res; (automaticlly cut the higher bit) 
// load16:  x = (int16)res; (manually cut)
// load obj.value: 1.load obj, 2.load obj.value 

static int (*error)(lua_State * L, const char *fmt, ...) = luaL_error;
// note: error never returned

struct Parser{
    const char *str;
    size_t len = 0;
    size_t idx = 0;
    int depth = 0;
};
struct strbuf {
	char * str;
	size_t size = 0, len = 0;
	lua_State * L;
	strbuf(lua_State * l) {
		L = l;
		size = 1024;
		str = (char *)malloc(size);
		if (!str) error(L, "Out of memory\n");
	}
	~strbuf() { destroy(); }
	void destroy() { if (str) free(str); size = 0; len = 0; str = NULL; }
	void push_back(char c) {
		if (size <= len) expand(size + 1);
		str[len++] = c;
	}
	void append(const char * ptr, size_t count) {
		if (size <= len + count) expand(size + count);
		memcpy(str + len, ptr, count);
		len += count;
	}
	void expand(size_t count) {
		size_t old = size;
		size += count;
		if (size < old) {
			destroy();
			error(L, "exceeded maximum string length.\n");
		}
		str = (char *)realloc(str, size);
		if (!str) error(L, "Out of memory\n");
	}
	inline size_t length() { return len; }
	inline char * data() { return str; }
};

static void pack_str(lua_State * L, strbuf & buf){

    size_t len = 0;
    const char* str = lua_tolstring(L, -1, &len);

    if(len < 32) {
        buf.push_back(CHAR(0xa0 | len));
    } else if(len < 65536) {
        char sbuf[3];
        store16(sbuf, 0xda, len);
        buf.append(sbuf, 3);
    } else if(len <= 0xFFFFFFFFU) {
        char sbuf[5];
        store32(sbuf, 0xdb, len);
        buf.append(sbuf, 5);
    }else {
		buf.destroy();
		error(L, "exceeded maximum byte size of String object(2^32 - 1):%d", len);
	}

    buf.append(str, len);
}

static void append_buffer(lua_State *, strbuf &, int);

static void pack_map(lua_State * L, strbuf & buf, size_t len, int depth)
{
    if(len < 16) {
        buf.push_back(CHAR(0x80 | len));
    } else if(len < 65536) {
        char sbuf[3];
        store16(sbuf, 0xde, len);
        buf.append(sbuf, 3);
    } else if(len < (1LL << 32)) {   
        char sbuf[5];
        store32(sbuf, 0xdf, len);
        buf.append(sbuf, 5);
    }else {
		buf.destroy();
		error(L, "exceeded maximum number of Array object(2^32 - 1):%d", len);
	}
    lua_pushnil(L);
    /* table, nil */
    while (lua_next(L, -2) != 0) {
        /* table, key, value */
        lua_pushvalue(L, -2);
        /* table, key, value key*/                
        append_buffer(L, buf, depth);
        lua_pop(L, 1);

        /* table, key, value */
        append_buffer(L, buf, depth);
        lua_pop(L, 1);        
        /* table, key */
    }
}

static void pack_array(lua_State * L, strbuf & buf, size_t len, int depth)
{
    if(len < 16) {
        buf.push_back(CHAR(0x90 | len));
    } else if(len < 65536) {
        char sbuf[3];
        store16(sbuf, 0xdc, len);
        buf.append(sbuf, 3);
    } else if(len < (1LL << 31)) {   // note: lua_rawgeti use int index
        char sbuf[5];
        store32(sbuf, 0xdd, len);
        buf.append(sbuf, 5);
    }else {
		buf.destroy();
		error(L, "exceeded maximum number of Array object(2^31 - 1):%d", len);
	}
    for (size_t i = 1; i <= len; i++) {
        lua_rawgeti(L, -1, i);
        append_buffer(L, buf, depth);
        lua_pop(L, 1);
    }
}

// note: float to double have precision issus.
static void pack_double(strbuf & buf, double x){
    union { double f; uint64_t i; } mem;
    mem.f = x;
    char sbuf[9];
    store64(sbuf, 0xcb, mem.i);
    buf.append(sbuf, 9);
}

static void pack_integer(strbuf & buf, int64_t d){
    /*{
        if (d >= -(1LL<<5) && d < (1<<7)){
            buf.push_back(CHAR(d));        
        }else if(d < 0 && d >= -(1<<7)){   
            // signed 8
            buf.push_back(CHAR(0xd0u));
            buf.push_back(CHAR(d));
        }else if(d >= -(1LL<<15) && d < (1LL << 15)){   // 0000 is 0 not 1, 1111 is -1, so negativ have one more number 
            // signed 16
            char sbuf[3];
            sbuf[0] = CHAR(0xd1u); uint16_t be = _msgpack_be16(d); memcpy(sbuf + 1, &be, 2);
            buf.append(sbuf, 3);
        }else if(d >= -(1LL<<31) && d < (1LL<<31)){
            // signed 32
            char sbuf[5];
            sbuf[0] = CHAR(0xd2u); uint32_t be = _msgpack_be32(d); memcpy(sbuf + 1, &be, 4);
            buf.append(sbuf, 5);
        }else{
            // signed 64
            char sbuf[9];
            sbuf[0] = CHAR(0xd3u); uint64_t be = _msgpack_be64(d); memcpy(sbuf + 1, &be, 8);
            buf.append(sbuf, 9);
        }
    }*/
    if(d < -(1LL<<5)) { // d < -32
        if(d < -(1LL<<15)) {    // d < -32768(0x8000)
            if(d < -(1LL<<31)) {
                /* signed 64 */
                char sbuf[9];
                store64(sbuf, 0xd3, d);
                buf.append(sbuf, 9);
            } else {
                /* signed 32 */
                char sbuf[5];
                store32(sbuf, 0xd2, d);
                buf.append(sbuf, 5);
            }
        } else {
            if(d < -(1<<7)) {   // d < -128(0x80)
                /* signed 16 */
                char sbuf[3];
                store16(sbuf, 0xd1, d);
                buf.append(sbuf, 3);
            } else {    // -128 <= d < -32
                /* signed 8 */
                buf.push_back(CHAR(0xd0));
                buf.push_back(CHAR(d));
            }
        }
    } else if(d < (1<<7)) { // -32 <= d < 128
        /* fixnum */
        buf.push_back(CHAR(d));
    } else {
        if(d < (1LL<<15)) {
            /* signed 16 */
            char sbuf[3];
            store16(sbuf, 0xd1, d);
            buf.append(sbuf, 3);
        } else {
            if(d < (1LL<<31)) {
                /* signed 32 */
                char sbuf[5];
                store32(sbuf, 0xd2, d);
                buf.append(sbuf, 5);
            } else {
                /* signed 64 */
                char sbuf[9];
                store64(sbuf, 0xd3, d);
                buf.append(sbuf, 9);
            }
        }
    }
}

static size_t check_table(lua_State * L, bool & is_map){
    size_t max = 0, n = 0;
    double k = 0;
    lua_pushnil(L);
    /* table, nil */
    while (lua_next(L, -2) != 0) {
        /* table, key, value */
        if (lua_type(L, -2) == LUA_TNUMBER && (k = lua_tonumber(L, -2)) 
            && floor(k) == k && k >= 1) {
            /* Integer >= 1 ? */
            if (k > max) max = k;
        }else
            is_map = true;
        n++;
        lua_pop(L, 1);
    }

    /* sparse array */
    if (max != n) is_map = true;

    // empty table as empty array
    return n;
}

static void append_buffer(lua_State * L, strbuf & buf, int depth){
    size_t len = 0;
    double x = 0;
    bool is_map = false;
    switch (lua_type(L, -1)) {
        case LUA_TNIL:
            buf.push_back(CHAR(0xc0));
            break;
        case LUA_TBOOLEAN:
            if (lua_toboolean(L, -1))
                buf.push_back(CHAR(0xc3));
            else
                buf.push_back(CHAR(0xc2));
            break;
        case LUA_TSTRING:
            pack_str(L, buf);
            break;
        case LUA_TNUMBER:
            x = lua_tonumber(L, -1);
            // nan != nan, inf == inf
            if (floor(x) == x && !std::isinf(x))    // integer
                pack_integer(buf, x);
            else
                pack_double(buf, x);
            break;
        case LUA_TTABLE:
            if (++depth > max_depth || !lua_checkstack(L, 3)){
                buf.destroy();                
                error(L, "exceeded maximum nesting depth(%d)", max_depth);
            }
            len = check_table(L, is_map);
            if (!is_map) 
                pack_array(L, buf, len, depth);
            else 
                pack_map(L, buf, len, depth);
            break;
        default:
            buf.destroy();        
            error(L, "type not supported");
    }
}

static int pack(lua_State * L){
    luaL_argcheck(L, lua_gettop(L) == 1, 1, "expected 1 argument");
    strbuf buf(L);    
    append_buffer(L, buf, 0);
    lua_pushlstring(L, buf.data(), buf.length());
    return 1;
}

////////////////unpack//////////////////

void unpack_any(lua_State*, Parser&);
    
void unpack_array(lua_State * L, Parser& psr, size_t n){
    if(++(psr.depth) > max_depth || !lua_checkstack(L, 2))
        error(L, "exceeded maximum nesting depth(%d) at character %d", max_depth, psr.idx);

    lua_newtable(L);
    for(size_t i = 1; i <= n; i++){
        unpack_any(L, psr);
        lua_rawseti(L, -2, i);
    }    

    psr.depth--;
}
void unpack_map(lua_State * L, Parser& psr, size_t n){
    if(++(psr.depth) > max_depth || !lua_checkstack(L, 3))
        error(L, "exceeded maximum nesting depth(%d) at character %d", max_depth, psr.idx);
    
    lua_newtable(L);
    while(n--){
        unpack_any(L, psr);  // key
        unpack_any(L, psr);  // value
        lua_settable(L, -3);        
    }  

    psr.depth--;    
}

void unpack_any(lua_State * L, Parser & psr){
    uint8_t tag = 0;
    read_byte(psr, tag);
    union { double f; uint64_t i; } n;    
    const char * str;
    switch(tag) {
    case 0xc0:  // nil
        lua_pushnil(L); break;
    case 0xc2:  // false
        lua_pushboolean(L, 0); break;
    case 0xc3:  // true
        lua_pushboolean(L, 1); break;
    case 0xd0:  // int 8
        read_byte(psr, n.i); lua_pushnumber(L, CHAR(n.i)); break;
    case 0xd1:  // int 16
        load16(psr, n.i); lua_pushnumber(L, (int16_t)n.i); break;
    case 0xd2:  // int 32
        load32(psr, n.i); lua_pushnumber(L, (int32_t)n.i); break;
    case 0xd3:  // int 64
        load64(psr, n.i); lua_pushnumber(L, (int64_t)n.i); break;
    case 0xcc:  // uint 8
        read_byte(psr, n.i); lua_pushnumber(L, U8(n.i)); break;
    case 0xcd:  // uint 16
        load16(psr, n.i); lua_pushnumber(L, (uint16_t)n.i); break;
    case 0xce:  // uint 32
        load32(psr, n.i); lua_pushnumber(L, (uint32_t)n.i); break;
    case 0xcf:  // uint 64
        load64(psr, n.i); lua_pushnumber(L, (uint64_t)n.i); break;
    case 0xca:  // float
        { 
            union{ float f; uint32_t i;} mem;
            load32(psr, mem.i);
            lua_pushnumber(L, mem.f);   // note: float to double have precision issue.
            break;        
        }
    case 0xcb:  // double
        load64(psr, n.i); lua_pushnumber(L, n.f); break;
    case 0xd9:  // str 8
        read_byte(psr, n.i); read_str(psr, n.i, str); lua_pushlstring(L, str, n.i); break;
    case 0xda:  // str 16
        load16(psr, n.i); read_str(psr, n.i, str); lua_pushlstring(L, str, n.i); break;
    case 0xdb:  // str 32
        load32(psr, n.i); read_str(psr, n.i, str); lua_pushlstring(L, str, n.i); break;
    case 0xdc:  // array 16
        load16(psr, n.i); unpack_array(L, psr, n.i); break;
    case 0xdd:  // array 32
        load32(psr, n.i); unpack_array(L, psr, n.i); break;
    case 0xde:  // map 16
        load16(psr, n.i); unpack_map(L, psr, n.i); break;
    case 0xdf:  // map 32
        load32(psr, n.i); unpack_map(L, psr, n.i); break;
    default:    // mix in first byte
        if ((tag & 0x80) == 0 || (tag & 0xe0) == 0xe0) {   // [-32, 128)
            lua_pushnumber(L, CHAR(tag));
        } else if ((tag & 0xe0) == 0xa0) {  // fixstr
            n.i = tag & 0x1f; read_str(psr, n.i, str); lua_pushlstring(L, str, n.i);
        } else if ((tag & 0xf0) == 0x80) {  // fixmap
            n.i = tag & 0xf; unpack_map(L, psr, n.i);
        } else if ((tag & 0xf0) == 0x90) {  // fixarray
            n.i = tag & 0xf; unpack_array(L, psr, n.i);
        } else {
            error(L, "type not supported at character %d", psr.idx);
        }
    }
}

static int unpack(lua_State * L){
    luaL_argcheck(L, lua_gettop(L) == 1, 1, "expected 1 argument");
    luaL_argcheck(L, lua_type(L, 1) == LUA_TSTRING, 1, "expected string");
    Parser psr;
    psr.str = lua_tolstring(L, 1, &psr.len);
    unpack_any(L, psr);
    if(psr.idx != psr.len) error(L, "extra bytes left, starts at character %d", psr.idx);
    return 1;    
}

static const luaL_Reg msgpack_funcs[] = {
    {"pack", pack},
    {"unpack", unpack},
    {NULL, NULL}
};

extern "C" int luaopen_msgpack(lua_State *L){
    lua_newtable(L);
	luaL_setfuncs(L, msgpack_funcs, 0);
	lua_setglobal(L, "msgpack");
    return 1;
}


