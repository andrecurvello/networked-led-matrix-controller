#ifndef _JSON_H
#define _JSON_H

#include "parser.h"

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
		   '[' + ST_ARRAY |
		   ST_STRING

   ST_OBJ 	:= ST_STRING(key) + ST_OBJ_1 + ST_OBJ_2 + ST_OBJ3
   ST_OBJ_1 	:= ':' + ST_OBJECT(value)
   ST_OBJ_2	:= ',' + ST_OBJ | e
   ST_OBJ_3	:= '}'

   ST_ARRAY	:= ST_OBJECT + ST_ARRAY_1
   ST_ARRAY_1	:= ',' + ST_ARRAY |
		   ']'
*/
#define EVENT_STRUCT_START 	1
#define EVENT_STRUCT_END	2
#define EVENT_ARRAY_START	3
#define EVENT_ARRAY_END		4
#define EVENT_KEY		5
#define EVENT_STRING		6

void event_handler(int event, void *data);
void json_parse_buf(struct ParserState *ps, const char *buf, int len);

#endif
