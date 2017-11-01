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

# install
Default lua version is 5.2, you may need to change luaopen_msgpack function to adapt yours.

Simple build command with g++ on windows('[' and ']' are not included):

`
g++ msgpack.cpp -I[YourLuaHeaderPath] [YourLuaBinPath]/lua52.dll -shared -o msgpack.dll
`

# spec for pack
- Int format use signed integer
- Float format use double precision
- Str 8 is not implemented
- array with holes pack to Map format
- empty table pack to Array format

note: this implementaion is compatible with [fperrad/lua-MessagePack](https://github.com/fperrad/lua-MessagePack), with the setting below:

```
set_string'string_compat'
set_integer'signed'
set_number'double'
set_array'without_hole'
```

# reference
>[msgpack-c](https://github.com/msgpack/msgpack-c), 
[fperrad/lua-MessagePack](https://github.com/fperrad/lua-MessagePack),
[mpx/lua-cjson](https://github.com/mpx/lua-cjson)
