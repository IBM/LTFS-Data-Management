RELPATH = ../../..

LIB_SRC_FILES := ltfsdm.pb.cc LTFSDmComm.cc
CLEANUP_FILES := ltfsdm.pb.cc ltfsdm.pb.h
BINARY :=
POSTTARGET :=


include $(RELPATH)/definitions.mk

ltfsdm.pb.cc: ltfsdm.proto
	protoc --proto_path=$(CURDIR) --cpp_out=$(CURDIR) $(CURDIR)/ltfsdm.proto
