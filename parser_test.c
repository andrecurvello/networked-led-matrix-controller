#include "json.h"

#include <stdio.h>
#include <string.h>

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
	}, \n\
	\"Hej\"\n\
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

static int stateStack[50];
struct ParserState parserState;

int
main(int argc, char *argv[]) {
	parser_init(&parserState, stateStack, 50, ST_OBJECT);

	printf("%s\n", test_string);
	//printf("%s\n", array_test_string);

	// We feed the parser one character at a time in order to ensure that
	// it is stable in all cases
	for(int i=0; i<strlen(test_string); i++) {
		json_parse_buf(&parserState, test_string + i, 1);

		if( parser_current_state(&parserState) == ST_INVALID ) {
			printf("INVALID STATE\n");
			break;
		}
	}
}

static char names[5][20];
static int level = -1;

void
event_handler(int event, void *data)
{
	switch(event) {
		case EVENT_STRUCT_START:
			level++;
			printf("START struct\n");
		break;
		case EVENT_STRUCT_END:
			level--;
			printf("END struct\n");
		break;
		case EVENT_ARRAY_START:
			printf("START array\n");
		break;
		case EVENT_ARRAY_END:
			printf("END array\n");
		break;
		case EVENT_KEY:
			strcpy(names[level], (char*)data);
			//printf("Key: %s\n", (char*)data);
		break;
		case EVENT_STRING:
			for(int i=0; i<=level; i++) {
				printf("%s.", names[i]);
			}
			printf(" = %s\n", (char*)data);
		break;
		default:
		break;
	}
}
