RELPATH = ../../..

LDFLAGS += -lprotobuf

ARC_SRC_FILES := ltfsdm.pb.cc LTFSDmComm.cc
CLEANUP_FILES := ltfsdm.pb.cc ltfsdm.pb.h
BINARY := client server
POSTTARGET :=


include $(RELPATH)/definitions.mk

ltfsdm.pb.cc: $(RELPATH)/ltfsdm.proto
	protoc --proto_path=$(RELPATH) --cpp_out=$(CURDIR) $(RELPATH)/ltfsdm.proto
