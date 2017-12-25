
SUB_BUILDS := rplayer

.PHONY: all $(SUB_BUILDS)

all: $(SUB_BUILDS)

define ruletemp_subbuilds
$(1):
	@$(MAKE) -f Makefile.$(1)
endef

$(foreach sub,$(SUB_BUILDS),$(eval $(call ruletemp_subbuilds,$(sub))))
