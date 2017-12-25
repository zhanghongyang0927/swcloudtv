
SOLIB_NAME := sdk
AGGLIB_NAME := sdk

IN_LIBS := core porting_layer stream http_client rplayer utils

ifneq ($(ENABLE_SSL),0)
    EXT_LIBS += ssl crypto
endif

include ../build_env/Makefile.include

.DEFAULT_GOAL := all

all: $(SOLIB_FILE) $(AGGLIB_FILE)
