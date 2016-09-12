RELPATH = ../../..

TARGET_FILES := ltfsdm.pb.cc LTFSDmC.cc
SOURCE_FILES := ltfsdm.pb.cc LTFSDmC.cc
CLEANUP_FILES := ltfsdm.pb.cc ltfsdm.pb.h

ltfsdm.pb.cc: ltfsdm.proto
	protoc --proto_path=$(CURDIR) --cpp_out=$(CURDIR) $(CURDIR)/ltfsdm.proto

include $(RELPATH)/definitions.mk
