/*
   Copyright 2013-2014 EditShare, 2013-2015 Skytechnology sp. z o.o.

   This file is part of LizardFS.

   LizardFS is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, version 3.

   LizardFS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with LizardFS. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "common/platform.h"

#include <cassert>
#include <exception>
#include <string>

#include "protocol/MFSCommunication.h"
#include "common/mfserr.h"

class Exception : public std::exception {
public:
	Exception(const std::string& message) : message_(message), status_(LIZARDFS_ERROR_UNKNOWN) {
	}

	Exception(const std::string& message, uint8_t status) : message_(message), status_(status) {
		assert(status != LIZARDFS_STATUS_OK);
		message_ += " (" + std::string(mfsstrerr(status)) + ")";
	}

	~Exception() throw() {}

	const char* what() const throw() {
		return message_.c_str();
	}

	const std::string& message() const throw() {
		return message_;
	}

	uint8_t status() const throw() {
		return status_;
	}

private:
	std::string message_;
	uint8_t status_;
};

#define LIZARDFS_CREATE_EXCEPTION_CLASS(name, base) \
	class name : public base { \
	public: \
		name(const std::string& message) : base(message) {} \
		name(const std::string& message, uint8_t status) : base(message, status) {} \
		~name() throw() {} \
	}

#define LIZARDFS_CREATE_EXCEPTION_CLASS_MSG(name, base, message) \
	class name : public base { \
	public: \
		name() : base(std::string(message)) {} \
		name(uint8_t status) : base(std::string(message), status) {} \
		~name() throw() {} \
	}
