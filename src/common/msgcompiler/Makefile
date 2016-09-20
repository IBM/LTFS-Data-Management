RELPATH = ../../..

MSGDIR := ../messages

LIB_SRC_FILES :=
CLEANUP_FILES := msgcompiler msgdefs.h
BINARY := msgcompiler
POSTTARGET := $(MSGDIR)/msgdefs.h

# to build the message compiler do not use any libs
ARCHIVES :=
LDFLAGS :=

include $(RELPATH)/definitions.mk

# for the message compiler do not use any library

$(MSGDIR)/msgdefs.h: $(RELPATH)/messages.cfg msgcompiler
	msgcompiler $(RELPATH)/messages.cfg $(MSGDIR)/msgdefs.h
