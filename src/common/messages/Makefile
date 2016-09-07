include $(BUILD_ROOT)/common.mk

TARGET := messages
SOURCE_FILES := msgcompiler.cc msgdefs.h messages.cc msgtest.cc
FILES_CLEANUP := msgcompiler msgtest msgdefs.h

msgdefs.h: msgcompiler
	./msgcompiler messages.cfg msgdefs.h

test: msgtest
	./msgtest

include $(BUILD_ROOT)/targets.mk
