# Copyright 2017 IBM Corp. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#  https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

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
