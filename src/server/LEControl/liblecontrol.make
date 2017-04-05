#
#   ZZ_Copyright_BEGIN
#
#
#   Licensed Materials - Property of IBM
#
#   IBM Linear Tape File System Enterprise Edition
#
#   (C) Copyright IBM Corp. 2012 2015 All Rights Reserved
#
#   US Government Users Restricted Rights - Use, duplication or
#   disclosure restricted by GSA ADP Schedule Contract with
#   IBM Corp.
#
#
#   ZZ_Copyright_END
#

TARGET_FINAL := liblecontrol.a

SOURCE_CXX_FILES := $(filter-out ut.cc, $(wildcard *.cc))

CXXFLAGS_ADD += -O0 -DDEBUG -I. -I.. -I./ltfsadminlib -I/usr/include/libxml2
LFLAGS_ADD +=

include ../ltfs-mig-defs.make

#ut: ut.o libltfsadmin.a
#	g++ $(CXXFLAGS) $(LFLAGS_ADD) -o $@ ut.o libltfsadmin.a
