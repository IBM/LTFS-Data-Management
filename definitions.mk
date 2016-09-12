# common definitions

PROJECT := OpenLTFS

CC = g++
CXX = g++

CXXFLAGS  := -std=c++11 -g2 -ggdb -Wall -Werror -D_GNU_SOURCE -I$(RELPATH)

LDFLAGS := -lprotobuf

objfiles = $(patsubst %.cc,%.o, $(1))

COMMON_LIB = src/common/lib
CLIENT_LIB = src/client/lib
SERVER_LIB = src/server/lib

TARGETCOMP := $(shell perl -e "print '$(CURDIR)' =~ /.*$(PROJECT)\/src\/([^\/]+)/")

ifdef TARGET_FILES
TARGETLIB := $(RELPATH)/lib/$(TARGETCOMP).a
endif

BINDIR := $(RELPATH)/bin

LDLIBS = $(RELPATH)/lib/$(TARGETCOMP).a $(RELPATH)/lib/common.a

# targets

$(TARGETLIB): $(TARGETLIB)($(call objfiles, $(TARGET_FILES)))

$(EXECUTABLE): $(LDLIBS)

$(BINDIR)/$(EXECUTABLE): $(EXECUTABLE)
	cp $^ $@

clean:
	rm -fr $(RELPATH)/lib/* *.o $(CLEANUP_FILES) $(EXECUTABLE) $(BINDIR)/* .d

build: .d $(call objfiles, $(SOURCE_FILES)) $(TARGETLIB) $(BINDIR)/$(EXECUTABLE)

.d: $(SOURCE_FILES)
	@mkdir .d
	$(CXX) $(CXXFLAGS) -MM $(CFLAGS) $^ > .d/deps.mk

-include .d/deps.mk
