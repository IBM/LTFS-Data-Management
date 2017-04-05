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
**  (C) Copyright IBM Corp. 2012, 2015
**
**  The source code for this program is not published or other-
**  wise divested of its trade secrets, irrespective of what has
**  been deposited with the U.S. Copyright Office
**
**
**  ZZ_Copyright_END
*/
#pragma once

#include "LEControl.h"

class StatusConv {
	private:
		static boost::unordered_map<string, int> cart_stat_;
		static boost::unordered_map<string, int> drive_stat_;
		static boost::unordered_map<string, int> node_stat_;
		static boost::unordered_map<string, int> cart_location_;

	public:
		static int get_cart_value(string in);
		static int get_drive_value(string in);
		static int get_node_value(string in);
		static int get_cart_location(string in);
};
