package = "mtint"
version = "scm-0"
source = {
  url = "https://github.com/osch/lua-mtint/archive/master.zip",
  dir = "lua-mtint-master",
}
description = {
  summary = "Make threads and coroutines interruptible",
  homepage = "https://github.com/osch/lua-mtint",
  license = "MIT/X11",
  detailed = [[
    This package provides a way to make arbitrary Lua interpreter states (i.e. main
    states and couroutines) interruptible from concurrently running threads. 
    The implementation is independent from the underlying threading 
    library (e.g. Lanes or lua-llthreads2).
  ]],
}
dependencies = {
  "lua >= 5.1, < 5.4",
}
build = {
  type = "builtin",
  platforms = {
    unix = {
      modules = {
        mtint = {
          libraries = {"pthread"},
        }
      }
    },
    windows = {
      modules = {
        mtint = {
          libraries = {"kernel32"},
        }
      }
    }
  },
  modules = {
    mtint = {
      sources = { 
          "src/main.c",
          "src/interruptible.c",
          "src/error.c",
          "src/util.c",
          "src/async_util.c",
          "src/mtint_compat.c",
      },
      defines = { "MTINT_VERSION="..version:gsub("^(.*)-.-$", "%1") },
    },
  }
}