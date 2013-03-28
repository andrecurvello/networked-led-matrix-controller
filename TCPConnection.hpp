#ifndef _TCPCONNECTION_HPP
#define _TCPCONNECTION_HPP

#include "lwip/opt.h"
#include "lwip/debug.h"
#include "lwip/stats.h"
#include "lwip/tcp.h"

#include <cstddef>

#include <stdbool.h>
#include <inc/hw_types.h>
#include <utils/uartstdio.h>

class TCPConnection {
public:
	TCPConnection(struct tcp_pcb *pcb);
	~TCPConnection();

private:
	virtual err_t onReceive(struct pbuf *p, err_t err);
	virtual err_t onPoll() {return ERR_OK;}
	virtual err_t onSent(uint16_t len);
	virtual err_t onRemoteClose(err_t err) {delete this; return ERR_OK;}

public:
	static err_t StaticReceive(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
		if( arg != NULL ) {
			TCPConnection *con = reinterpret_cast<TCPConnection*>(arg);
			if( p == NULL ) {
				return con->onRemoteClose(err);
			}
			return con->onReceive(p, err);
		}
		return ERR_OK;
	}

	static void StaticError(void *arg, err_t err) {
		if( arg != NULL ) {
			TCPConnection *con = reinterpret_cast<TCPConnection*>(arg);
			delete con;
		}
	}

	static err_t StaticPoll(void *arg, struct tcp_pcb *tpcb) {
		if( arg != NULL ) {
			TCPConnection *con = reinterpret_cast<TCPConnection*>(arg);
			return con->onPoll();
		}
		return ERR_MEM;
	}

	static err_t StaticSent(void *arg, struct tcp_pcb *tpcb, u16_t len) {
		if( arg != NULL ) {
			TCPConnection *con = reinterpret_cast<TCPConnection*>(arg);
			return con->onSent(len);
		}
		return ERR_MEM;
	}

public:
	void* operator new(std::size_t size) {
		return mem_malloc(size);
	}

	void operator delete(void *m) {
		mem_free(m);
	}

	inline struct tcp_pcb *getPCB() {
		return pcb;
	}

private:
	struct tcp_pcb 	*pcb;
};
#endif
