include components.mk

.PHONY: build clean fuse dmapi

# for executing code
export PATH := $(PATH):$(CURDIR)/bin

export ROOTDIR := $(CURDIR)

BUILD_HASH := 0x$(shell git rev-parse --short HEAD)
ifeq ($(BUILD_HASH), 0x)
    BUILD_HASH := 0
endif
export BUILD_HASH

COMMIT_COUNT := $(shell git rev-list --count HEAD)
ifeq ($(COMMIT_COUNT), )
    COMMIT_COUNT := 0
endif
export COMMIT_COUNT

export CONNECTOR := fuse
fuse: CONNECTOR = fuse
fuse: build
dmapi: CONNECTOR = dmapi
dmapi: build

ifeq ($(wildcard src/connector/$(CONNECTOR)),)
    $(error connector $(CONNECTOR) does not exit)
endif

SEP = >
addtgtprefix = $(addprefix $(1)$(SEP), $(2))
remtgtprefix = $(subst $(1)$(SEP),,$(2))

BUILDDIRS := $(call addtgtprefix, build, $(COMMONDIRS) $(CONNECDIRS) $(CLIENTDIRS) $(SERVERDIRS))
build: $(BUILDDIRS)
$(BUILDDIRS):
	$(MAKE) -C $(call remtgtprefix, build, $@) build


CLEANDIRS := $(call addtgtprefix, clean, $(COMMONDIRS) $(CONNECDIRS) $(CLIENTDIRS) $(SERVERDIRS))
clean: $(CLEANDIRS)
$(CLEANDIRS):
	$(MAKE) -C $(call remtgtprefix, clean, $@) clean
