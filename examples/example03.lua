local llthreads = require("llthreads2.ex")
local mtmsg     = require("mtmsg")
local mtint     = require("mtint")

local threadIn  = mtmsg.newbuffer()
local threadOut = mtmsg.newbuffer()

local thread = llthreads.new(function(threadInId, threadOutId)
                                local loadstring = loadstring or load
                                local mtmsg      = require("mtmsg")
                                local mtint      = require("mtint")
                                local threadIn   = mtmsg.buffer(threadInId)
                                local threadOut  = mtmsg.buffer(threadOutId)
                                local stop       = false
                                local counter    = 0
                                mtint.sethook(function()
                                    print("counter", counter)
                                    local cmd = threadIn:nextmsg()
                                    if cmd == "CONTINUE" then
                                        threadOut:addmsg("OK")
                                    end
                                    if cmd == "QUIT" then
                                        threadOut:addmsg("QUITTING")
                                        stop = true
                                    end
                                end)
                                threadOut:addmsg(mtint.id())
                                while not stop do 
                                    counter = counter + 1
                                    -- do some work, prevent jit compilation for LuaJIT:
                                    assert(counter == loadstring("return "..counter)())
                                end
                            end,
                            threadIn:id(), threadOut:id())
thread:start()
local intId = threadOut:nextmsg()

threadIn:addmsg("CONTINUE")
mtint.interrupt(intId)
assert("OK" == threadOut:nextmsg())

threadIn:addmsg("CONTINUE")
mtint.interrupt(intId)
assert("OK" == threadOut:nextmsg())

threadIn:addmsg("QUIT")
mtint.interrupt(intId)
assert("QUITTING" == threadOut:nextmsg())

thread:join()
