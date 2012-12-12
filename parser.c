#include "parser.h"

void
parser_init(struct ParserState *ps, int *stack, int size, int initialState)
{
	ps->stateStack = stack;
	ps->stackSize = size;
	ps->stackTop = 0;
	ps->stateStack[0] = initialState;
	ps->lastOperation = OP_NONE;
}

void
parser_push_state(struct ParserState *ps, int state)
{
	ps->stackTop++;
	ps->stateStack[ps->stackTop] = state;
	ps->lastOperation = OP_PUSH;
}

void
parser_replace_state(struct ParserState *ps, int state)
{
	ps->stateStack[ps->stackTop] = state;
}

int
parser_pop_state(struct ParserState *ps) 
{
	int s = ps->stateStack[ps->stackTop];
	ps->stackTop--;
	ps->lastOperation = OP_POP;
	return s;
}

bool
parser_is_ws(char ch)
{
	switch(ch) {
		case '\0':
		case ' ':
		case '\r':
		case '\n':
		case '\t':
			return true;
			break;
		default:
			return false;
			break;
	}	
}

bool
parser_is(char ch, const char set[], int set_len)
{
	for(int i=0; i<set_len; i++) {
		if( ch == set[i] ) {
			return true;
		}
	}

	return false;
}

int
parser_prev_pop_state(struct ParserState *ps, int error_state)
{
	if( ps->lastOperation != OP_POP ) {
		return error_state;
	}
	ps->lastOperation = OP_NONE;
	return ps->stateStack[ps->stackTop+1];
}
