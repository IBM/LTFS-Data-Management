include $(BUILD_ROOT)/common.mk

TARGET := comm
ARCH_FILES := ltfsdm.pb.cc LTFSDmC.cc
SOURCE_FILES := ltfsdm.pb.cc LTFSDmC.cc
FILES_CLEANUP := ltfsdm.pb.cc ltfsdm.pb.h

ltfsdm.pb.cc:
	protoc --proto_path=$(CURDIR) --cpp_out=$(CURDIR) $(CURDIR)/ltfsdm.proto

include $(BUILD_ROOT)/targets.mk
