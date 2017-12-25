
export TARGET?=linux
export PLATFORM?=linux
export OUTPUT_DIR?=output
export ENABLE_COVERAGE?=0
export ENABLE_DEBUG?=0
export ENABLE_SSL?=0
export ENABLE_CUSTOM_STL?=0
export CTVC_VERSION:=4.5.0-beta.1509959796

ifneq ($(firstword $(subst /,/ ,$(OUTPUT_DIR))),/)
  OUTPUT_DIR:=$(dir $(abspath $(lastword $(MAKEFILE_LIST))))/$(OUTPUT_DIR)
endif

.PHONY: all clean

all:
	make -C src/porting_layer
	make -C src/core
	make -C src/stream
	make -C src/http_client
	make -C src/submodules
	make -C src/utils
	make -C src
clean:
	make -C src/core clean
	make -C src/porting_layer clean
	make -C src/stream clean
	make -C src/http_client clean
	make -C src/submodules clean
	make -C src/utils clean
	make -C src clean

