if _VERSION == "Lua 5.1" then return end

local mtint = require("mtint")
local c = coroutine.create(function() end)
local id = mtint.id(c)
mtint.interrupt(id)
c = nil
collectgarbage()
local _, err = pcall(function()
    mtint.interrupt(id)
end)
assert(err:match(mtint.error.unknown_object))
