#include "json.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

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

	FILE *file = fopen(argv[1], "r");

	//printf("%s\n", test_string);
	//printf("%s\n", array_test_string);

	// We feed the parser one character at a time in order to ensure that
	// it is stable in all cases
	//for(int i=0; i<strlen(test_string); i++) {
	while(!feof(file)) {
		char buf[1];
		if( fread(buf, 1, 1, file) < 1 ) {
			break;
		}
		json_parse_buf(&parserState, buf, 1);

		if( parser_current_state(&parserState) == ST_INVALID ) {
			printf("INVALID STATE\n");
			break;
		}
	}
}

static char names[5][20];

void
event_handler(int event, void *data)
{
	static int jobsLevel = 0;
	static char currentColor[10];
	static bool setColor = false;
	static bool setName = false;
	static bool isMatch = false;
	static int level = -1;

	switch(event) {
		case EVENT_STRUCT_START:
			level++;
			//printf("START struct\n");
			if( jobsLevel >= 1 ) {
				jobsLevel++;
				isMatch = false;
			}
		break;
		case EVENT_STRUCT_END:
			level--;
			if( jobsLevel == 3 ) {
				if( isMatch ) 
					printf("Color is %s\n", currentColor);
			}
			if( jobsLevel > 0 )
				jobsLevel--;
			//printf("END struct\n");
		break;
		case EVENT_ARRAY_START:
			//printf("START array\n");
			if( jobsLevel == 1 ) {
				jobsLevel++;
			}
		break;
		case EVENT_ARRAY_END:
			//printf("END array\n");
			if( jobsLevel == 2 ) {
				jobsLevel--;
			}
		break;
		case EVENT_KEY:
			strcpy(names[level], (char*)data);
			//printf("Key: %s\n", (char*)data);
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
			//printf(" = %s\n", (char*)data);
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
