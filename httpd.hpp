#ifndef _HTTPD_HPP
#define _HTTPD_HPP

#include "lwip/err.h"

struct tcp_pcb;

struct HttpdCallbacks;

/**
 * A simple LWIP http daemon 
 */
class Httpd {
friend struct HttpdCallbacks;

public:
	err_t init();

private:
	err_t accept(struct tcp_pcb *pcb, err_t err);

private:
	struct tcp_pcb	*pcb;
};

#endif
