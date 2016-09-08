PROJECT := OpenLTFS

CC = g++
CXX = g++

CXXFLAGS  := -std=c++11 -g2 -ggdb -Wall -Werror -D_GNU_SOURCE -lprotobuf
CXXFLAGS  += -I$(RELPATH)

objfiles = $(patsubst %.cc,%.o, $(1))

COMMON_LIB = src/common/lib
CLIENT_LIB = src/client/lib
SERVER_LIB = src/server/lib

TARGETCOMP := $(shell echo $(CURDIR) |sed s/.*OpenLTFS.//g |awk -F / '{print $$2}')

ifdef TARGET_FILES
TARGETLIB := $(RELPATH)/lib/$(TARGETCOMP).a
endif

BINDIR := $(RELPATH)/bin

LDLIBS = $(RELPATH)/lib/$(TARGETCOMP).a $(RELPATH)/lib/common.a
