include components.mk

.PHONY: build

SEP := >
addtgtprefix = $(addprefix $(1)$(SEP), $(2))
remtgtprefix = $(subst $(1)$(SEP),,$(2))

BUILDDIRS = $(call addtgtprefix, build, $(CLIENTDIRS) $(SERVERDIRS) $(COMMONDIRS))
build: $(BUILDDIRS)
$(BUILDDIRS):
	$(MAKE) -C $(call remtgtprefix, build, $@) build


all: build
