#include "jenkins-api-client.h"
#include "vartext.h"
#include <stdbool.h>

#include "json.h"

#define RED_SHIFT               0
#define GREEN_SHIFT     	4
#define COLOR(r,g,b) 		((r & 0xFF)+((g & 0xFF)<<GREEN_SHIFT))
extern void set_char(char c, uint16_t color);
extern void set_message(char *buf, uint16_t len);

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
	enum jac_states 	state;
	const char 		*hostname;
	struct tcp_pcb 		*pcb;
	int 			stateStack[50];
	struct JSONParserState 	parser_state;
	bool 			header_done;
	int 			match;
	jac_status_callback_t	status_cb;
	int level;
	int jobsLevel;
	bool isMatch;
	char currentColor[10];
	char currentName[50];
	bool setColor;
	bool setName;
};

static void jac_error(void *arg, err_t err);
static err_t jac_poll(void *arg, struct tcp_pcb *tpcb);
static err_t jac_connected(void *arg, struct tcp_pcb *tpcb, err_t err);
static err_t jac_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);
static void jac_parse_response(struct jac_state *state, const char *buf, uint16_t len);
static void event_handler(struct JSONParserState *ps, int event, void *data);

void
jenkins_get_status(ip_addr_t addr, const char *hostname, jac_status_callback_t cb) {
	struct tcp_pcb *pcb;
	
	pcb = tcp_new();
	
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
	state->header_done = false;
	state->match = 0;
	state->hostname = hostname;
	state->status_cb = cb;
	state->level = -1;
	state->jobsLevel = 0;
	state->isMatch = false;
	state->parser_state.event_callback = &event_handler;
	state->parser_state.user_state = state;
	//UARTprintf("state: %p\nparser_state: %p\n", state, &state->parser_state);
	parser_init(&state->parser_state.ps, state->stateStack, 50, ST_OBJECT);
	/*state->parse_state = JACP_HEADER_RESULT;
	state->jacp_data_used = 0;
	state->got_match = false;*/

	tcp_arg(pcb, state);
	tcp_err(pcb, jac_error);
	tcp_poll(pcb, jac_poll, 10);
	tcp_recv(pcb, jac_recv);

	int ret;
	if( ( ret = tcp_connect(pcb, &addr, 8080, jac_connected) ) != ERR_OK ) {
		UARTprintf("Could not connect (%d)\n", ret);
		//set_message("UNABLE TO CONNECT  ", 19);
		tcp_close(pcb);
		mem_free(state);
	}
}

static void
jac_error(void *arg, err_t err) 
{
	UARTprintf("Error: %d\n", err);
	//set_message("ERROR ", 6);
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
	struct jac_state *state = (struct jac_state*)arg;
	char buf[200];
	//UARTprintf("Connected\n");

	char *buf_end = request_header_expand(buf, state->hostname);
	buf_end[1] = '\0';
	//UARTprintf("%s\n", buf);

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
	//UARTprintf("Got data\n");

	if( p == NULL) {
		//UARTprintf("NULL data, remote closed\n");
		tcp_close(tpcb);
		mem_free(arg);
		return ERR_OK;
	}

	struct pbuf *q;
	for(q = p; q != NULL; q = q->next) {
		jac_parse_response((struct jac_state*)arg, (char*)q->payload, q->len);
	}

	tcp_recved(tpcb, p->tot_len);
	pbuf_free(p);

	return ERR_OK;
}

static void
jac_print(uint8_t *buf, uint16_t len) 
{
	for(int i=0; i<len; i++) {
		UARTprintf("%c", buf[i]);
	}
	UARTprintf("\n");
}


void
event_handler(struct JSONParserState *ps, int event, void *data)
{
	struct jac_state *state = ps->user_state;
	/*UARTprintf("event: %d\nps: %p\nstate: %p\n", event, ps, state);
	UARTFlushTx(false);*/
#if 1
	switch(event) {
		case EVENT_STRUCT_START:
			state->level++;
		//	UARTprintf("START struct\n");
			if( state->jobsLevel >= 1 ) {
				state->jobsLevel++;
			}
		break;
		case EVENT_STRUCT_END:
			state->level--;
			if( state->jobsLevel == 3 ) {
				state->status_cb(state->currentName, state->currentColor);
				/*if( state->isMatch )  {
					UARTprintf("Color is %s\n", state->currentColor);
					if( strncmp(state->currentColor, "red", 3) == 0) {
						set_char(FONT_SAD_SMILEY, COLOR(15, 0, 0));
					} else if( strncmp(state->currentColor, "yellow", 6) == 0) {
						set_char(FONT_HAPPY_SMILEY, COLOR(15,15,0));
					} else {
						set_char(FONT_HAPPY_SMILEY, COLOR(0, 15, 0));
					}
				}*/
			}
			if( state->jobsLevel > 0 )
				state->jobsLevel--;
			//UARTprintf("END struct\n");
		break;
		case EVENT_ARRAY_START:
			//UARTprintf("START array\n");
			if( state->jobsLevel == 1 ) {
				state->jobsLevel++;
			}
		break;
		case EVENT_ARRAY_END:
			//UARTprintf("END array\n");
			if( state->jobsLevel == 2 ) {
				state->jobsLevel--;
			}
		break;
		case EVENT_KEY:
			//strcpy(names[level], (char*)data);
			//UARTprintf("Key: %s\n", (char*)data);
			if( state->level == 0 && strncmp("jobs", data, 4) == 0) {
				state->jobsLevel++;
			}
			if( state->jobsLevel == 3 && strncmp((char*)data, "color", 5) == 0) {
				state->setColor = true;
			} else {
				state->setColor = false;
			}

			if( state->jobsLevel == 3 && strncmp((char*)data, "name", 4) == 0) {
				state->setName = true;
			} else {
				state->setName = false;
			}
		break;
		case EVENT_STRING:
			for(int i=0; i<=state->level; i++) {
				//printf("%s.", names[i]);
			}
			//UARTprintf(" = %s\n", (char*)data);
			//UARTprintf("setName: %d\njobsLevel: %d\n", setName, jobsLevel);
			if( state->setColor && state->jobsLevel == 3 ) {
				strcpy(state->currentColor, (char*)data);
			} else if( state->setName && state->jobsLevel == 3) {
				strcpy(state->currentName, (char*)data);
			}
		break;
		default:
		break;
	}
#endif
}

static void
jac_parse_response(struct jac_state *state, const char *buf, uint16_t len) 
{
	static const char header_match[] = "\r\n\r\n";
	if( !state->header_done ) {
		for(;len>0;len--,buf++) {
			if( *buf == header_match[state->match] ) {
				state->match++;
				if( state->match >= 4 ) {
					state->header_done = true;
					break;
				}
			} else {
				state->match = 0;
			}
		}
	}
	if( state->header_done ) {
		//jac_print(buf, len);
		json_parse_buf(&state->parser_state, buf, len);
	}
}
