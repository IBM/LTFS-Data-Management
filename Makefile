include components.mk

.PHONY: build clean fuse dmapi prepare

# for executing code
export PATH := $(PATH):$(CURDIR)/bin

export ROOTDIR := $(CURDIR)

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

all: build

BUILDDIRS := $(call addtgtprefix, build, $(COMMONDIRS) $(CONNECDIRS) $(CLIENTDIRS) $(SERVERDIRS))
build: prepare $(BUILDDIRS)
$(BUILDDIRS):
	$(MAKE) -C $(call remtgtprefix, build, $@) build


CLEANDIRS := $(call addtgtprefix, clean, $(COMMONDIRS) $(CONNECDIRS) $(CLIENTDIRS) $(SERVERDIRS))
clean: $(CLEANDIRS)
$(CLEANDIRS):
	$(MAKE) -C $(call remtgtprefix, clean, $@) clean


prepare:
	@if [ -d ".git/hooks" ]; then \
		ln -sf ../../.hooks/pre-commit .git/hooks; \
		ln -sf ../../.hooks/post-commit .git/hooks; \
		ln -sf ../../.hooks/pre-push .git/hooks; \
	fi
