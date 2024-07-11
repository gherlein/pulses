#ifndef _LWIPOPTS_H
#define _LWIPOPTS_H

#include "lwipopts_common.h"

#define LWIP_HTTPD 1
// #define LWIP_HTTPD_SSI 1
// #define LWIP_HTTPD_CGI 1
//  don't include the tag comment - less work for the CPU, but may be harder to
//  debug
// #define LWIP_HTTPD_SSI_INCLUDE_TAG 0
//  use generated fsdata
#define HTTPD_FSDATA_FILE "fsdata.c"
#endif
