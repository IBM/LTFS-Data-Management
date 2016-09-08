include components.mk

export BUILD_ROOT := $(CURDIR)
#export getrelpath = $(shell echo $(1) |sed -e s/.*$(PROJECT)\\///g -e s/[^/]*/../g)


.PHONY: build

SEP := >
addtgtprefix = $(addprefix $(1)$(SEP), $(2))
remtgtprefix = $(subst $(1)$(SEP),,$(2))

BUILDDIRS = $(call addtgtprefix, build, $(CLIENTDIRS) $(SERVERDIRS) $(COMMONDIRS))
build: $(BUILDDIRS)
$(BUILDDIRS):
	$(MAKE) -C $(call remtgtprefix, build, $@) build


CLEANDIRS = $(call addtgtprefix, clean, $(CLIENTDIRS) $(SERVERDIRS) $(COMMONDIRS))
clean: $(CLEANDIRS)
$(CLEANDIRS):
	$(MAKE) -C $(call remtgtprefix, clean, $@) clean


all: build
