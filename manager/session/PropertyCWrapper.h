/**
 * Property C Wrapper is a way so the C Modules can
 * get access to the property manager
 *
 * there will be 3 kinds of propertys
 * PropertyBool
 * PropertyString
 * PropertyNumber
 *
 * Copyright 2013 Thincast Technologies GmbH
 * Copyright 2013 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
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

#ifndef PROPERTYCWRAPPER_H_
#define PROPERTYCWRAPPER_H_

#include <freerds/properties.h>

#ifdef __cplusplus
extern "C" {
#endif

BOOL getPropertyBool(char* path, BOOL* value);
BOOL getPropertyNumber(char* path, long* value);
BOOL getPropertyString(char* path, char* value, unsigned int valueLength);

#ifdef __cplusplus
}
#endif

#endif /* PROPERTYCWRAPPER_H_ */
