#ifndef _HTTPD_HPP
#define _HTTPD_HPP

#include "lwip/err.h"

#include "TCPConnection.hpp"

struct tcp_pcb;

struct HttpdCallbacks;

class HttpConnection : public TCPConnection {
public:
	HttpConnection(struct tcp_pcb *pcb);

private:
	virtual void onHeader(char *key, char *val) {}
	virtual void setRequest(char *method, char *path) {}
	virtual void onBody(char *data, uint16_t len) {}
	virtual void onHeaderDone() { }

private:
	int findSubStr(char *data, uint16_t len, const char target[], uint16_t target_len);
	int findLineEnd(char *data, uint16_t len);
	char *parseRequest(char *data, uint16_t *len);
	char *parseHeader(char *data, uint16_t *len);
	void parseData(char *data, uint16_t len);

	err_t onReceive(struct pbuf *p, err_t err);

private:
	typedef enum {
		Connected,
		ParsingRequest,
		ParsingHeader,
		Body
	} State;

private:
	State	state;
};

/**
 * A simple LWIP http daemon 
 */
class Httpd {
friend struct HttpdCallbacks;

public:
	err_t init();

	typedef enum RequestMethod {
		GET,
		POST,
		DELETE,
		PUT
	};

private:
	err_t accept(struct tcp_pcb *pcb, err_t err);
	virtual HttpConnection *createConnection(struct tcp_pcb *pcb) = 0;

private:
	struct tcp_pcb	*pcb;
};


#endif
