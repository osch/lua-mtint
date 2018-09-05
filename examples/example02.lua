if _VERSION == "Lua 5.1" then return end

local llthreads = require("llthreads2.ex")
local mtmsg     = require("mtmsg")
local mtint     = require("mtint")
local threadOut = mtmsg.newbuffer()
local thread    = llthreads.new(function(outId)
                                    local mtmsg     = require("mtmsg")
                                    local mtint     = require("mtint")
                                    local threadOut = mtmsg.buffer(outId)
                                    local c = coroutine.create(function()
                                        coroutine.yield(mtint.id())
                                        threadOut:addmsg(mtint.id())
                                        local x = 1
                                        while true do x = x + 1 end
                                    end)
                                    local ok, cid = coroutine.resume(c)
                                    assert(ok and cid == mtint.id(c))
                                    local ok, err = coroutine.resume(c)
                                    assert(not ok and err:match(mtint.error.interrupted))
                                end,
                                threadOut:id())
thread:start()
local interruptibleId = threadOut:nextmsg()
mtint.interrupt(interruptibleId)
assert(thread:join())
