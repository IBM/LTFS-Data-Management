RELPATH = ../../..
include $(RELPATH)/common.mk

TARGET_FILES := messages.cc
SOURCE_FILES := msgcompiler.cc msgdefs.h messages.cc msgtest.cc
CLEANUP_FILES := msgcompiler msgtest msgdefs.h

msgdefs.h: messages.cfg msgcompiler
	./msgcompiler messages.cfg msgdefs.h

test: msgdefs.h msgtest
	./msgtest

include $(RELPATH)/targets.mk
