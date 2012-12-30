#ifndef _LED_MATRIX_H
#define _LED_MATRIX_H

#include <stdint.h>
#include <stdbool.h>

#include <inc/hw_ints.h>
#include <inc/hw_types.h>
#include <inc/hw_memmap.h>
#include <inc/hw_gpio.h>
#include <driverlib/gpio.h>

#define ROW(n) 			(1<<n)

#define RED_SHIFT               0
#define GREEN_SHIFT     	4

#define COLOR(r,g,b) 		((r & 0xFF)+((g & 0xFF)<<GREEN_SHIFT))

#define CLK_OUT_PORT		GPIO_PORTA_BASE
#define CLK_OUT_PIN		GPIO_PIN_4

#define LATCH_PORT		GPIO_PORTA_BASE
#define LATCH_PIN		GPIO_PIN_3

#define SER_OUT_PORT		GPIO_PORTA_BASE
#define SER_OUT_PIN		GPIO_PIN_2

#ifndef FAST_GPIOPinWrite
#define FAST_GPIOPinWrite(ulPort, ucPins, ucVal) HWREG(ulPort + (GPIO_O_DATA + (ucPins << 2))) = ucVal
#endif

#define LED_MATRIX_ROWS		8
#define LED_MATRIX_COLS		16

#define FB_SIZE			(LED_MATRIX_ROWS*LED_MATRIX_COLS*2)

extern uint8_t msg_mode;
extern volatile uint16_t fb[LED_MATRIX_ROWS][LED_MATRIX_COLS];

#define MODE_STATIC	0
#define MODE_ANIM	1

typedef void (*display_anim_callback_t)(void);

void set_message(char *buf, uint16_t len);
void clearDisplay(uint16_t v[LED_MATRIX_ROWS][LED_MATRIX_COLS]);
void displayTick(void);
void displayScrollTickSetMessage(char *buf, uint16_t len);
bool displayScrollTick(void);
void displayInit(void);
void displaySetAnim(display_anim_callback_t cb, uint8_t interval);
uint8_t displayGetInterval(void);
bool displayCheckUpdate(void);
void displayAnimTick(void);

#endif
