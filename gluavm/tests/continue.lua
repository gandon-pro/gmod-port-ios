local total, calls = 0, {}
for i = 1, 5 do
    local captured = i
    calls[i] = function() return captured end
    if i % 2 == 0 then continue end
    total = total + i
end
assert(total == 9)
for i = 1, 5 do assert(calls[i]() == i) end
local i = 0
while i < 5 do
    i = i + 1
    if i < 5 then continue end
    total = total + 100
end
assert(total == 109)
local rows = 0
for k,v in ipairs({1,2,3}) do
    if v == 2 then continue end
    rows = rows + v
end
assert(rows == 4)
local counter = 0
repeat
    counter = counter + 1
    if counter < 3 then continue end
until counter >= 3
assert(counter == 3)
assert(loadstring("continue") == nil)
assert(loadstring("while true do local function f() continue end end") == nil)
