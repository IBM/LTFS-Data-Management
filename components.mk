CLIENTDIRS := $(dir $(shell find src/client -name Makefile))
SERVERDIRS := $(dir $(shell find src/server -name Makefile))
COMMONDIRS := $(dir $(shell find src/common -name Makefile))
