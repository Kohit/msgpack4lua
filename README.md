# msgpack4lua
A simple yet fast c/c++ implementation of [MessagePack](https://msgpack.org/) for Lua

# usage example
```
require('msgpack')
local a = {'abc', 123, {1, 2, false, {'aaa', 11111111111.012, {-1}}}}
local p = msgpack.pack(a)
print(type(p))      -- string
local res = msgpack.unpack(p)
print(type(res))    -- table
```

# performance
below result comes from a simple test(loop a large table for 500 times)

                        encode          decode

    lua-MessagePack      6.26            2.72

    msgpack4lua          0.38            0.26

    lua_cmsgpack         0.45            0.33

# install
Default lua version is 5.2, you may need to change luaopen_msgpack function to adapt yours.

Simple build command with g++ on windows('[' and ']' are not included):

`
g++ msgpack.cpp -I[YourLuaHeaderPath] [YourLuaBinPath]/lua52.dll -shared -o msgpack.dll
`

# spec for pack
- Int format use signed integer
- Float format use double precision
- array with holes pack to Map format
- empty table pack to Array format

# reference
>[msgpack-c](https://github.com/msgpack/msgpack-c), 
[fperrad/lua-MessagePack](https://github.com/fperrad/lua-MessagePack),
[mpx/lua-cjson](https://github.com/mpx/lua-cjson)
