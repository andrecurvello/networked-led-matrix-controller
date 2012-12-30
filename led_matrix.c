#include "led_matrix.h"

#include "font.h"
#include <string.h>

uint8_t msg_mode;
volatile uint16_t fb[LED_MATRIX_ROWS][LED_MATRIX_COLS];

static void shift_latch(void);
static void shift_out(uint8_t b);
static void shift_out_data(const uint16_t b[8], uint8_t shift, uint8_t threshold);
static void shift_out_row(uint8_t row, const uint16_t data[LED_MATRIX_COLS], uint8_t threshold);
static void displayScrollTickCb(void);

#define delayMs(ms) (SysCtlDelay(((SysCtlClockGet() / 3) / 1000)*ms))

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
	memset(fb, 0, FB_SIZE);
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
		if( ((b[8-1-i] >> shift) & 0xF) > threshold) {
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

void shift_out_row(uint8_t row, const uint16_t data[LED_MATRIX_COLS], uint8_t threshold)
{
	// Shift out left-most display first
	//shift_out_data(data, 8, current_color == 2 ? threshold : 15);
	shift_out_data(data, 4, current_color == 1 ? threshold : 15);
	shift_out_data(data, 0, current_color == 0 ? threshold : 15);

	// Shift out right-most display next
	//shift_out_data(data, 8, current_color == 2 ? threshold : 15);
	shift_out_data(data + 8, 4, current_color == 1 ? threshold : 15);
	shift_out_data(data + 8, 0, current_color == 0 ? threshold : 15);

	shift_out(row);

	shift_latch();
}

void set_char(char c, uint16_t color) {
	msg_mode = MODE_STATIC;
	for(int i=0; i<LED_MATRIX_ROWS; i++) {
		for(int l=0; l<LED_MATRIX_COLS; l++) {
			fb[i][l] = ((font[c-32][i] >> (LED_MATRIX_COLS-1-l)) & 0x1) * color;
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

void clearDisplay(uint16_t v[LED_MATRIX_ROWS][LED_MATRIX_COLS]) 
{
	for(int i=0; i<LED_MATRIX_ROWS; i++) {
		for(int l=0; l<LED_MATRIX_COLS; l++) {
			v[i][l] = 0x00;
		}
	}
}

void inline displayTick(void)
{
	shift_out_row(ROW(current_row), (const uint16_t*)fb[current_row], current_intensity);
	current_color++;

	if( current_color > 1) {
		current_color = 0;
		current_row++;
		if( current_row >= LED_MATRIX_ROWS ) {
			current_intensity++;
			current_row = 0;

			if( current_intensity > 15 ) {
				current_intensity = 0;
			}
		}
	}
}

void displayScrollTickCb(void)
{
	displayScrollTick();
}

bool displayScrollTick(void)
{
	bool done = false;

	for(int i=0; i<LED_MATRIX_ROWS; i++) {
		for(int l=0; l<LED_MATRIX_COLS-1; l++) {
			fb[i][l] = fb[i][l+1];
		}
		fb[i][LED_MATRIX_COLS-1] = ((font[msg[next_char]-32][i] >> (7-off)) & 0x1) * COLOR(0, 15, 0); //msg_color[next_char];
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
