#ifndef CGIPINS_H
#define CGIPINS_H

#include "httpd.h"

int cgiPins(HttpdConnData *connData);
int cgiRelay(HttpdConnData *connData);
int cgiDefRelay(HttpdConnData *connData);

#endif
