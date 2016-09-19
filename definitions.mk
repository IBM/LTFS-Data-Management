# common definitions

.PHONY: clean build

# to be used below
PROJECT := OpenLTFS

# for linking - no standard C required
CC = g++

CXX = g++

# use c++11 to build the code
# CXXFLAGS  := -std=c++11 -g2 -ggdb -Wall -Werror -Wno-format-security -D_GNU_SOURCE -I$(RELPATH)
CXXFLAGS  := -std=c++11 -g2 -ggdb -Wall -Werror -D_GNU_SOURCE -I$(RELPATH)

# for protocol buffers
LDFLAGS := -lprotobuf

# client, common, or server
TARGETCOMP := $(shell perl -e "print '$(CURDIR)' =~ /.*$(PROJECT)\/src\/([^\/]+)/")

# to not set ARCHIVES (e.g. link w/o) set 'ARCHIVES :=' before
ARCHIVES ?= $(RELPATH)/lib/common.a $(RELPATH)/lib/$(TARGETCOMP).a

# binary source files contain the main routine
# library source files will be added to the archives
SOURCE_FILES := $(LIB_SRC_FILES) $(BIN_SRC_FILES)

ifneq ($(strip $(SOURCE_FILES)),)
DEPDIR := .d
endif

ifneq ($(strip $(LIB_SRC_FILES)),)
TARGETLIB := $(RELPATH)/lib/$(TARGETCOMP).a
endif

BINDIR := $(RELPATH)/bin

ifneq ($(strip $(BINARY)),)
TARGETBIN := $(BINDIR)/$(BINARY)
endif

objfiles = $(patsubst %.cc,%.o, $(1))

# build rules
default: build

# auto dependencies
$(DEPDIR): $(SOURCE_FILES)
	@rm -fr $(DEPDIR) && mkdir $(DEPDIR)
	$(CXX) $(CXXFLAGS) -MM $^ > .d/deps.mk

# client.a, common.a, and server.a archive files
$(TARGETLIB): $(TARGETLIB)($(call objfiles, $(LIB_SRC_FILES)))

$(BINARY): $(ARCHIVES)

# copy binary to bin directory
$(TARGETBIN): $(BINARY)
	cp $^ $@

clean:
	rm -fr $(RELPATH)/lib/* *.o $(CLEANUP_FILES) $(BINARY) $(TARGETBIN) $(DEPDIR)

build: $(DEPDIR) $(call objfiles, $(SOURCE_FILES)) $(TARGETLIB) $(TARGETBIN) $(POSTTARGET)

-include .d/deps.mk
