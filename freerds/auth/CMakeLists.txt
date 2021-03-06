# FreeRDS: FreeRDP Remote Desktop Services (RDS)
#
# Copyright 2013-2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Authentication Library

set(MODULE_NAME "freerds-auth")
set(MODULE_PREFIX "FREERDS_AUTH")

set(${MODULE_PREFIX}_SRCS
	auth.c)

add_library(${MODULE_NAME} ${${MODULE_PREFIX}_SRCS})

list(APPEND ${MODULE_PREFIX}_LIBS winpr)

target_link_libraries(${MODULE_NAME} ${${MODULE_PREFIX}_LIBS})

install(TARGETS ${MODULE_NAME} DESTINATION ${CMAKE_INSTALL_LIBDIR})

# Authentication Modules

if(UNIX)
	set(PAM_FEATURE_TYPE "REQUIRED")
	set(PAM_FEATURE_PURPOSE "authentication")
	set(PAM_FEATURE_DESCRIPTION "user authentication")

	find_feature(PAM ${PAM_FEATURE_TYPE} ${PAM_FEATURE_PURPOSE} ${PAM_FEATURE_DESCRIPTION})

	if(PAM_FOUND)
		add_subdirectory(pam)
	endif()
endif()
