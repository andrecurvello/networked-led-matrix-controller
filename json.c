#include "json.h"
#include <stdint.h>
#include <string.h>

#ifdef DEBUG
#include <stdio.h>

#define DPRINTF(x) printf(x)
#else
#define DPRINTF(x)
#endif

static const char string_chars[] = {'a','b','c','d','e','f','g','h','i','j','k',
	'l','m','n','o','p','q','r','s','t','u','v','w','x','y','z',
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
	'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z'};

static const int string_chars_len = sizeof(string_chars);


static char string_val[50];
static int string_val_len = 0;

/*static char obj_attr_name[50];
static char obj_attr_name_len = 0;

static char obj_attr_val[50];
static char obj_attr_val_len = 0;*/

void
json_parse_buf(struct JSONParserState *json_ps, const char *buf, int len) {
	bool inc;
	int prevPopState;
	struct ParserState *ps = (struct ParserState*)json_ps;

	// Deal with the characters one at a time
	while(len > 0 && parser_current_state(ps) != ST_INVALID) {
		inc = true;
		switch(parser_current_state(ps)) {
			case ST_OBJECT:
				prevPopState = parser_prev_pop_state(ps, ST_INVALID);
				if( prevPopState != ST_INVALID ) {
					if( prevPopState == ST_STRING ) {
						json_ps->event_callback(json_ps, EVENT_STRING, string_val);
					}
					inc = false;
					parser_pop_state(ps);
				} else if( *buf == '{' ) {
					json_ps->event_callback(json_ps, EVENT_STRUCT_START, NULL);
					parser_push_state(ps, ST_OBJ);
				} else if( *buf == '[' ) {
					json_ps->event_callback(json_ps, EVENT_ARRAY_START, NULL);
					parser_push_state(ps, ST_ARRAY);
				} else if( *buf == '"' ) {
					inc = false;
					parser_push_state(ps, ST_STRING);
				} else if( !parser_is_ws(*buf) ) {
					DPRINTF(("ST_OBJECT push invalid\n"));
					parser_push_state(ps, ST_INVALID);
				}
			break;
			case ST_ARRAY:
				prevPopState = parser_prev_pop_state(ps, ST_INVALID);
				if( prevPopState == ST_OBJECT ) {
					parser_replace_state(ps, ST_ARRAY_1);
					inc = false;
				} else {
					parser_push_state(ps, ST_OBJECT);
				}
			break;
			case ST_ARRAY_1:
				if( *buf == ',' ) {
					parser_replace_state(ps, ST_ARRAY);
				} else if( *buf == ']' ) {
					json_ps->event_callback(json_ps, EVENT_ARRAY_END, NULL);
					parser_pop_state(ps);
				} else if( !parser_is_ws(*buf) ) {
					DPRINTF(("ST_ARRAY_1 push invalid\n"));
					parser_push_state(ps, ST_INVALID);
				}
			break;
			case ST_OBJ:
				prevPopState = parser_prev_pop_state(ps, ST_INVALID);
				if( prevPopState == ST_STRING ) {
					json_ps->event_callback(json_ps, EVENT_KEY, string_val);
					//strcpy(obj_attr_name, string_val);
					parser_push_state(ps, ST_OBJ_1);
					inc = false;
				} else if( prevPopState == ST_OBJ_1) {
					//strcpy(obj_attr_val, string_val);
					parser_replace_state(ps, ST_OBJ_2);
					inc = false;
				} else if( !parser_is_ws(*buf) ) {
					//obj_attr_name_len = 0;
					//obj_attr_val_len = 0;
					parser_push_state(ps, ST_STRING);
					inc = false;
				}
			break;
			case ST_OBJ_1:
				if( parser_prev_pop_state(ps, ST_INVALID) == ST_OBJECT ) {
					parser_pop_state(ps);
					inc = false;
				} else if( *buf == ':' ) {
					string_val_len = 0;
					parser_push_state(ps, ST_OBJECT);
				} else if( !parser_is_ws(*buf) ) {
					DPRINTF(("ST_OBJ_1 pushed invalid\n"));
					parser_push_state(ps, ST_INVALID);
				}
			break;
			case ST_OBJ_2:
				if( *buf == ',' ) {
					parser_replace_state(ps, ST_OBJ);
				} else if( !parser_is_ws(*buf) ) {
					inc = false;
					parser_replace_state(ps, ST_OBJ_3);
				}
			break;
			case ST_OBJ_3:
				if( *buf == '}' ) {
					json_ps->event_callback(json_ps, EVENT_STRUCT_END, NULL);
					parser_pop_state(ps);
				} else if( !parser_is_ws(*buf) ) {
					parser_push_state(ps, ST_INVALID);
				}
			break;
			case ST_STRING:
				string_val_len = 0;
				if( parser_prev_pop_state(ps, ST_INVALID) == ST_DSTRING ) {
					parser_pop_state(ps);
					inc = false;
				} else if( *buf == '"' ) {
					parser_push_state(ps, ST_DSTRING);
				} else if( !parser_is_ws(*buf) ) {
					DPRINTF(("Invalid char: %c\n", *buf));
					parser_push_state(ps, ST_INVALID);
				}
			break;
			case ST_DSTRING:
				if( *buf == '"' ) {
					parser_pop_state(ps);
					string_val[string_val_len] = 0;
				}  else {
					/*UARTprintf("C: '%c'\n", *buf);
					UARTFlushTx(false);*/
					string_val[string_val_len++] = *buf;
				}
			break;
		}

		if( inc && parser_current_state(ps) != ST_INVALID) {
			len--;
			buf++;
		}
	}

	if( parser_current_state(ps) == ST_INVALID ) {
		DPRINTF(("Invalid state on char '%c', previous was: %d\n", *buf, ps->stateStack[ps->stackTop-1]));
#ifdef DEBUG
		for(int i=ps->stackTop; i>=0; i--) {
			DPRINTF(("[%d] = %d\n", i, ps->stateStack[i]));
		}
#endif
	}
}
