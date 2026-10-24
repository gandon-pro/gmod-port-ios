local called = 0
local function mark() called = called + 1 return 42 end
assert((true || mark()) == true)
assert(called == 0)
assert((nil || mark()) == 42)
assert(called == 1)
assert((false || "fallback") == "fallback")
