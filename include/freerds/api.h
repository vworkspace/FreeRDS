/**
 * FreeRDS: FreeRDP Remote Desktop Services (RDS)
 *
 * Copyright 2013-2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FREERDS_API_H
#define FREERDS_API_H

#include <winpr/spec.h>

#ifdef FREERDP_EXPORTS
#define FREERDS_EXPORTS
#endif

#if defined _WIN32 || defined __CYGWIN__
	#ifdef FREERDS_EXPORTS
		#ifdef __GNUC__
			#define FREERDS_EXPORT __attribute__((dllexport))
		#else
			#define FREERDS_EXPORT __declspec(dllexport)
		#endif
	#else
		#ifdef __GNUC__
			#define FREERDS_EXPORT __attribute__((dllimport))
		#else
			#define FREERDS_EXPORT __declspec(dllimport)
		#endif
	#endif
#else
	#if __GNUC__ >= 4
		#define FREERDS_EXPORT   __attribute__ ((visibility("default")))
	#else
		#define FREERDS_EXPORT
	#endif
#endif

#endif /* FREERDS_API_H */
