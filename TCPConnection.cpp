#include "TCPConnection.hpp"

TCPConnection::TCPConnection(struct tcp_pcb *pcb) 
	: pcb(pcb) {
	tcp_arg(pcb, this);
	tcp_recv(pcb, TCPConnection::StaticReceive);
	tcp_err(pcb, TCPConnection::StaticError);
	tcp_poll(pcb, TCPConnection::StaticPoll, 0);
	tcp_sent(pcb, TCPConnection::StaticSent);

	UARTprintf("Created Connection Handler %p\r\n", pcb);
}

TCPConnection::~TCPConnection() {
	UARTprintf("Destroyed connection handler %p\r\n", pcb);
	tcp_recv(pcb, NULL);
	tcp_err(pcb, NULL);
	tcp_poll(pcb, NULL, 0);

	tcp_close(pcb);
}

err_t
TCPConnection::onReceive(struct pbuf *p, err_t err) {
	return ERR_OK;
}

err_t
TCPConnection::onPoll() {
	UARTprintf("Poll\r\n");
	delete this;
	return ERR_OK;
}

err_t
TCPConnection::onSent(uint16_t len) {
	UARTprintf("Sent %d bytes\r\n", len);
	return ERR_OK;
}
