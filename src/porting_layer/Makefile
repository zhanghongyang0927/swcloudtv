
target_to_system_api_map := \
  test:none mac:mac linux:posix win:windows \
  vip1002:posix vip1113:posix vip1853:posix vip4302:posix albis.8073:posix albis.8083:posix \
  mediaroom:wince metrowerks:stub

SYSTEM_API := $(subst $(TARGET):,,$(filter $(TARGET):%,$(target_to_system_api_map)))

# Overwrite native API with the detected system
ifeq ($(SYSTEM_API),native)
  SYSTEM_API :=
  ifeq ($(OS),Windows_NT) # MingW variable
    SYSTEM_API := windows
  else
    UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S),Linux)
      SYSTEM_API := posix
    endif
    ifeq ($(UNAME_S),Darwin)
      SYSTEM_API := mac
    endif
  endif
endif

ifeq ($(SYSTEM_API),)
  $(error No system API exists for given target: $(TARGET))
endif

SRC_DIR := src/generic src/$(SYSTEM_API)

ifeq ($(SYSTEM_API),wince)
  SOURCE_FILES := $(foreach dir,$(SRC_DIR),$(wildcard $(dir)/*.c $(dir)/*.cpp $(dir)/*.cc))
  SOURCE_FILES += src/windows/Socket.cpp
  SOURCE_FILES += src/windows/Semaphore.cpp
  SOURCE_FILES += src/windows/Condition.cpp
  SOURCE_FILES += src/windows/Time.cpp
endif

ifeq ($(SYSTEM_API),windows)
  SOURCE_FILES := $(foreach dir,$(SRC_DIR),$(wildcard $(dir)/*.c $(dir)/*.cpp $(dir)/*.cc))
  SOURCE_FILES += src/posix/Mutex.cpp
  SOURCE_FILES += src/posix/Thread.cpp
  SOURCE_FILES += src/posix/FileSystem.cpp
endif

ifeq ($(SYSTEM_API),mac)
  SOURCE_FILES := $(foreach dir,$(SRC_DIR),$(wildcard $(dir)/*.c $(dir)/*.cpp $(dir)/*.cc))
  SOURCE_FILES += src/posix/Keyboard.cpp
  SOURCE_FILES += src/posix/Mutex.cpp
  SOURCE_FILES += src/posix/Socket.cpp
  SOURCE_FILES += src/posix/Thread.cpp
  SOURCE_FILES += src/posix/ThreadSleep.cpp
  SOURCE_FILES += src/posix/FileSystem.cpp
endif

LIB_NAME := porting_layer

include ../build_env/Makefile.include
