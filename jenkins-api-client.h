#ifndef _JENKINS_API_CLIENT_H
#define _JENKINS_API_CLIENT_H

#include "lwip/tcp.h"

typedef void(*jac_status_callback_t)(const char *name, const char *color);

void jenkins_get_status(ip_addr_t addr, const char *hostname, jac_status_callback_t cb);

#endif
