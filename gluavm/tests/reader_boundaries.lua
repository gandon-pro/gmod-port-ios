local code = "local x = 2/*\r\ncomment **/ + 3 // line\r\nreturn x"
local at = 0
local f = assert(load(function()
    at = at + 1
    if at > #code then return nil end
    return string.sub(code, at, at)
end, "@one-byte-reader.lua"))
assert(f() == 5)
