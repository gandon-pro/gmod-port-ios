/* comment containing "strings", //, --, and a lone *
   on several lines; this is not nested. */
local a = 1/**/+/**/2
assert(a == 3)
assert("/* not a comment */" == "/* not a comment */")
assert([==[/* inside long string */]==] == "/* inside long string */")
assert(loadstring("/* unfinished") == nil)
local f, err = loadstring("/* a\r\nb\r\nc */\r\nlocal =", "@lines.lua")
assert(f == nil && string.find(err, "lines.lua:4:", 1, true))
