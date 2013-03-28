#include "httpd.hpp"

#include "lwip/opt.h"
#include "lwip/debug.h"
#include "lwip/stats.h"
#include "lwip/tcp.h"

#include <cstddef>

#include <stdbool.h>
#include <inc/hw_types.h>
#include <utils/uartstdio.h>

#include "TCPConnection.hpp"


struct HttpdCallbacks {
public:
	static err_t tcp_accept(void *arg, struct tcp_pcb *pcb, err_t err) {
		Httpd *instance = reinterpret_cast<Httpd*>(arg);
		return instance->accept(pcb, err);
	}
};

err_t
Httpd::init() {
	pcb = tcp_new();

	pcb->callback_arg = this;

	if( pcb == NULL ) {
		return ERR_MEM;
	}

	err_t err;
	err = tcp_bind(pcb, IP_ADDR_ANY, 80);
	if( err != ERR_OK ) {
		return err;
	}

	pcb = tcp_listen(pcb);
	tcp_accept(pcb, HttpdCallbacks::tcp_accept);

	return ERR_OK;
}

err_t
Httpd::accept(struct tcp_pcb *pcb, err_t err) {
	UARTprintf("Got connection %p\r\n", pcb);
	HttpConnection *conn = createConnection(pcb);
	return ERR_OK;
}


/** HttpConnection **/
HttpConnection::HttpConnection(struct tcp_pcb *pcb)
	: TCPConnection(pcb),
	  state(Connected) 
{
}

int
HttpConnection::findSubStr(char *data, uint16_t len, const char target[], uint16_t target_len) {
	int match = 0;
	int i;
	for(i=0;i<len;i++,data++) {
		if( *data == target[match] ) {
			match++;
			if( match == target_len ) {
				return i-target_len+1;
			}
		} else {
			match = 0;
		}
	}
	return -1;
}

int
HttpConnection::findLineEnd(char *data, uint16_t len) {
	return findSubStr(data, len, "\r\n", 2);
}

char*
HttpConnection::parseRequest(char *data, uint16_t *len) {
	int lineEnd = findLineEnd(data, *len);
	if( lineEnd < 0 ) {
		// No line-ends found, better luck next time
		return data;
	}

	data[lineEnd] = '\0';

	int firstSep = findSubStr(data, lineEnd, " ", 1);
	int secondSep = findSubStr(data+firstSep+1, lineEnd-firstSep-1, " ", 1);

	if( firstSep > 0 && secondSep > 0 ) {
		data[firstSep] = '\0';
		data[secondSep+firstSep+1] = '\0';

		char *method = data;
		char *path = &data[firstSep+1];
		setRequest(method, path);
	}

	data = data + lineEnd + 2;
	*len = *len - lineEnd - 2;

	state = ParsingHeader;

	return data;
}

char*
HttpConnection::parseHeader(char *data, uint16_t *len) {
	// We assume that all header fields are within one chunk of data.
	// This makes life real easy :-)

	// Parse one line at a time, set next state when we reach an empty line
	while(true) {
		int lineEnd = findLineEnd(data, *len);
		if( lineEnd < 0 ) {
			UARTprintf("Negative line end\r\n");
			// No line-ends found, better luck next time
			return data;
		}

		data[lineEnd] = '\0';
		UARTprintf("'%s'\r\n", data);

		int sep = findSubStr(data, lineEnd, ": ", 2);
		data[sep] = '\0';

		char *key = data;
		char *val = &data[sep+2];

		data = data + lineEnd + 2;
		*len = *len - lineEnd - 2;

		if( lineEnd == 0 ) {
			onHeaderDone();
			state = Body;
			return data;
		}
		onHeader(key, val);
	}
}

void
HttpConnection::parseData(char *data, uint16_t len) {
	if( state == ParsingRequest ) {
		data = parseRequest(data, &len);
	}
	if( state == ParsingHeader ) {
		data = parseHeader(data, &len);
	}
	if( state == Body ) {
		if( len > 0 ) 
			onBody(data, len);
	}
}

err_t
HttpConnection::onReceive(struct pbuf *p, err_t err) {
	UARTprintf("onReceive %p\r\n", p);
	if( state == Connected ) {
		state = ParsingRequest;
	}

	struct pbuf *q;
	for(q = p; q != NULL; q = q->next) {
		parseData((char*)q->payload, q->len);
	}
	tcp_recved(getPCB(), p->tot_len);
	pbuf_free(p);
	return ERR_OK;
}
