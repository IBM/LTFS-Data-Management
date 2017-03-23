COMMONDIRS := src/common/msgcompiler
COMMONDIRS += src/common/util
COMMONDIRS += src/common/messages
COMMONDIRS += src/common/tracing
COMMONDIRS += src/common/comm

CLIENTDIRS := src/client

SERVERDIRS := src/server

CONNECDIRS := src/connector/fuse
ifneq ($(wildcard /usr/include/xfs/dmapi.h),)
    CONNECDIRS += src/connector/dmapi
endif
