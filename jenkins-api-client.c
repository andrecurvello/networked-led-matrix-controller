#include "jenkins-api-client.h"
#include "vartext.h"
#include <stdbool.h>

#include "font.h"
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
	enum jac_states state;
	struct tcp_pcb *pcb;
	int stateStack[50];
	struct ParserState parser_state;
	bool header_done;
	int match;
};

static void jac_error(void *arg, err_t err);
static err_t jac_poll(void *arg, struct tcp_pcb *tpcb);
static err_t jac_connected(void *arg, struct tcp_pcb *tpcb, err_t err);
static err_t jac_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);
static void jac_parse_response(struct jac_state *state, uint8_t *buf, uint16_t len);

static int jobsLevel = 0;
static char currentColor[10];
static bool setColor = false;
static bool setName = false;
static bool isMatch = false;
static int level = -1;

void
jenkins_get_status(ip_addr_t addr) {
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
	parser_init(&state->parser_state, state->stateStack, 50, ST_OBJECT);
	/*state->parse_state = JACP_HEADER_RESULT;
	state->jacp_data_used = 0;
	state->got_match = false;*/

	jobsLevel = 0;
	setColor = false;
	setName = false;
	isMatch = false;
	level = -1;

	tcp_arg(pcb, state);
	tcp_err(pcb, jac_error);
	tcp_poll(pcb, jac_poll, 10);
	tcp_recv(pcb, jac_recv);

	int ret;
	if( ( ret = tcp_connect(pcb, &addr, 8080, jac_connected) ) != ERR_OK ) {
		UARTprintf("Could not connect (%d)\n", ret);
		set_message("UNABLE TO CONNECT  ", 19);
		tcp_close(pcb);
		mem_free(state);
	}
}

static void
jac_error(void *arg, err_t err) 
{
	UARTprintf("Error: %d\n", err);
	set_message("ERROR ", 6);
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
	//UARTprintf("Connected\n");

	char *buf_end = request_header_expand(buf, "10.0.0.239");
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
		jac_parse_response((struct jac_state*)arg, (uint8_t*)q->payload, q->len);
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
event_handler(int event, void *data)
{

	//UARTprintf("event: %d\n", event);

	switch(event) {
		case EVENT_STRUCT_START:
			level++;
		//	UARTprintf("START struct\n");
			if( jobsLevel >= 1 ) {
				jobsLevel++;
				isMatch = false;
			}
		break;
		case EVENT_STRUCT_END:
			level--;
			if( jobsLevel == 3 ) {
				if( isMatch )  {
					UARTprintf("Color is %s\n", currentColor);
					if( strncmp(currentColor, "red", 3) == 0) {
						set_char(FONT_SAD_SMILEY, COLOR(15, 0, 0));
					} else if( strncmp(currentColor, "yellow", 6) == 0) {
						set_char(FONT_HAPPY_SMILEY, COLOR(15,15,0));
					} else {
						set_char(FONT_HAPPY_SMILEY, COLOR(0, 15, 0));
					}
				}
			}
			if( jobsLevel > 0 )
				jobsLevel--;
			//UARTprintf("END struct\n");
		break;
		case EVENT_ARRAY_START:
			//UARTprintf("START array\n");
			if( jobsLevel == 1 ) {
				jobsLevel++;
			}
		break;
		case EVENT_ARRAY_END:
			//UARTprintf("END array\n");
			if( jobsLevel == 2 ) {
				jobsLevel--;
			}
		break;
		case EVENT_KEY:
			//strcpy(names[level], (char*)data);
			//UARTprintf("Key: %s\n", (char*)data);
			if( level == 0 && strncmp("jobs", data, 4) == 0) {
				jobsLevel++;
			}
			if( jobsLevel == 3 && strncmp((char*)data, "color", 5) == 0) {
				setColor = true;
			} else {
				setColor = false;
			}

			if( jobsLevel == 3 && strncmp((char*)data, "name", 4) == 0) {
				setName = true;
			} else {
				setName = false;
			}
		break;
		case EVENT_STRING:
			for(int i=0; i<=level; i++) {
				//printf("%s.", names[i]);
			}
			//UARTprintf(" = %s\n", (char*)data);
			//UARTprintf("setName: %d\njobsLevel: %d\n", setName, jobsLevel);
			if( setColor && jobsLevel == 3 ) {
				strcpy(currentColor, (char*)data);
			} else if( setName && jobsLevel == 3 && strncmp((char*)data, "api-client-base", 15) == 0) {
				isMatch = true;
			}
		break;
		default:
		break;
	}
}

static void
jac_parse_response(struct jac_state *state, uint8_t *buf, uint16_t len) 
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
