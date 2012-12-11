#include "jenkins-api-client.h"
#include "vartext.h"
#include <stdbool.h>

#include "json.h"

#define RED_SHIFT               0
#define GREEN_SHIFT     	4
#define COLOR(r,g,b) 		((r & 0xFF)+((g & 0xFF)<<GREEN_SHIFT))
extern void set_char(char c, uint16_t color);

#define delayMs(ms) (SysCtlDelay(((SysCtlClockGet() / 3) / 1000)*ms))

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
	JACP_HEADER_DATA,			// 1
	JACP_DATA_START,			// 2
	JACP_DATA_JOBS_START,			// 3
	JACP_DATA_JOBS_KEY_STRING,		// 4
	JACP_DATA_JOBS_COLON,			// 5
	JACP_DATA_JOBS_START_BRACKET,		// 6
	JACP_DATA_JOBS_ARRAY,			// 7
	JACP_DATA_JOBS_ENTRY_START,		// 8
	JACP_DATA_JOBS_ENTRY_KEY_STRING, 	// 9
	JACP_DATA_JOBS_ENTRY_COLON,		// 10
	JACP_DATA_JOBS_ENTRY_VALUE,		// 11
	JACP_DATA_JOBS_ENTRY_VALUE_STRING,	// 12
	JACP_DATA_JOBS_ENTRY_VALUE_DONE,	// 13
	JACP_DATA_JOBS_ENTRY_END,		// 14
	JACP_INVALID 				// 15
};

struct jac_state
{
	enum jac_states state;
	enum jac_parse_states parse_state;
	struct tcp_pcb *pcb;
	uint8_t jacp_data[200];
	uint8_t jacp_data_used;
	bool got_match;
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
	state->jacp_data_used = 0;
	state->got_match = false;

	tcp_arg(pcb, state);
	tcp_err(pcb, jac_error);
	tcp_poll(pcb, jac_poll, 10);
	tcp_recv(pcb, jac_recv);

	int ret;
	if( ( ret = tcp_connect(pcb, &addr, 8080, jac_connected) ) != ERR_OK ) {
		UARTprintf("Could not connect (%d)\n", ret);
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

static uint8_t*
jac_skip_ws(uint8_t *buf, uint16_t len)
{
	for(int i=0; (*buf == ' ' || *buf == '\t' || *buf == '\r' || *buf == '\n') && i<len; buf++, i++);
	return buf;
}

static uint8_t*
jac_copy_until(uint8_t *buf, uint16_t buf_len, uint8_t *src, uint16_t src_len, uint8_t ch)
{
	uint16_t len = buf_len > src_len ? src_len : buf_len;

	int i;

	for(i=0; i<len && src[i] != ch; i++) {
		buf[i] = src[i];
	}

	return src+i;
}

static uint8_t*
jac_parse_constant_char(struct jac_state *state, uint8_t *buf, uint16_t len, int next_state, uint8_t ch)
{
	uint8_t *next;
	next = jac_skip_ws(buf, len);
	len -= next-buf;
	buf = next;

	if( *buf == ch ) {
		state->parse_state = next_state;
		//UARTprintf("Got '%c'\n", ch);
		buf++;
	} else {
		state->parse_state = JACP_INVALID;
	}
	return buf;
}

void event_handler(int event, void *data) {
}

static void
jac_parse_response(struct jac_state *state, uint8_t *buf, uint16_t len) 
{
	json_parse_buf(NULL, buf, len);
#if 0
	uint8_t *ptr = buf;
	uint8_t *next;

	//UARTprintf("State: %d\n", state->parse_state);

	while( len > 0 && state->parse_state != JACP_INVALID) {
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

		if( state->parse_state == JACP_HEADER_DATA) {
			// We skip all headers, so we are just looking for \r\n\r\n
			// We assume that \r\n\r\n is in a entire frame, which might not
			// be the case.
			for(int i = 0; i<len-4; i++) {
				if( ptr[i] == '\r' && ptr[i+1] == '\n' &&
						ptr[i+2] == '\r' && ptr[i+3] == '\n' ) {
					state->parse_state = JACP_DATA_START;
					ptr += i+4;
					len -= i+4;
					//UARTprintf("Header done, first char: %c\n", *ptr);
					break;
				}
			}
		}

		// We are looking for the entry in "jobs" that matches name == api-client-base.
		// Frame may split at any time, so we have an internal cache where we keep
		// data between frames
		if( state->parse_state == JACP_DATA_START) {
			next = jac_skip_ws(ptr, len);
			len -= next-ptr;
			ptr = next;

			if( *ptr == '{' ) {
				state->parse_state = JACP_DATA_JOBS_START;
				//UARTprintf("Got '{'\n");
				ptr++;
				len--;
			} else {
				UARTprintf("Expected '{', but got '%c'", *ptr);
				state->parse_state = JACP_INVALID;
			}
		}

		if( state->parse_state == JACP_DATA_JOBS_START ) {
			next = jac_parse_constant_char(state, ptr, len, JACP_DATA_JOBS_KEY_STRING, '"');
			len -= next - ptr;
			ptr = next;
		}

		if( state->parse_state == JACP_DATA_JOBS_KEY_STRING ) {
			/* Copy everything into jacp_data until '"' */
			next = jac_copy_until(state->jacp_data, 200, ptr, len, '"');
			state->jacp_data_used += next - ptr;
			len -= next - ptr;
			ptr = next;

			//UARTprintf("Next: %c\n", *next);

			if( *next == '"' ) {
				ptr++;
				len--;
				state->jacp_data[state->jacp_data_used] = '\0';
				//UARTprintf("Got string: '%s'\n", state->jacp_data);
				state->parse_state = JACP_DATA_JOBS_COLON;
			}
		}

		if( state->parse_state == JACP_DATA_JOBS_COLON ) {
			next = jac_parse_constant_char(state, ptr, len, JACP_DATA_JOBS_START_BRACKET, ':');
			len -= next - ptr;
			ptr = next;
		}

		if( state->parse_state == JACP_DATA_JOBS_START_BRACKET ) {
			next = jac_parse_constant_char(state, ptr, len, JACP_DATA_JOBS_ARRAY, '[');
			len -= next - ptr;
			ptr = next;
		}

		if( state->parse_state == JACP_DATA_JOBS_ARRAY ) {
			next = jac_parse_constant_char(state, ptr, len, JACP_DATA_JOBS_ENTRY_START, '{');
			len -= next - ptr;
			ptr = next;
		}

		if( state->parse_state == JACP_DATA_JOBS_ENTRY_START ) {
			next = jac_parse_constant_char(state, ptr, len, JACP_DATA_JOBS_KEY_STRING, '"');
			state->jacp_data_used = 0;
			len -= next - ptr;
			ptr = next;
		}

		if( state->parse_state == JACP_DATA_JOBS_KEY_STRING ) {
			/* Copy everything into jacp_data until '"' */
			next = jac_copy_until(state->jacp_data, 200-state->jacp_data_used, ptr, len, '"');
			state->jacp_data_used += next - ptr;
			len -= next - ptr;
			ptr = next;

			//UARTprintf("Next: %c\n", *next);

			if( *next == '"' ) {
				ptr++;
				len--;
				state->jacp_data[state->jacp_data_used] = '\0';
				//UARTprintf("Got string: '%s'\n", state->jacp_data);
				state->parse_state = JACP_DATA_JOBS_ENTRY_COLON;
				if( strncmp(state->jacp_data, "api-client-base", 15) == 0)  {
					UARTprintf("Match\n");
					state->got_match = true;
				}
			}
		}

		if( state->parse_state == JACP_DATA_JOBS_ENTRY_COLON ) {
			next = jac_parse_constant_char(state, ptr, len, JACP_DATA_JOBS_ENTRY_VALUE, ':');
			len -= next - ptr;
			ptr = next;
		}

		if( state->parse_state == JACP_DATA_JOBS_ENTRY_VALUE ) {
			next = jac_parse_constant_char(state, ptr, len, JACP_DATA_JOBS_ENTRY_VALUE_STRING, '"');
			len -= next - ptr;
			ptr = next;
		}

		if( state->parse_state == JACP_DATA_JOBS_ENTRY_VALUE_STRING ) {
			/* Copy everything into jacp_data until '"' */
			next = jac_copy_until(state->jacp_data, 200-state->jacp_data_used, ptr, len, '"');
			state->jacp_data_used += next - ptr;
			len -= next - ptr;
			ptr = next;

			//UARTprintf("Next: %c\n", *ptr);

			if( *ptr == '"' ) {
				ptr++;
				len--;
				state->jacp_data[state->jacp_data_used] = '\0';
				UARTprintf("Got value: '%s'\n", state->jacp_data);
				if( strncmp(state->jacp_data, "yellow", 6) == 0) {
					set_char('!', COLOR(15, 15, 0));
				} else if( strncmp(state->jacp_data, "blue", 4) == 0) {
					set_char('!', COLOR(0, 15, 0));
				}
				state->parse_state = JACP_DATA_JOBS_ENTRY_VALUE_DONE;
			}
		}

		if( state->parse_state == JACP_DATA_JOBS_ENTRY_VALUE_DONE ) {
			next = jac_skip_ws(ptr, len);
			len -= next - ptr;
			ptr = next;

			UARTprintf("ptr: %d\n", *ptr);
			UARTprintf("len: %d\n", len);

			if( len == 0 )
				return;

			if( *ptr == ',' ) {
				ptr++;
				len--;
				state->parse_state = JACP_DATA_JOBS_ENTRY_START;
			} else if( *ptr == '}' ) {
				ptr++;
				len--;
				state->parse_state = JACP_DATA_JOBS_ENTRY_END;
			} else {
				UARTprintf("Expected '}' or ',' but got '%c'", *ptr);
				state->parse_state = JACP_INVALID;
			}
		}

		if( state->parse_state == JACP_DATA_JOBS_ENTRY_END ) {
			next = jac_parse_constant_char(state, ptr, len, JACP_DATA_JOBS_ARRAY, ',');
			len -= next - ptr;
			ptr = next;
		}
	}
#endif
}
