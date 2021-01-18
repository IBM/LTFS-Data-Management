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

# common definitions

.PHONY: clean buildsrc buildtgt build libdir link

# for linking - no standard C required
CC = g++

CXX = g++

# use c++11 to build the code
# CXXFLAGS  := -std=c++11 -g2 -ggdb -Wall -Werror -Wno-format-security -D_GNU_SOURCE -I$(RELPATH)
CXXFLAGS  := -std=c++11 -g2 -ggdb -fPIC -Wall -Werror -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE \
             -I$(RELPATH) -I/usr/include/libxml2 -I/opt/IBM/ltfs/include

BINDIR := $(RELPATH)/bin
LIBDIR := $(RELPATH)/lib

LDFLAGS += -L$(BINDIR) -L/opt/IBM/ltfs/lib64 -L/opt/ibm/ltfsle/lib64/

# client, common, or server
TARGETCOMP := $(shell perl -e "print '$(CURDIR)' =~ /.*$(subst /,\/,$(ROOTDIR))\/src\/([^\/]+)/")

# to not set ARCHIVES (e.g. link w/o) set 'ARCHIVES :=' before
ARCHIVES ?= $(RELPATH)/lib/$(TARGETCOMP).a $(RELPATH)/lib/communication.a $(RELPATH)/lib/common.a

# library source files will be added to the archives
SOURCE_FILES := $(ARC_SRC_FILES) $(SO_SRC_FILES)

DEPDIR := .d

ifneq ($(strip $(SOURCE_FILES)),)
DEPS := $(DEPDIR)/deps.mk
endif

ifneq ($(strip $(ARC_SRC_FILES)),)
TARGETLIB := $(LIBDIR)/$(TARGETCOMP).a
endif

TARGET := $(addprefix $(BINDIR)/, $(BINARY))

objfiles = $(patsubst %.cc,%.o, $(1))

# build rules
default: build
mklink:

ifeq ($(CONNECTOR_TYPE),$(notdir $(CURDIR)))
mklink:
	ln -sf $(SHAREDLIB) $(BINDIR)/libconnector.so
endif

$(SHAREDLIB): $(call objfiles, $(SO_SRC_FILES))
	$(CXX) -shared $(LDFLAGS) $(CXXFLAGS) $^ -o $@

# creating diretories if missing
$(DEPDIR) $(LIBDIR) $(BINDIR):
	mkdir $@

# auto dependencies
$(DEPS): $(SOURCE_FILES) | $(DEPDIR)
	$(CXX) $(CXXFLAGS) -MM $(SOURCE_FILES) > $@

libdir: | $(LIBDIR)

# client.a, common.a, and server.a archive files
$(TARGETLIB): libdir $(TARGETLIB)($(call objfiles, $(ARC_SRC_FILES)))

# link binaries
$(BINARY): $(ARCHIVES)

# copy binary to bin directory
$(TARGET): $(BINARY) | $(BINDIR)
	cp $(BINARY) $(BINDIR)

deps: $(DEPS)

clean:
	rm -fr $(RELPATH)/lib/* *.o $(CLEANUP_FILES) $(BINARY) $(BINDIR)/* $(DEPDIR)

buildsrc: deps $(call objfiles, $(SOURCE_FILES)) $(TARGETLIB) 

buildtgt: buildsrc $(TARGET) mklink $(POSTTARGET)

build: buildsrc buildtgt

ifeq ($(MAKECMDGOALS),buildsrc)
    -include .d/deps.mk
endif
