RELPATH = ../../..

LDFLAGS := -lprotobuf -lfuse -lpthread -luuid -lblkid
SHAREDLIB := lib$(notdir $(CURDIR))connector.so

SO_SRC_FILES := Connector.cc FuseLock.cc FuseFS.cc
ARC_SRC_FILES := Connector.cc FuseLock.cc FuseFS.cc
CLEANUP_FILES := $(SHAREDLIB) ltfsdmd.ofs
BINARY := $(SHAREDLIB) ltfsdmd.ofs
POSTTARGET :=

ARCHIVES := $(RELPATH)/lib/connector.a $(RELPATH)/lib/common.a

include $(RELPATH)/definitions.mk
