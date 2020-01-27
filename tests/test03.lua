local resume = coroutine.resume
local yield  = coroutine.yield

local llthreads = require("llthreads2.ex")
local mtmsg     = require("mtmsg")
local mtint     = require("mtint")


local function line()
    return debug.getinfo(2).currentline
end
local function PRINT(s)
    print(s.." ("..debug.getinfo(2).currentline..")")
end

PRINT("==================================================================================")
do
    local threadIn  = mtmsg.newbuffer()
    local threadOut = mtmsg.newbuffer()
    
    local thread = llthreads.new(function(threadInId, threadOutId)
                                    local loadstring = loadstring or load
                                    local mtmsg      = require("mtmsg")
                                    local mtint      = require("mtint")
                                    local threadIn   = mtmsg.buffer(threadInId)
                                    local threadOut  = mtmsg.buffer(threadOutId)
                                    local interruptCount  = 0
                                    local loopCount       = 0
                                    mtint.sethook(function()
                                        interruptCount = interruptCount + 1
                                        print(debug.traceback("Thread: interrupted "..interruptCount..": loopCount="..loopCount))
                                        local cmd = threadIn:nextmsg(0)
                                        if cmd == "CONTINUE" then
                                            print("Thread: Continue...")
                                            threadOut:addmsg("OK"..interruptCount)
                                        end
                                        if cmd == "QUIT" then
                                            print("Thread: quitting.")
                                            threadOut:addmsg("QUITTING")
                                            error("exit thread")
                                        end
                                    end)
                                    threadOut:addmsg(mtint.id())
                                    while true do 
                                        loopCount = loopCount + 1
                                        -- do some work, prevent jit compilation for LuaJIT:
                                        assert(loopCount == loadstring("return "..loopCount)())
                                    end
                                end,
                                threadIn:id(), threadOut:id())
    thread:start()
    local intId = threadOut:nextmsg()

    threadIn:addmsg("CONTINUE")
    mtint.interrupt(intId)
    assert("OK1" == threadOut:nextmsg())

    threadIn:addmsg("CONTINUE")
    mtint.interrupt(intId)
    assert("OK2" == threadOut:nextmsg())

    threadIn:addmsg("QUIT")
    mtint.interrupt(intId)
    assert("QUITTING" == threadOut:nextmsg())

    thread:join()
end
PRINT("==================================================================================")
do
    if _VERSION ~= "Lua 5.1" then
        local threadIn  = mtmsg.newbuffer()
        local threadOut = mtmsg.newbuffer()
        
        local thread = llthreads.new(function(threadInId, threadOutId)
                                        local loadstring = loadstring or load
                                        local mtmsg      = require("mtmsg")
                                        local mtint      = require("mtint")
                                        local threadIn   = mtmsg.buffer(threadInId)
                                        local threadOut  = mtmsg.buffer(threadOutId)
                                        local interruptCount  = 0
                                        local loopCount       = 0
                                        local co = coroutine.create(function()
                                            while true do 
                                                loopCount = loopCount + 1
                                                assert(loopCount == loadstring("return "..loopCount)())
                                            end
                                        end)
                                        mtint.sethook(co, function()
                                            interruptCount = interruptCount + 1
                                            print(debug.traceback("Thread: interrupted "..interruptCount..": loopCount="..loopCount))
                                            local cmd = threadIn:nextmsg(0)
                                            if cmd == "CONTINUE" then
                                                print("Thread: Continue...")
                                                threadOut:addmsg("OK"..interruptCount)
                                            end
                                            if cmd == "QUIT" then
                                                print("Thread: quitting.")
                                                threadOut:addmsg("QUITTING")
                                                error("exit thread")
                                            end
                                        end)
                                        threadOut:addmsg(mtint.id(co))
                                        coroutine.resume(co)
                                    end,
                                    threadIn:id(), threadOut:id())
        thread:start()
        local intId = threadOut:nextmsg()
    
        threadIn:addmsg("CONTINUE")
        mtint.interrupt(intId)
        assert("OK1" == threadOut:nextmsg())
    
        threadIn:addmsg("CONTINUE")
        mtint.interrupt(intId)
        assert("OK2" == threadOut:nextmsg())
    
        threadIn:addmsg("QUIT")
        mtint.interrupt(intId)
        assert("QUITTING" == threadOut:nextmsg())
    
        thread:join()
    end
end
PRINT("==================================================================================")
print("OK.")
