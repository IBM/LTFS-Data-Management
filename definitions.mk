# common definitions

.PHONY: clean build libdir

# to be used below
PROJECT := OpenLTFS

# for linking - no standard C required
CC = g++

CXX = g++

# use c++11 to build the code
# CXXFLAGS  := -std=c++11 -g2 -ggdb -Wall -Werror -Wno-format-security -D_GNU_SOURCE -I$(RELPATH)
CXXFLAGS  := -std=c++11 -g2 -ggdb -Wall -Werror -D_GNU_SOURCE -I$(RELPATH) -I/opt/local/include -I/usr/local/include

# for protocol buffers
LDFLAGS := -lprotobuf -lpthread -L/usr/local/lib

BINDIR := $(RELPATH)/bin
LIBDIR := $(RELPATH)/lib

# client, common, or server
TARGETCOMP := $(shell perl -e "print '$(CURDIR)' =~ /.*$(PROJECT)\/src\/([^\/]+)/")

# to not set ARCHIVES (e.g. link w/o) set 'ARCHIVES :=' before
ARCHIVES ?= $(RELPATH)/lib/common.a $(RELPATH)/lib/$(TARGETCOMP).a

# library source files will be added to the archives
SOURCE_FILES := $(LIB_SRC_FILES)

ifneq ($(strip $(SOURCE_FILES)),)
DEPDIR := .d
endif

ifneq ($(strip $(LIB_SRC_FILES)),)
TARGETLIB := $(LIBDIR)/$(TARGETCOMP).a
endif

TARGET := $(addprefix $(BINDIR)/, $(BINARY))

objfiles = $(patsubst %.cc,%.o, $(1))

# build rules
default: build

# auto dependencies
$(DEPDIR): $(SOURCE_FILES)
	@rm -fr $(DEPDIR) && mkdir $(DEPDIR)
	$(CXX) $(CXXFLAGS) -MM $^ > .d/deps.mk

# create lib directory if not exists
$(LIBDIR):
	mkdir $@

# client.a, common.a, and server.a archive files
$(TARGETLIB): | $(LIBDIR) $(TARGETLIB)($(call objfiles, $(LIB_SRC_FILES)))

# link binaries
$(BINARY): $(ARCHIVES)

# create bin directory if not exists
$(BINDIR):
	mkdir $@

# copy binary to bin directory
$(TARGET): | $(BINDIR) $(BINARY)
	cp $(BINARY) $(BINDIR)

clean:
	rm -fr $(RELPATH)/lib/* *.o $(CLEANUP_FILES) $(BINARY) $(BINDIR)/* $(DEPDIR)

build: $(DEPDIR) $(call objfiles, $(SOURCE_FILES)) $(TARGETLIB) $(TARGET) $(POSTTARGET)

-include .d/deps.mk
