/*
**  %Z% %I% %W% %G% %U%
**
**  ZZ_Copyright_BEGIN
**
**
**  IBM Confidential
**
**  OCO Source Materials
**
**  IBM Linear Tape File System Enterprise Edition
**
**  (C) Copyright IBM Corp. 2015
**
**  The source code for this program is not published or other-
**  wise divested of its trade secrets, irrespective of what has
**  been deposited with the U.S. Copyright Office
**
**
**  ZZ_Copyright_END
*/

#include <boost/assign.hpp>

#include "StatusConv.h"

using namespace std;
using namespace boost;

unordered_map<string, int> StatusConv::cart_stat_ = boost::assign::map_list_of
	( string("Unformatted"),        TAPE_STATUS_UNFORMATTED)
	( string("Valid LTFS"),         TAPE_STATUS_VALID_LTFS)
	( string("Invalid LTFS"),       TAPE_STATUS_INVALID_LTFS)
	( string("Unknown"),            TAPE_STATUS_UNKNOWN)
	( string("Unavailable"),        TAPE_STATUS_UNAVAILABLE)
	( string("Warning"),            TAPE_STATUS_WARNING)
	( string("Error"),              TAPE_STATUS_ERROR)
	( string("Critical"),           TAPE_STATUS_CRITICAL)
	( string("Inaccessible"),       TAPE_STATUS_INACCESIBLE)
	( string("Cleaning"),           TAPE_STATUS_CLEANING)
	( string("Write Protected"),    TAPE_STATUS_WRITE_PROTECTED)
	( string("Duplicated"),         TAPE_STATUS_DUPLICATED)
	( string("Not Supported"),      TAPE_STATUS_NON_SUPPORTED)
	( string("In Progress"),        TAPE_STATUS_IN_PROGRESS)
	( string("Disconnected"),       TAPE_STATUS_DISCONNECTED)
	;

unordered_map<string, int> StatusConv::drive_stat_ = boost::assign::map_list_of
	( string("Available"),     DRIVE_LTFS_STATUS_AVAILABLE)
	( string("Unavailable"),   DRIVE_LTFS_STATUS_UNAVAILABLE)
	( string("Error"),         DRIVE_LTFS_STATUS_ERROR)
	( string("Not Installed"), DRIVE_LTFS_STATUS_NOT_INSTALLED)
	( string("Locked"),        DRIVE_LTFS_STATUS_LOCKED)
	( string("Disconnected"),  DRIVE_LTFS_STATUS_DISCONNECTED)
	;

unordered_map<string, int> StatusConv::node_stat_ = boost::assign::map_list_of
	( string("Available"),       NODE_STATUS_AVAILABLE)
	( string("Out of sync"),     NODE_STATUS_OUT_OF_SYNC)
	( string("License Expired"), NODE_STATUS_LICENSE_EXPIRED)
	( string("Unknown"),         NODE_STATUS_UNKNOWN)
	( string("Disconnected"),    NODE_STATUS_DISCONNECTED)
	( string("Not Configured"),  NODE_STATUS_NOT_CONFIGURED)
	;

unordered_map<string, int> StatusConv::cart_location_ = boost::assign::map_list_of
	( string("Medium transport element"), LOCATION_MEDIUM_TRANSPORT_ELEMENT)
	( string("Medium storage element"),   LOCATION_MEDIUM_STORAGE_ELEMENT)
	( string("Import/Export slot"),       LOCATION_IMPORT_EXPORT_SLOT)
	( string("Data transfer element"),    LOCATION_DATA_TRANSFER_ELEMENT)
;

int StatusConv::get_cart_value(string inp)
{
	int rc = -1;

	if (cart_stat_.count(inp))
		rc =  cart_stat_[inp];

	return rc;
}

int StatusConv::get_drive_value(string inp)
{
	int rc = -1;

	if (drive_stat_.count(inp))
		rc = drive_stat_[inp];

	return rc;
}

int StatusConv::get_node_value(string inp)
{
	int rc = -1;

	if (node_stat_.count(inp))
		rc = node_stat_[inp];

	return rc;
}

int StatusConv::get_cart_location(string inp)
{
	int rc = -1;

	if (cart_location_.count(inp))
		rc = cart_location_[inp];

	return rc;
}
