RELPATH = ../../..

TARGET_FILES := msgdefs.cc
SOURCE_FILES := msgcompiler.cc msgdefs.cc  msgtest.cc
CLEANUP_FILES := msgcompiler msgtest msgdefs.cc runtest

msgcompiler: msgcompiler.cc
	$(CXX) -o $@ $^

msgdefs.h msgdefs.cc: messages.cfg msgcompiler
	./msgcompiler messages.cfg msgdefs.h msgdefs.cc

runtest: msgdefs.h
	@touch runtest
	@./msgtest && echo "===== message test successful ====="

test: msgtest runtest

include $(RELPATH)/common.mk

include $(RELPATH)/targets.mk
