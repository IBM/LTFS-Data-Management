#pragma once

#include "LEControl.h"

class StatusConv {
	private:
		static std::unordered_map<std::string, int> cart_stat_;
		static std::unordered_map<std::string, int> drive_stat_;
		static std::unordered_map<std::string, int> node_stat_;
		static std::unordered_map<std::string, int> cart_location_;

	public:
		static int get_cart_value(std::string in);
		static int get_drive_value(std::string in);
		static int get_node_value(std::string in);
		static int get_cart_location(std::string in);
};
