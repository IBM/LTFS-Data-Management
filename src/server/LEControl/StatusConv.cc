#include <string>
#include <memory>
#include <unordered_map>
#include <list>
#include <condition_variable>
#include <thread>

#include "StatusConv.h"

std::unordered_map<std::string, int> StatusConv::cart_stat_ = {
	{ std::string("Unformatted"),        TAPE_STATUS_UNFORMATTED },
	{ std::string("Valid LTFS"),         TAPE_STATUS_VALID_LTFS },
	{ std::string("Invalid LTFS"),       TAPE_STATUS_INVALID_LTFS },
	{ std::string("Unknown"),            TAPE_STATUS_UNKNOWN },
	{ std::string("Unavailable"),        TAPE_STATUS_UNAVAILABLE },
	{ std::string("Warning"),            TAPE_STATUS_WARNING },
	{ std::string("Error"),              TAPE_STATUS_ERROR },
	{ std::string("Critical"),           TAPE_STATUS_CRITICAL },
	{ std::string("Inaccessible"),       TAPE_STATUS_INACCESIBLE },
	{ std::string("Cleaning"),           TAPE_STATUS_CLEANING },
	{ std::string("Write Protected"),    TAPE_STATUS_WRITE_PROTECTED },
	{ std::string("Duplicated"),         TAPE_STATUS_DUPLICATED },
	{ std::string("Not Supported"),      TAPE_STATUS_NON_SUPPORTED },
	{ std::string("In Progress"),        TAPE_STATUS_IN_PROGRESS },
	{ std::string("Disconnected"),       TAPE_STATUS_DISCONNECTED }};

std::unordered_map<std::string, int> StatusConv::drive_stat_ = {
	{ std::string("Available"),     DRIVE_LTFS_STATUS_AVAILABLE },
	{ std::string("Unavailable"),   DRIVE_LTFS_STATUS_UNAVAILABLE },
	{ std::string("Error"),         DRIVE_LTFS_STATUS_ERROR },
	{ std::string("Not Installed"), DRIVE_LTFS_STATUS_NOT_INSTALLED },
	{ std::string("Locked"),        DRIVE_LTFS_STATUS_LOCKED },
	{ std::string("Disconnected"),  DRIVE_LTFS_STATUS_DISCONNECTED }};

std::unordered_map<std::string, int> StatusConv::node_stat_ = {
	{ std::string("Available"),       NODE_STATUS_AVAILABLE },
	{ std::string("Out of sync"),     NODE_STATUS_OUT_OF_SYNC },
	{ std::string("License Expired"), NODE_STATUS_LICENSE_EXPIRED },
	{ std::string("Unknown"),         NODE_STATUS_UNKNOWN },
	{ std::string("Disconnected"),    NODE_STATUS_DISCONNECTED },
	{ std::string("Not Configured"),  NODE_STATUS_NOT_CONFIGURED }};

std::unordered_map<std::string, int> StatusConv::cart_location_ = {
	{ std::string("Medium transport element"), LOCATION_MEDIUM_TRANSPORT_ELEMENT },
	{ std::string("Medium storage element"),   LOCATION_MEDIUM_STORAGE_ELEMENT },
	{ std::string("Import/Export slot"),       LOCATION_IMPORT_EXPORT_SLOT },
	{ std::string("Data transfer element"),    LOCATION_DATA_TRANSFER_ELEMENT }};

int StatusConv::get_cart_value(std::string inp)
{
	int rc = -1;

	if (cart_stat_.count(inp))
		rc =  cart_stat_[inp];

	return rc;
}

int StatusConv::get_drive_value(std::string inp)
{
	int rc = -1;

	if (drive_stat_.count(inp))
		rc = drive_stat_[inp];

	return rc;
}

int StatusConv::get_node_value(std::string inp)
{
	int rc = -1;

	if (node_stat_.count(inp))
		rc = node_stat_[inp];

	return rc;
}

int StatusConv::get_cart_location(std::string inp)
{
	int rc = -1;

	if (cart_location_.count(inp))
		rc = cart_location_[inp];

	return rc;
}
