RELPATH = ../../..

LIB_SRC_FILES :=
BIN_SRC_FILES := msgcompiler.cc
CLEANUP_FILES := msgcompiler msgdefs.h
BINARY := msgcompiler
POSTTARGET := msgdefs.h

# to build the message compiler do not use any libs
ARCHIVES :=
LDFLAGS :=

include $(RELPATH)/definitions.mk

# for the message compiler do not use any library

msgdefs.h: messages.cfg msgcompiler
	msgcompiler messages.cfg msgdefs.h
