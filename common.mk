PROJECT := OpenLTFS

CC = g++
CXX = g++

CXXFLAGS  = -std=c++11 -g2 -ggdb -Wall -Werror $(CLARGEFILES) -D_GNU_SOURCE -fPIC -lprotobuf

objfiles = $(patsubst %.cc,%.o, $(1))

COMMON_LIB = src/common/lib
CLIENT_LIB = src/client/lib
SERVER_LIB = src/server/lib

# TARGETCOMP := $(shell echo $(CURDIR) |sed -e s/.*$(PROJECT)\\/src\\///g -e s/\\/.*//g )
TARGETCOMP := $(lastword $(subst /, , $(realpath $(patsubst ../../%,%, $(RELPATH)))))
TARGETLIB := $(RELPATH)/lib/$(TARGETCOMP).a
