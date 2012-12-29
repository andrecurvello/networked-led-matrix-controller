#include "led_matrix.h"

#include "font.h"
#include <string.h>

static void shift_latch(void);
static void shift_out(uint8_t b);
static void shift_out_data(const uint16_t b[8], uint8_t shift, uint8_t threshold);
static void shift_out_row(uint8_t row, const uint16_t data[8], uint8_t threshold);
static void displayScrollTickCb(void);

volatile char msg[50];
uint8_t msg_len;
uint8_t next_char = 0;
uint8_t off = 0;
static int current_intensity = 0;
static int current_row = 0;
static int current_color = 0;
static display_anim_callback_t display_anim_cb;
static uint8_t display_interval = 0;
static volatile uint8_t display_counter;

void displayInit(void) 
{
	msg_mode = MODE_STATIC;
	memset(fb, 0, 128);
	display_interval = 2;
	display_counter = 0;
}

static void
shift_latch(void) {
	//P1OUT |= LATCH_OUT;
	FAST_GPIOPinWrite(LATCH_PORT, LATCH_PIN, LATCH_PIN);
	//delayMs(1);
	FAST_GPIOPinWrite(LATCH_PORT, LATCH_PIN, 0);
	//P1OUT &= ~LATCH_OUT;
}


static void
shift_out(uint8_t b) {
	for(int i=0;i<8; i++) {
		if( b & 0x1) {
			FAST_GPIOPinWrite(SER_OUT_PORT, SER_OUT_PIN, SER_OUT_PIN);
			//P1OUT |= SER_OUT;
		} else {
			//P1OUT &= ~SER_OUT;
			FAST_GPIOPinWrite(SER_OUT_PORT, SER_OUT_PIN, 0);
		}
		b = b >> 1;
		//P1OUT |= CLK_OUT;
		FAST_GPIOPinWrite(CLK_OUT_PORT, CLK_OUT_PIN, CLK_OUT_PIN);
		//delayMs(10);
		//__delay_cycles(100);
		//P1OUT &= ~CLK_OUT;
		FAST_GPIOPinWrite(CLK_OUT_PORT, CLK_OUT_PIN, 0);
	}
}

void shift_out_data(const uint16_t b[8], uint8_t shift, uint8_t threshold) {
	for(int i=0;i<8; i++) {
		if( ((b[7-i] >> shift) & 0xF) > threshold) {
			FAST_GPIOPinWrite(SER_OUT_PORT, SER_OUT_PIN, SER_OUT_PIN);
		} else {
			FAST_GPIOPinWrite(SER_OUT_PORT, SER_OUT_PIN, 0);
		}
		//b = b >> 1;
		FAST_GPIOPinWrite(CLK_OUT_PORT, CLK_OUT_PIN, CLK_OUT_PIN);
		//delayMs(10);
		FAST_GPIOPinWrite(CLK_OUT_PORT, CLK_OUT_PIN, 0);
	}
}

void shift_out_row(uint8_t row, const uint16_t data[8], uint8_t threshold)
{
	shift_out_data(data, 8, current_color == 2 ? threshold : 15);
	shift_out_data(data, 4, current_color == 1 ? threshold : 15);
	shift_out_data(data, 0, current_color == 0 ? threshold : 15);
	shift_out(row);
	shift_latch();

#if 0
	shift_out_data(data, 8, 15);
	shift_out_data(data, 4, threshold);
	shift_out_data(data, 0, 15);
	shift_out(row);
	shift_latch();

	shift_out_data(data, 8, threshold);
	shift_out_data(data, 4, 15);
	shift_out_data(data, 0, 15);
	shift_out(row);

	shift_out(0);
	shift_out(0);
	shift_out(0);
	shift_out(row);
#endif
}

void set_char(char c, uint16_t color) {
	msg_mode = MODE_STATIC;
	for(int i=0; i<8; i++) {
		for(int l=0; l<8; l++) {
			fb[i][l] = ((font[c-32][i] >> (7-l)) & 0x1) * color;
		}
	}
}

void set_message(char *buf, uint16_t len) {
	set_char(' ', 0x00);
	displaySetAnim(displayScrollTickCb, 2);
	strncpy((char*)msg, buf, len);
	msg_len = len;
	next_char = 0;
	off = 0;
}

void displayScrollTickSetMessage(char *buf, uint16_t len)
{
	set_char(' ', 0x00);
	strncpy((char*)msg, buf, len);
	msg_len = len;
	next_char = 0;
	off = 0;
}

void clearDisplay(uint16_t v[8][8]) 
{
	for(int i=0; i<8; i++) {
		for(int l=0; l<8; l++) {
			v[i][l] = 0x00;
		}
	}
}

void inline displayTick(void)
{
#if 1
	shift_out_row(ROW(current_row), (const uint16_t*)fb[current_row], current_intensity);
	current_color++;

	if( current_color > 2 ) {
		current_color = 0;
		current_row++;
		if( current_row > 7 ) {
			current_intensity++;
			current_row = 0;

			if( current_intensity > 14 ) {
				current_intensity = 0;
			}
		}
	}
#endif
#if 0
	for(int l=0; l<15; l+=1) {
		for(int i=0; i<8; i++) {
                  for(current_color=0; current_color<3; current_color++) 
			shift_out_row(ROW(i), fb[i], l);
		}
	}
#endif
#if 0
		counter++;
		if( counter > 100000) {
			MAP_IntMasterDisable();
			counter = 0;
			for(int i=0; i<8; i++) {
				for(int l=0; l<7; l++) {
					fb[i][l] = fb[i][l+1];
				}
				fb[i][7] = ((font[msg[next_char]-48][i] >> (8-off)) & 0x1) * COLOR(15, 15, 0); //msg_color[next_char];
			}
			MAP_IntMasterEnable();
			off++;
			if( off >= 8) {
				off = 0;
				next_char++;
				if( next_char >= msg_len) {
					next_char = 0;
				}
			}
		}
#endif
}

void displayScrollTickCb(void)
{
	displayScrollTick();
}

bool displayScrollTick(void)
{
	bool done = false;

	for(int i=0; i<8; i++) {
		for(int l=0; l<7; l++) {
			fb[i][l] = fb[i][l+1];
		}
		fb[i][7] = ((font[msg[next_char]-32][i] >> (7-off)) & 0x1) * COLOR(0, 15, 0); //msg_color[next_char];
	}
	off++;
	if( off >= 8) {
		off = 0;
		next_char++;
		if( next_char >= msg_len) {
			done = true;
			next_char = 0;
		}
	}

	return done;
}

void
displaySetAnim(display_anim_callback_t cb, uint8_t interval)
{
	msg_mode = MODE_ANIM;
	display_anim_cb = cb;
	display_interval = interval;
	display_counter = 0;
}

uint8_t
displayGetInterval(void)
{
	return display_interval;
}

bool displayCheckUpdate(void)
{
	display_counter++;
	if( display_counter >= display_interval ) {
		display_counter = 0;
		return true;
	}
	return false;
}

void
displayAnimTick(void)
{
	display_anim_cb();
}
