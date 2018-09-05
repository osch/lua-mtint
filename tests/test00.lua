local mtmsg = require("mtmsg")
local gcq = mtmsg.newbuffer()

local function newcref(cor, name)
    local ref = { cor = cor, name = name }
    setmetatable(ref, {
        __gc = function(self)
            print("gc:", self.name, "cor:", self.cor)
            gcq:addmsg(tostring(self.cor))
        end
    })
    return ref
end

local creg = {}
setmetatable(creg, {
    __mode = "k"
})

---------------------------------------------------------------------------------

local c1 = coroutine.create(function() end)
local c2 = coroutine.create(function() end)
local c3 = coroutine.create(function() end)
assert(type(c1) == "thread")
assert(type(c2) == "thread")
assert(type(c3) == "thread")
print(c1)
print(c2)
print(c3)
local c1s = tostring(c1)
local c2s = tostring(c2)
local c3s = tostring(c3)

creg[c1] = newcref(c1, "c1")
creg[c2] = newcref(c2, "c2")
creg[c3] = newcref(c3, "c3")
print("------")
c2 = nil
for i = 1, 1 do
    collectgarbage()
end

print("------")
assert(gcq:nextmsg() == c2s)
print("------")

c1 = nil
c3 = nil
collectgarbage()
print("------")
assert(gcq:nextmsg() == c3s)
assert(gcq:nextmsg() == c1s)



print("OK.")


