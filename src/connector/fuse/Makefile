RELPATH = ../../..

LDFLAGS := -lprotobuf -lfuse -lpthread 
SHAREDLIB := lib$(notdir $(CURDIR))connector.so

SO_SRC_FILES := Connector.cc FuseFS.cc
ARC_SRC_FILES := Connector.cc FuseFS.cc
CLEANUP_FILES := $(SHAREDLIB) FuseFS
BINARY := $(SHAREDLIB) FuseFS
POSTTARGET :=

ARCHIVES := $(RELPATH)/lib/common.a $(RELPATH)/lib/connector.a

include $(RELPATH)/definitions.mk
