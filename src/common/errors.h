/*******************************************************************************
 * Copyright 2018 IBM Corp. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *  https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *******************************************************************************/
#pragma once

enum class Error
{
    GENERAL_ERROR = -1,
    OK = 0,
    COMM_ERROR = 1001,
    ATTR_FORMAT = 1002,
    FS_CHECK_ERROR = 1003,
    FS_ADD_ERROR = 1004,
    TAPE_EXISTS_IN_POOL = 1005,
    TAPE_NOT_EXISTS_IN_POOL = 1006,
    POOL_EXISTS = 1007,
    POOL_NOT_EXISTS = 1008,
    TAPE_NOT_EXISTS = 1009,
    POOL_NOT_EMPTY = 1010,
    WRONG_POOLNUM = 1011,
    NOT_ALL_POOLS_EXIST = 1012,
    DRIVE_BUSY = 1013,
    TERMINATING = 1014,
    FS_BUSY = 1015,
    CONFIG_POOL_EXISTS = 1016,
    CONFIG_POOL_NOT_EXISTS = 1017,
    CONFIG_TAPE_EXISTS = 1018,
    CONFIG_TAPE_NOT_EXISTS = 1019,
    CONFIG_TARGET_EXISTS = 1020,
    CONFIG_SOURCE_EXISTS = 1021,
    CONFIG_UUID_EXISTS = 1022,
    CONFIG_POOL_NOT_EMPTY = 1023,
    CONFIG_TARGET_NOT_EXISTS = 1024,
    CONFIG_FORMAT_ERROR = 1025,
    FS_IN_FSTAB = 1026,
    FS_UNMOUNT = 1027,
    POOL_TOO_SMALL = 1028,

    ALREADY_FORMATTED = 1050,
    WRITE_PROTECTED = 1051,
    TAPE_STATE_ERR = 1052,
    NOT_FORMATTED = 1053,
    INACCESSIBLE = 1054,
    UNKNOWN_FORMAT_STATUS = 1055,
    TAPE_NOT_WRITABLE = 1056,

    COMMAND_PARTIALLY_FAILED = 2001,
    COMMAND_FAILED = 2002,

};

