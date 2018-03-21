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

.PHONY: build buildsrc buildtgt clean fuse dmapi prepare messages communication common connector client server

# for executing code
export PATH := $(PATH):$(CURDIR)/bin

export ROOTDIR := $(CURDIR)

export CONNECTOR_TYPE := fuse
fuse: CONNECTOR_TYPE = fuse
fuse: build
dmapi: CONNECTOR_TYPE = dmapi
dmapi: build

ifeq ($(wildcard src/connector/$(CONNECTOR_TYPE)),)
    $(error connector $(CONNECTOR_TYPE) does not exit)
endif

all: build

messages:
	$(MAKE) -C $(MESSAGES) build

communication: messages
	$(MAKE) -C $(COMMUNICATION) build

common: messages communication
	$(MAKE) -C $(COMMON) deps
	$(MAKE) -j -C $(COMMON) buildsrc
	$(MAKE) -C $(COMMON) buildtgt
	
connector: messages communication common
	$(MAKE) -C $(CONNECTOR) deps
	$(MAKE) -j -C $(CONNECTOR) buildsrc
	$(MAKE) -C $(CONNECTOR) buildtgt
	
client: messages communication common connector
	$(MAKE) -C $(CLIENT) deps
	$(MAKE) -j -C $(CLIENT) buildsrc
	$(MAKE) -C $(CLIENT) buildtgt
	
server: messages communication common connector
	$(MAKE) -C $(SERVER) deps
	$(MAKE) -j -C $(SERVER) buildsrc
	$(MAKE) -C $(SERVER) buildtgt
	
build: prepare messages communication connector client server

clean:
	$(MAKE) -C $(MESSAGES) clean
	$(MAKE) -C $(COMMUNICATION) clean
	$(MAKE) -C $(CONNECTOR) clean
	$(MAKE) -C $(CLIENT) clean
	$(MAKE) -C $(SERVER) clean


prepare:
	@if [ -d ".git/hooks" ]; then \
		ln -sf ../../.hooks/pre-commit .git/hooks; \
		ln -sf ../../.hooks/post-commit .git/hooks; \
		ln -sf ../../.hooks/pre-push .git/hooks; \
	fi
