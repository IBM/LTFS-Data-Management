#include <unordered_map>

#include "Inventory.h"

using namespace ltfsadmin;

std::string Inventory::ToString()
{
	if (!doc_) {
		Prepare();
		if (object_type_.length()) {
			xmlNewTextChild(root_, NULL, BAD_CAST "object_type", BAD_CAST object_type_.c_str());
		}

		// Handle options here
		if (options_.size()) {
			for (std::unordered_map<std::string, std::string>::iterator it = options_.begin(); it != options_.end(); ++it) {
				xmlNode* opt_node = xmlNewTextChild(root_, NULL, BAD_CAST "option", BAD_CAST it->second.c_str());
				xmlNewNsProp(opt_node, NULL, BAD_CAST "name", BAD_CAST it->first.c_str());
			}
		}
	}

	std::string ret = LTFSRequestMessage::ToString();

	return ret;
}
