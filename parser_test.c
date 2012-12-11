#include "parser.h"

#include <stdio.h>
#include <string.h>

static const char string_chars[] = {'a','b','c','d','e','f','g','h','i','j','k',
	'l','m','n','o','p','q','r','s','t','u','v','w','x','y','z',
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
	'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z'};

static const int string_chars_len = sizeof(string_chars);

const char *simple_test_string =
" 	{ \n\
		\"name\" : \"Project 1\",\n\
		\"color\" : \"blue\",\n\
		\"description\":\"Hello World\"\n\
	}";

const char *array_test_string =
"[ 	{ \n\
		\"name\" : \"Project 1\",\n\
		\"color\" : \"blue\",\n\
		\"description\" : \"Hello World\",\n\
		\"description1\":\"Hello World\"\n\
	}, \n\
	{ \n\
		\"name\" : \"Project 2\"\n\
	} \n\
]";

const char *test_string =
"{ \"jobs\" : [\n\
 	{ \n\
		\"name\" : \"Project1\",\n\
		\"color\" : \"blue\"\n\
	}, \n\
	{ \n\
		\"name\" : \"Project2\",\n\
		\"color\" : \"yellow\"\n\
	}\n\
] }";

static void parse_test(struct ParserState *state, const char *buf, int len);

static int stateStack[5];
struct ParserState parserState;

enum States {
	ST_OBJECT 	= 0,
	ST_OBJ		= 1,
	ST_OBJ_1 	= 2,
	ST_OBJ_2	= 3,
	ST_OBJ_3	= 4,
	ST_STRING	= 5,
	ST_DSTRING	= 6,
	ST_ARRAY	= 7,
	ST_ARRAY_1	= 8,
	ST_INVALID	= 9
};

/* ST_OBJECT 	:= '{' + ST_OBJ |
		   '[' + ST_ARRAY

   ST_OBJ 	:= ST_STRING(key) + ST_OBJ_1 + ST_OBJ_2 + ST_OBJ3
   ST_OBJ_1 	:= ':' + ST_STRING(value)
   ST_OBJ_2	:= ',' + ST_OBJ | e
   ST_OBJ_3	:= '}'

   ST_ARRAY	:= ST_OBJECT + ST_ARRAY_1
   ST_ARRAY_1	:= ',' + ST_ARRAY |
		   ']'
*/


static char string_val[50];
static int string_val_len = 0;

static char obj_attr_name[50];
static char obj_attr_name_len = 0;

static char obj_attr_val[50];
static char obj_attr_val_len = 0;

int
main(int argc, char *argv[]) {
	parser_init(&parserState, stateStack, 5, ST_OBJECT);

	//printf("%s\n", test_string);
	printf("%s\n", array_test_string);

	// We feed the parser one character at a time in order to ensure that
	// it is stable in all cases
	for(int i=0; i<strlen(array_test_string); i++) {
		parse_test(&parserState, array_test_string + i, 1);

		if( parser_current_state(&parserState) == ST_INVALID ) {
			break;
		}
	}
}

static void
parse_test(struct ParserState *ps, const char *buf, int len) {
	bool inc;
	int prevPopState;

	// Deal with the characters one at a time
	while(len > 0 && parser_current_state(ps) != ST_INVALID) {
		inc = true;
		switch(parser_current_state(ps)) {
			case ST_OBJECT:
				prevPopState = parser_prev_pop_state(ps, ST_INVALID);
				if( prevPopState != ST_INVALID ) {
					inc = false;
					parser_pop_state(ps);
				} else if( *buf == '{' ) {
					parser_push_state(ps, ST_OBJ);
				} else if( *buf == '[' ) {
					parser_push_state(ps, ST_ARRAY);
				} else if( !parser_is_ws(*buf) ) {
					printf("ST_OBJECT push invalid\n");
					parser_push_state(ps, ST_INVALID);
				}
			break;
			case ST_ARRAY:
				prevPopState = parser_prev_pop_state(ps, ST_INVALID);
				printf("prevPopState: %d\n", prevPopState);
				if( prevPopState == ST_OBJECT ) {
					parser_replace_state(ps, ST_ARRAY_1);
					inc = false;
				} else {
					parser_push_state(ps, ST_OBJECT);
				}
			break;
			case ST_ARRAY_1:
				if( *buf == ',' ) {
					printf("Replace ST_ARRAY\n");
					parser_replace_state(ps, ST_ARRAY);
				} else if( *buf == ']' ) {
					parser_pop_state(ps);
				} else if( !parser_is_ws(*buf) ) {
					printf("ST_ARRAY_1 push invalid\n");
					parser_push_state(ps, ST_INVALID);
				}
			break;
			case ST_OBJ:
				prevPopState = parser_prev_pop_state(ps, ST_INVALID);
				if( prevPopState == ST_STRING ) {
					printf("Key: '%s'\n", string_val);
					strcpy(obj_attr_name, string_val);
					parser_push_state(ps, ST_OBJ_1);
					inc = false;
				} else if( prevPopState == ST_OBJ_1) {
					printf("Value: '%s'\n", string_val);
					strcpy(obj_attr_val, string_val);
					parser_replace_state(ps, ST_OBJ_2);
					inc = false;
				} else if( !parser_is_ws(*buf) ) {
					obj_attr_name_len = 0;
					obj_attr_val_len = 0;
					parser_push_state(ps, ST_STRING);
					inc = false;
				}
			break;
			case ST_OBJ_1:
				if( parser_prev_pop_state(ps, ST_INVALID) == ST_STRING ) {
					parser_pop_state(ps);
					inc = false;
				} else if( *buf == ':' ) {
					string_val_len = 0;
					parser_push_state(ps, ST_STRING);
				} else if( !parser_is_ws(*buf) ) {
					printf("ST_OBJ_1 pushed invalid\n");
					parser_push_state(ps, ST_INVALID);
				}
			break;
			case ST_OBJ_2:
				if( *buf == ',' ) {
					printf("Attribute constructed, %s = %s\n", obj_attr_name, obj_attr_val);
					parser_replace_state(ps, ST_OBJ);
				} else if( !parser_is_ws(*buf) ) {
					printf("Attribute constructed, %s = %s\n", obj_attr_name, obj_attr_val);
					inc = false;
					parser_replace_state(ps, ST_OBJ_3);
				}
			break;
			case ST_OBJ_3:
				if( *buf == '}' ) {
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
					printf("Invalid char: %c\n", *buf);
					parser_push_state(ps, ST_INVALID);
				}
			break;
			case ST_DSTRING:
				if( *buf == '"' ) {
					parser_pop_state(ps);
					string_val[string_val_len] = 0;
					printf("String: '%s'\n", string_val);
				}  else {
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
		printf("Invalid state on char '%c', previous was: %d\n", *buf, ps->stateStack[ps->stackTop-1]);
		for(int i=ps->stackTop; i>=0; i--) {
			printf("[%d] = %d\n", i, ps->stateStack[i]);
		}
	}
}
