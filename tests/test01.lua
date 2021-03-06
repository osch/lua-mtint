local resume = coroutine.resume
local yield  = coroutine.yield
local mtint  = require("mtint")

print("test01 started")
print("mtint._VERSION", mtint._VERSION)
print("mtint._INFO",    mtint._INFO)

local function line()
    return debug.getinfo(2).currentline
end
local function PRINT(s)
    print(s.." ("..debug.getinfo(2).currentline..")")
end

PRINT("==================================================================================")
do
    local id = mtint.id()
    assert(id == mtint.id())
    local intr = mtint.interruptible(id)
    assert(id == intr:id())
    local _, err = pcall(function()
        assert(intr:interrupt())
    end)
    print("-------------------------------------")
    print("-- Expected error:")
    print(err)
    print("-------------------------------------")
    assert(err:match(mtint.error.interrupted))
end
PRINT("==================================================================================")
do
    print("main id", mtint.id())
    local cid
    local c = coroutine.create(function(arg)
        print("co id", mtint.id())
        assert(cid == mtint.id())
        yield(100)
        yield(200)
        yield(300)
    end)
    local ok, rslt = pcall(function() return mtint.id(c) end)
    if _VERSION == "Lua 5.1" then
        print("-------------------------------------")
        print("-- Expected error:")
        print(rslt)
        print("-------------------------------------")
        assert(not ok and rslt:match(mtint.error.not_supported))
    else
        cid = rslt
        local ok, rslt = resume(c)
        print(ok, rslt)
        assert(ok and rslt == 100)
        local id = mtint.id()
        assert(cid ~= id)
        local intr = mtint.interruptible(cid)
        assert(intr:id() == cid)
        assert(intr:interrupt())
        local ok, err = resume(c)
        print("-------------------------------------")
        print("-- Expected error:")
        print(err)
        print("-------------------------------------")
        assert(not ok and err:match(mtint.error.interrupted))
        local ok, rslt = resume(c)
        print(ok, rslt)
        assert(not ok and rslt:match("cannot resume dead coroutine"))
    end    
end
PRINT("==================================================================================")
if _VERSION ~= "Lua 5.1" then
    local c = coroutine.create(function(arg)
        local ok, err = pcall(function()
            yield(100)
            yield(200)
            yield(300)
        end)
        print("-------------------------------------")
        print("-- Co Expected error:")
        print(err)
        print("-------------------------------------")
        assert(not ok and err:match(mtint.error.interrupted))
        local ok, err = pcall(function()
            yield(200)
            yield(300)
        end)
        assert(false)
    end)
    local ok, rslt = resume(c)
    print(ok, rslt)
    assert(ok and rslt == 100)
    local cid = mtint.id(c)
    local intr = mtint.interruptible(cid)
    assert(intr:id() == cid)
    assert(intr:interrupt())
    local ok, rslt = resume(c)
    assert(ok and rslt == 200)
    intr:interrupt(true)
    local ok, err = resume(c)
    print("-------------------------------------")
    print("-- Expected error:")
    print(err)
    print("-------------------------------------")
    assert(not ok and err:match(mtint.error.interrupted))
    assert(intr:interrupt())
    c = nil
    collectgarbage()
    assert(not intr:interrupt())
    assert(mtint.interruptible(cid):id() == intr:id())
    intr = nil
    collectgarbage()
    local ok, err = pcall(function()
        mtint.interruptible(cid)
    end)
    print("-------------------------------------")
    print("-- Expected error:")
    print(err)
    print("-------------------------------------")
    assert(not ok and err:match(mtint.error.unknown_object))
end
PRINT("==================================================================================")
do
    local c = coroutine.create(function() end)
    if _VERSION == "Lua 5.1" then
        local ok, err = pcall(function()
            mtint.sethook(c, function() end)
        end)
        print("-------------------------------------")
        print("-- Expected error:")
        print(err)
        print("-------------------------------------")
        assert(not ok and err:match(mtint.error.not_supported))
    else
        mtint.sethook(c, function() end)
    end
end
PRINT("==================================================================================")
do
    local c = coroutine.create(function() 
        mtint.sethook(function() end)
        yield("ok")
    end)
    local ok, rslt = resume(c)
    if _VERSION == "Lua 5.1" then
        print("-------------------------------------")
        print("-- Expected error:")
        print(rslt)
        print("-------------------------------------")
        assert(not ok and rslt:match(mtint.error.not_supported))
    else
        assert(ok and rslt == "ok")
    end
end
PRINT("==================================================================================")
do
    local c = coroutine.create(function() end)
    mtint.sethook()
    if _VERSION ~= "Lua 5.1" then
        mtint.sethook(c)
    end
end
PRINT("==================================================================================")
print("OK.")
