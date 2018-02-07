# Copyright 2017 IBM Corp. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#  https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

RELPATH = ../..

LDFLAGS += -lprotobuf -lconnector -latomic -lpthread -luuid -lblkid -lmount

ARC_SRC_FILES := LTFSDMCommand.cc
ARC_SRC_FILES += StartCommand.cc
ARC_SRC_FILES += StopCommand.cc
ARC_SRC_FILES += AddCommand.cc
ARC_SRC_FILES += MigrateCommand.cc
ARC_SRC_FILES += RecallCommand.cc
ARC_SRC_FILES += HelpCommand.cc
ARC_SRC_FILES += InfoRequestsCommand.cc
ARC_SRC_FILES += InfoJobsCommand.cc
ARC_SRC_FILES += InfoFilesCommand.cc
ARC_SRC_FILES += InfoFsCommand.cc
ARC_SRC_FILES += StatusCommand.cc
ARC_SRC_FILES += RetrieveCommand.cc
ARC_SRC_FILES += InfoDrivesCommand.cc
ARC_SRC_FILES += InfoTapesCommand.cc
ARC_SRC_FILES += PoolCreateCommand.cc
ARC_SRC_FILES += PoolDeleteCommand.cc
ARC_SRC_FILES += PoolAddCommand.cc
ARC_SRC_FILES += PoolRemoveCommand.cc
ARC_SRC_FILES += InfoPoolsCommand.cc
ARC_SRC_FILES += VersionCommand.cc
CLEANUP_FILES := ltfsdm
BINARY := ltfsdm
POSTTARGET :=

include $(RELPATH)/definitions.mk
