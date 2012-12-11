#ifndef _PARSER_H
#define _PARSER_H

#include <stdbool.h>

#define OP_NONE 0
#define OP_PUSH 1
#define OP_POP  2

struct ParserState {
	int *stateStack;
	int stackSize;
	int stackTop;
	int lastOperation;
};

void parser_init(struct ParserState *ps, int *stack, int size, int initialState);
void parser_push_state(struct ParserState *ps, int state);
void parser_replace_state(struct ParserState *ps, int state);
int parser_pop_state(struct ParserState *ps);
#define parser_current_state(ps) (ps)->stateStack[(ps)->stackTop]
bool parser_is_ws(char ch);
bool parser_is(char ch, const char set[], int set_len);
int parser_prev_pop_state(struct ParserState *ps, int error_state);

#endif
