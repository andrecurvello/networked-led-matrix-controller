#include "jenkins-api-client.h"
#include "vartext.h"

static TEXT_TEMPLATE(request_header, ARG(host), 
	CONST(1, "GET /jenkins-trunk/api/json?pretty=true&tree=jobs[name,color] HTTP/1.0\r\n")
	CONST(2, "HOST: ")
	VAR(host)
	CONST(3, "\r\n\r\n")
	);

enum jac_states
{
	JAC_NONE = 0,
	JAC_CONNECTING,
	JAC_CONNECTED,
};

enum jac_parse_states
{
	JACP_HEADER_RESULT = 0,
	JACP_HEADER_DATA,
	JACP_DATA,
	JACP_INVALID
};

struct jac_state
{
	enum jac_states state;
	enum jac_parse_states parse_state;
	struct tcp_pcb *pcb;
	uint8_t jacp_remain[200];
};

static void jac_error(void *arg, err_t err);
static err_t jac_poll(void *arg, struct tcp_pcb *tpcb);
static err_t jac_connected(void *arg, struct tcp_pcb *tpcb, err_t err);
static err_t jac_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);
static void jac_parse_response(struct jac_state *state, uint8_t *buf, uint16_t len);

void
jenkins_get_status(ip_addr_t addr) {
	struct tcp_pcb *pcb;
	
	pcb = tcp_new();
	
	UARTprintf("Getting status\n");

	if( pcb == NULL ) {
		UARTprintf("Could not allocate TCP PCB\n");
		return;
	}

	struct jac_state *state;

	state = (struct jac_state*)mem_malloc(sizeof(struct jac_state));

	if( state == NULL ) {
		UARTprintf("Could not allocate memory for state\n");
		tcp_close(pcb);
		return;
	}

	state->state = JAC_CONNECTING;
	state->pcb = pcb;
	state->parse_state = JACP_HEADER_RESULT;

	tcp_arg(pcb, state);
	tcp_err(pcb, jac_error);
	tcp_poll(pcb, jac_poll, 10);
	tcp_recv(pcb, jac_recv);

	if( tcp_connect(pcb, &addr, 8080, jac_connected) != ERR_OK ) {
		UARTprintf("Could not connect\n");
		tcp_close(pcb);
		mem_free(state);
	}
}

static void
jac_error(void *arg, err_t err) 
{
	UARTprintf("Error: %d\n", err);
	if( arg != NULL ) {
		mem_free(arg);
	}
}

static err_t
jac_poll(void *arg, struct tcp_pcb *tpcb)
{
	struct jac_state *state = (struct jac_state*)arg;

	UARTprintf("poll\n");

	if( state == NULL ) {
		tcp_abort(tpcb);
		return ERR_ABRT;
	}

	if( state->state == JAC_CONNECTING ) {
		tcp_abort(tpcb);
		return ERR_ABRT;
	}

	return ERR_OK;
}

static err_t
jac_connected(void *arg, struct tcp_pcb *tpcb, err_t err)
{
	char buf[200];
	UARTprintf("Connected\n");

	char *buf_end = request_header_expand(buf, "10.0.0.239");
	buf_end[1] = '\0';
	UARTprintf("%s\n", buf);

	if( tcp_write(tpcb, buf, buf_end - buf, TCP_WRITE_FLAG_COPY) != ERR_OK ) {
		UARTprintf("Error writing\n");
		tcp_abort(tpcb);
		return ERR_ABRT;
	}

	return ERR_OK;
}

static err_t
jac_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
	UARTprintf("Got data\n");

	if( p == NULL) {
		UARTprintf("NULL data, remote closed\n");
		tcp_close(tpcb);
		mem_free(arg);
		return ERR_OK;
	}

	struct pbuf *q;
	for(q = p; q != NULL; q = q->next) {
		jac_parse_response((struct jac_state*)arg, (uint8_t*)q->payload, q->len);
	}

	UARTprintf("\n");
	tcp_recved(tpcb, p->tot_len);
	pbuf_free(p);

	return ERR_OK;
}

static char*
jac_find_eol(uint8_t *buf, uint16_t len) {
	for(int i=0; i<len; i++) {
		if( buf[i] == '\r' && buf[i+1] == '\n' ) 
			return buf+i;
	}
	return NULL;
}

static char*
jac_index(uint8_t ch, uint8_t *buf, uint16_t len) {
	for(int i=0; i<len; i++) {
		if( buf[i] == ch )
			return buf+i;
	}
	return NULL;
}

static void
jac_print(uint8_t *buf, uint16_t len) 
{
	for(int i=0; i<len; i++) {
		UARTprintf("%c", buf[i]);
	}
}

static void
jac_parse_response(struct jac_state *state, uint8_t *buf, uint16_t len) 
{
	uint8_t *ptr = buf;
	uint8_t *next;

	if( state->parse_state == JACP_HEADER_RESULT ) {
		next = jac_index(' ', ptr, len);

		if( strncmp(ptr, "HTTP/1", 6) != 0 ) {
			UARTprintf("Invalid response\n");
			state->parse_state = JACP_INVALID;
			return;
		}

		ptr = next + 1;
		len -= next-ptr;

		next = jac_find_eol(ptr, len);

		if( strncmp(ptr, "200 OK", 6) != 0 ) {
			UARTprintf("Invalid response");
			state->parse_state = JACP_INVALID;
			return;
		}

		ptr = next + 1;
		len -= next-ptr;

		UARTprintf("Response ok\n");

		state->parse_state = JACP_HEADER_DATA;
	}
}
