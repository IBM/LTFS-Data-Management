RELPATH = ../../..

LDFLAGS := -lprotobuf -lfuse -lpthread 
SHAREDLIB := lib$(notdir $(CURDIR))connector.so

SO_SRC_FILES := Connector.cc ltfsdmd.ofs.cc
ARC_SRC_FILES := Connector.cc ltfsdmd.ofs.cc
CLEANUP_FILES := $(SHAREDLIB) ltfsdmd.ofs
BINARY := $(SHAREDLIB) ltfsdmd.ofs
POSTTARGET :=

ARCHIVES := $(RELPATH)/lib/common.a $(RELPATH)/lib/connector.a

include $(RELPATH)/definitions.mk
