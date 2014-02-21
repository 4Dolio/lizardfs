/*
   Copyright 2005-2010 Jakub Kruszona-Zawadzki, Gemius SA, 2013 Skytechnology sp. z o.o..

   This file was part of MooseFS and is part of LizardFS.

   LizardFS is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, version 3.

   LizardFS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with LizardFS  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <inttypes.h>

#define _CONFIG_MAKE_PROTOTYPE(fname,type) type cfg_get##fname(const char *name,const type def)

int cfg_load (const char *fname,int logundefined);
int cfg_reload (void);
void cfg_term (void);

int cfg_isdefined(const char *name);

_CONFIG_MAKE_PROTOTYPE(str,char*);
_CONFIG_MAKE_PROTOTYPE(num,int);
_CONFIG_MAKE_PROTOTYPE(uint8,uint8_t);
_CONFIG_MAKE_PROTOTYPE(int8,int8_t);
_CONFIG_MAKE_PROTOTYPE(uint16,uint16_t);
_CONFIG_MAKE_PROTOTYPE(int16,int16_t);
_CONFIG_MAKE_PROTOTYPE(uint32,uint32_t);
_CONFIG_MAKE_PROTOTYPE(int32,int32_t);
_CONFIG_MAKE_PROTOTYPE(uint64,uint64_t);
_CONFIG_MAKE_PROTOTYPE(int64,int64_t);
_CONFIG_MAKE_PROTOTYPE(double,double);

template <class T>
T cfg_get(const char* name, T defaultValue);

template <class T>
T cfg_get_minvalue(const char* name, T defaultValue, T minValue) {
	T configValue = cfg_get(name, defaultValue);
	if (configValue < minValue) {
		syslog(LOG_WARNING,
				"config value %s was set to %" PRId64
				", but minimal value is %" PRId64 " - increasing",
				name, int64_t(configValue), int64_t(minValue));
		configValue = minValue;
	}
	return configValue;
}

template <class T>
T cfg_get_maxvalue(const char* name, T defaultValue, T maxValue) {
	T configValue = cfg_get(name, defaultValue);
	if (configValue > maxValue) {
		syslog(LOG_WARNING,
				"config value %s was set to %" PRId64
				", but maximal value is %" PRId64 " - decreasing",
				name, int64_t(configValue), int64_t(maxValue));
		configValue = maxValue;
	}
	return configValue;
}

template <class T>
void cfg_warning_on_value_change(const char* name, T expectedValue) {
	T newValue = cfg_get(name, expectedValue);
	if (expectedValue != newValue) {
		syslog(LOG_WARNING,
				"config value %s has changed, but changing it requires restart", name);
	}
}
