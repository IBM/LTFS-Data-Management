include $(BUILD_ROOT)/common.mk

TARGET := messages
ARCH_FILES := messages.cc
SOURCE_FILES := msgcompiler.cc msgdefs.h messages.cc msgtest.cc
FILES_CLEANUP := msgcompiler msgtest msgdefs.h

# $(info =============== $(firstword $(subst /, , $(patsubst $(BUILD_ROOT)/src/%,%, $(CURDIR)))) ===============)

msgdefs.h: msgcompiler
	./msgcompiler messages.cfg msgdefs.h

test: msgtest
	./msgtest

include $(BUILD_ROOT)/targets.mk
