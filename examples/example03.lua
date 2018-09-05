local mtint = require("mtint")
local _, err = pcall(function() 
    mtint.interrupt(1)
end)
assert(err:match(mtint.error.unknown_object))

