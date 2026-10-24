local called = 0
local function mark() called = called + 1 return 42 end
assert((false && mark()) == false)
assert(called == 0)
assert((true && mark()) == 42)
assert(called == 1)
assert((0 && "zero is truthy") == "zero is truthy")
