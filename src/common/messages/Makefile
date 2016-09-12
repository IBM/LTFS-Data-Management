RELPATH = ../../..

TARGET_FILES :=
SOURCE_FILES := msgcompiler.cc
CLEANUP_FILES := msgcompiler msgdefs.h

msgcompiler: msgcompiler.cc
	$(CXX) -o $@ $^

msgdefs.h: messages.cfg msgcompiler
	./msgcompiler messages.cfg msgdefs.h

build: msgdefs.h

include $(RELPATH)/common.mk

include $(RELPATH)/targets.mk
