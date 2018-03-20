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

MESSAGES := src/messages
COMMUNICATION := src/communication
COMMON := src/common
CLIENT := src/client
SERVER := src/server

CONNECTOR := src/connector/fuse
ifneq ($(wildcard /usr/include/xfs/dmapi.h),)
    CONNECTOR += src/connector/dmapi
endif
