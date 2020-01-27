  local mtint = require("mtint")
  local _, err = pcall(function() 
      mtint.interrupt(1)
  end)
  assert(err:match(mtint.error.unknown_object))
  assert(mtint.error.unknown_object == "mtint.error.unknown_object")

