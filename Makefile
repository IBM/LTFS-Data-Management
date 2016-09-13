include components.mk

.PHONY: build clean

default: build

SEP = >
addtgtprefix = $(addprefix $(1)$(SEP), $(2))
remtgtprefix = $(subst $(1)$(SEP),,$(2))

BUILDDIRS := $(call addtgtprefix, build, $(COMMONDIRS) $(CLIENTDIRS) $(SERVERDIRS))
build: $(BUILDDIRS)
$(BUILDDIRS):
	$(MAKE) -C $(call remtgtprefix, build, $@) build


CLEANDIRS := $(call addtgtprefix, clean, $(COMMONDIRS) $(CLIENTDIRS) $(SERVERDIRS))
clean: $(CLEANDIRS)
$(CLEANDIRS):
	$(MAKE) -C $(call remtgtprefix, clean, $@) clean
