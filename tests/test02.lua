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
    print("MainId  ", mtint.id())

    local threadIn  = mtmsg.newbuffer()
    local threadOut = mtmsg.newbuffer()
    
    local thread = llthreads.new(function(threadInId, threadOutId)
                                    local resume    = coroutine.resume
                                    local yield     = coroutine.yield
                                    local mtmsg     = require("mtmsg")
                                    local mtint     = require("mtint")
                                    local threadIn  = mtmsg.buffer(threadInId)
                                    local threadOut = mtmsg.buffer(threadOutId)
                                    print("ThreadId", mtint.id())
                                    threadOut:addmsg("started")
                                    local ok, err = pcall(function()
                                        threadOut:addmsg(mtint.id())
                                        x = 0
                                        while true do 
                                            x = x + 1
                                            mtmsg.sleep(0) -- for LuaJIT
                                        end
                                    end)
                                    print("-------------------------------------")
                                    print("-- Thread: Expected error:")
                                    print(err)
                                    print("-------------------------------------")
                                    assert(not ok and err:match(mtint.error.interrupted))
                                    threadOut:addmsg(mtint.id())
                                    x = 0
                                    while true do 
                                        x = x + 1
                                        mtmsg.sleep(0) -- for LuaJIT
                                    end
                                end,
                                threadIn:id(), threadOut:id())
    thread:start()
    
    assert(threadOut:nextmsg() == "started")
    local tid = threadOut:nextmsg()
    assert(mtint.interrupt(tid))
    local tid = threadOut:nextmsg()
    assert(mtint.interrupt(tid))
    local ok, err = thread:join()
    print("-------------------------------------")
    print("-- Main: Expected error:")
    print(err)
    print("-------------------------------------")
    assert(not ok and err:match(mtint.error.interrupted))
end
PRINT("==================================================================================")
if _VERSION ~= "Lua 5.1" then

    local threadIn  = mtmsg.newbuffer()
    local threadOut = mtmsg.newbuffer()
    
    local thread = llthreads.new(function(threadInId, threadOutId)
                                    local resume    = coroutine.resume
                                    local yield     = coroutine.yield
                                    local mtmsg     = require("mtmsg")
                                    local mtint     = require("mtint")
                                    local threadIn  = mtmsg.buffer(threadInId)
                                    local threadOut = mtmsg.buffer(threadOutId)
                                    print("ThreadId", mtint.id())
                                    threadOut:addmsg("started")
                                    local cid
                                    local c = coroutine.create(function(arg)
                                        print("CorId    ", mtint.id())
                                        assert(cid == mtint.id())
                                        local ok, err = pcall(function()
                                            yield(100)
                                            while true do mtmsg.sleep(0.02) end
                                            yield(200)
                                        end)
                                        print("-------------------------------------")
                                        print("-- Co: Expected error:")
                                        print(err)
                                        print("-------------------------------------")
                                        assert(not ok and err:match(mtint.error.interrupted))
                                        local ok, err = pcall(function()
                                            yield(200)
                                            while true do mtmsg.sleep(0.02) end
                                            yield(300)
                                        end)
                                        assert(false)
                                    end)
                                    cid = mtint.id(c)
                                    local ok, rslt = resume(c)
                                    print(ok, rslt)
                                    assert(ok and rslt == 100)
                                    threadOut:addmsg(cid)
                                    local ok , rslt = resume(c)
                                    print(ok, rslt)
                                    threadOut:addmsg("continue1")
                                    local ok, err = resume(c)
                                    print("-------------------------------------")
                                    print("-- Thread: Expected error:")
                                    print(err)
                                    print("-------------------------------------")
                                    assert(not ok and err:match(mtint.error.interrupted))
                                    local ok, err = pcall(function()
                                        threadOut:addmsg(mtint.id())
                                        while true do mtmsg.sleep(0.02) end
                                    end)
                                    print("-------------------------------------")
                                    print("-- Thread: Expected error:")
                                    print(err)
                                    print("-------------------------------------")
                                    assert(not ok and err:match(mtint.error.interrupted))
                                    local ok, err = pcall(function()
                                        threadOut:addmsg("continue2")
                                        while true do mtmsg.sleep(0.02) end
                                    end)
                                    assert(false)
                                end,
                                threadIn:id(), threadOut:id())
    thread:start()
    
    assert(threadOut:nextmsg() == "started")
    local cid  = threadOut:nextmsg()
    local cint = mtint.interruptible(cid)
    assert(cint:interrupt())
    assert(threadOut:nextmsg() == "continue1")
    assert(cint:interrupt(true))
    local tid = threadOut:nextmsg()
    local tint = mtint.interruptible(tid)
    assert(tint:interrupt())
    assert(threadOut:nextmsg() == "continue2")
    mtmsg.sleep(0.1)
    assert(tint:interrupt(true))
    local ok, err = thread:join()
    print("-------------------------------------")
    print("-- Main: Expected error:")
    print(err)
    print("-------------------------------------")
    assert(not ok and type(err) == "string") -- error from llthreads
    assert(err:match("mtint.error.interrupted"))
    assert(mtint.interruptible(cid):id() == cid)
    assert(mtint.interruptible(tid):id() == tid)
    assert(not cint:interrupt())
    assert(not tint:interrupt())
    cint = nil
    tint = nil
    collectgarbage()
    local ok, err = pcall(function()
        mtint.interruptible(tid)
    end)
    print("-------------------------------------")
    print("-- Main: Expected error:")
    print(err)
    print("-------------------------------------")
    assert(not ok and err:match(mtint.error.unknown_object))
    local ok, err = pcall(function()
        mtint.interruptible(cid)
    end)
    print("-------------------------------------")
    print("-- Main: Expected error:")
    print(err)
    print("-------------------------------------")
    assert(not ok and err:match(mtint.error.unknown_object))
end
PRINT("==================================================================================")
print("OK.")
