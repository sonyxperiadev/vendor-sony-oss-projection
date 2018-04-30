/* Micro Communicator for Projection uC
* a High-Speed Serial communications server
*
* Copyright (C) 2018 AngeloGioacchino Del Regno <kholk11@gmail.com>
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
        *
        *      http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#ifndef PROJECTORSETTINGS_UCOMM_EXT_H
#define PROJECTORSETTINGS_UCOMM_EXT_H

#define UCOMM_FOCUS_TEST_FAR		0xF0CA1
#define UCOMM_FOCUS_TEST_NEAR		0xF0CA0

#define ERR_UCOMM_FOCUS_UNDERFLOW	-6
#define ERR_UCOMM_FOCUS_OVERFLOW	-7
#define ERR_UCOMM_FOCUS_GENERAL		-8

int ucommsvr_set_backlight(int brightness);
int ucommsvr_set_keystone(int ksval);
int ucommsvr_set_focus(int focus);

#endif //PROJECTORSETTINGS_UCOMM_EXT_H
