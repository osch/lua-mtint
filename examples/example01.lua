local llthreads = require("llthreads2.ex")
local mtmsg     = require("mtmsg")
local mtint     = require("mtint")
local threadOut = mtmsg.newbuffer()
local thread    = llthreads.new(function(outId)
                                    local mtmsg     = require("mtmsg")
                                    local mtint     = require("mtint")
                                    local threadOut = mtmsg.buffer(outId)
                                    threadOut:addmsg(mtint.id())
                                    local x = 1
                                    while true do 
                                        x = x + 1 
                                        mtmsg.sleep(0) -- for LuaJIT
                                    end
                                end,
                                threadOut:id())
thread:start()
local interruptibleId = threadOut:nextmsg()
mtint.interrupt(interruptibleId)
local _, err = thread:join()
assert(err:match(mtint.error.interrupted))
