/*
Copyright (C) 2023 DEV47APPS, github.com/dev47apps

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "plugin.h"

#if __APPLE__
#include <objc/objc.h>

void get_os_name_version(char *out, size_t out_size) {
    Class NSProcessInfo = objc_getClass("NSProcessInfo");
    typedef id (*func)(id, SEL);
    func msgSend = (func)objc_msgSend;

    id pi = msgSend((id)NSProcessInfo, sel_registerName("processInfo"));
    SEL UTF8StringSel = sel_registerName("UTF8String");

    id vs = msgSend(pi, sel_registerName("operatingSystemVersionString"));

    typedef const char *(*utf8_func)(id, SEL);
    utf8_func UTF8String = (utf8_func)objc_msgSend;
    const char *version = UTF8String(vs, UTF8StringSel);

    if (version) {
        int major = 0, minor = 0,
            count = sscanf(version, "Version %d.%d", &major, &minor);
        if (count == 2 && major > 0 && minor > 0) {
            snprintf(out, out_size, "osx%d.%d", major, minor);
        }
    }
}

#endif
