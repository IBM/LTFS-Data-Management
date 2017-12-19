RELPATH = ../..

LDFLAGS := -lprotobuf -lpthread -lsqlite3 -lconnector -latomic -lxml2

ARC_SRC_FILES := SQLStatements.cc
ARC_SRC_FILES += Server.cc
ARC_SRC_FILES += DataBase.cc
ARC_SRC_FILES += SubServer.cc
ARC_SRC_FILES += Receiver.cc
ARC_SRC_FILES += MessageParser.cc
ARC_SRC_FILES += FileOperation.cc
ARC_SRC_FILES += Migration.cc
ARC_SRC_FILES += SelRecall.cc
ARC_SRC_FILES += TransRecall.cc
ARC_SRC_FILES += Scheduler.cc
ARC_SRC_FILES += Status.cc
ARC_SRC_FILES += LTFSDMDrive.cc
ARC_SRC_FILES += LTFSDMCartridge.cc
ARC_SRC_FILES += LTFSDMInventory.cc

CLEANUP_FILES := ltfsdmd
BINARY := ltfsdmd
POSTTARGET :=

include $(RELPATH)/definitions.mk
