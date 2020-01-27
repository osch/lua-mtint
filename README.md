# mtint 
[![Licence](http://img.shields.io/badge/Licence-MIT-brightgreen.svg)](LICENSE)
[![Build Status](https://travis-ci.org/osch/lua-mtint.svg?branch=master)](https://travis-ci.org/osch/lua-mtint)
[![Build status](https://ci.appveyor.com/api/projects/status/g5sijdrvdx6vviqr/branch/master?svg=true)](https://ci.appveyor.com/project/osch/lua-mtint/branch/master)
[![Install](https://img.shields.io/badge/Install-LuaRocks-brightgreen.svg)](https://luarocks.org/modules/osch/mtint)

<!-- ---------------------------------------------------------------------------------------- -->

Make threads and coroutines interruptible for the [Lua] scripting language.

This package provides a way to make arbitrary Lua interpreter states (i.e. main
states and couroutines) interruptible from concurrently running threads. 
The implementation is independent from the underlying threading 
library (e.g. [Lanes] or [lua-llthreads2]).

The general principle is to interrupt a Lua state by installing a debug hook 
that triggers an error. This can be useful for applications with interactive user 
interface when a user wants to abort a long running background task or a user 
supplied script that is stuck in an infinite loop.

This package is also available via LuaRocks, see https://luarocks.org/modules/osch/mtint.

[Lua]:               https://www.lua.org
[Lanes]:             https://luarocks.org/modules/benoitgermain/lanes
[lua-llthreads2]:    https://luarocks.org/modules/moteus/lua-llthreads2

See below for full [reference documentation](#documentation).

<!-- ---------------------------------------------------------------------------------------- -->

#### Requirements

   * Tested operating systems: Linux, Windows, MacOS
   * Other Unix variants: could work, but untested, required are:
      * gcc atomic builtins or C11 `stdatomic.h`
      * `pthread.h` or C11 `threads.h`
   * Tested Lua versions: 5.1, 5.2, 5.3, luajit 2.0 & 2.1
       * Lua 5.2 & 5.3: full support, main states and coroutines are interruptible.
       * Lua 5.1: coroutines cannot be interrupted, only main states.
       * LuaJIT: same restrictions than Lua 5.1, JIT compiled pure machine code 
                 not considering the debug hook cannot be interrupted.

<!-- ---------------------------------------------------------------------------------------- -->

## Examples

For the examples [llthreads2](https://luarocks.org/modules/moteus/lua-llthreads2)
is used as low level multi-threading implementation and 
[mtmsg](https://luarocks.org/modules/osch/mtmsg) is used for inter-thread
message passing.

#### Example 1

A parallel running thread passes its interruptible id as integer to 
the main thread and enters an infinite loop. The main thread takes the id and
interrupts the infinite loop:

```lua
local llthreads = require("llthreads2.ex")
local mtmsg     = require("mtmsg")
local mtint     = require("mtint")
local threadOut = mtmsg.newbuffer()
local thread    = llthreads.new(function(outId)
                                    local loadstring = loadstring or load
                                    local mtmsg      = require("mtmsg")
                                    local mtint      = require("mtint")
                                    local threadOut  = mtmsg.buffer(outId)
                                    threadOut:addmsg(mtint.id())
                                    local x = 1
                                    while true do 
                                        x = x + 1 
                                        -- do some work, prevent jit compilation for LuaJIT:
                                        assert(x == loadstring("return "..x)())
                                    end
                                end,
                                threadOut:id())
thread:start()
local interruptibleId = threadOut:nextmsg()
mtint.interrupt(interruptibleId)
local _, err = thread:join()
assert(err:match(mtint.error.interrupted))
```

The `loadstring(...)` is necessary for LuaJIT because otherwise the infinite loop would 
be compiled by the JIT to pure machine code that does not consider the lua debug 
hook which is needed for interrupting.

#### Example 2

In the second example a coroutine is interrupted while it runs concurrently in 
another thread. This example does only work with Lua 5.2 & 5.3 (interrupting 
coroutines is not supported for Lua 5.1).

```lua
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
```

#### Example 3

The interrupting main thread communicates via [mtmsg](https://luarocks.org/modules/osch/mtmsg)
with an interrupt handler on the interrupted thread:

```lua
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
```


<!-- ---------------------------------------------------------------------------------------- -->

## Documentation

   * [Module Functions](#module-functions)
       * mtint.id()
       * mtint.sethook()
       * mtint.interrupt()
   * [Errors](#errors)
       * mtint.error.interrupt
       * mtint.error.not_supported
       * mtint.error.unknown_object

<!-- ---------------------------------------------------------------------------------------- -->

### Module Functions

* **`mtint.id([co])`**

  Returns the interruptible id.
  
    * *co*  - optional coroutine whose interruptible id is returned. If not
              given, the interruptible id of the current running coroutine or
              main state is returned.

  Possible errors: *mtint.error.not_supported*



* **`mtint.sethook([co,]func)`**

  Sets an interrupt handler function. The interrupt handler is called on the interrupted thread 
  if *mtint.interrupt()* is called from somewhere else. If the interrupt handler is set 
  to `nil`, an error *mtint.error.interrupt* is raised on the interrupted thread.
  
  
    * *co*   - optional coroutine whose interrupt handler is to be set. If not
               given, the interrupt handler of the current running coroutine or
               main state is set.

    * *func* - interrupt handler function or `nil`.
    
  Possible errors: *mtint.error.not_supported*


* **`mtint.interrupt(id[,flag])`**

  * *id*   - integer, the interruptible id of a main state or coroutine that can
             be obtained by *mtint.id()*.

  * *flag* - optional boolean, if not specified or *nil* the state
             is only interrupted once, if *true* the state is interrupted
             at every operation again, if *false* the state is no
             longer interrupted.

  Possible errors: *mtint.error.unknown_object*


<!-- ---------------------------------------------------------------------------------------- -->

### Errors

* All errors raised by this module are string values. For evaluation purposes 
  special error strings are available in the table `mtint.error`, example:

  ```lua
  local mtint = require("mtint")
  local _, err = pcall(function() 
      mtint.interrupt(1)
  end)
  assert(err:match(mtint.error.unknown_object))
  assert(mtint.error.unknown_object == "mtint.error.unknown_object")
  ```

* **`mtint.error.interrupt`**

  The current state or coroutine has been interrupted by invoking *mtint.interrupt()*
  and no interrupt handler function was set via *mtint.sethook()*.

* **`mtint.error.not_supported`**

  Under Lua 5.1 it is not supported to obtain an interruptible id from a couroutine,
  only Lua main states are allowed.

* **`mtint.error.unknown_object`**

  An interruptible id has been given to *mtint.interrupt()* and the corresponding
  interruptible object cannot be found. One reason could be, that the object
  has been garbage collected, example:

  ```lua
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
  ```

End of document.

<!-- ---------------------------------------------------------------------------------------- -->
