#ifndef _JENKINS_API_CLIENT_H
#define _JENKINS_API_CLIENT_H

#include "lwip/tcp.h"

void jenkins_get_status(ip_addr_t addr);

#endif
