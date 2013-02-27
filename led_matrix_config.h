#ifndef _LED_MATRIX_CONFIG_H
#define _LED_MATRIX_CONFIG_H

#if 1
#include <inc/hw_ints.h>
#include <inc/hw_types.h>
#include <inc/hw_memmap.h>
#include <inc/hw_gpio.h>
#include <driverlib/gpio.h>

#define CLK_OUT_PORT		GPIO_PORTA_BASE
#define CLK_OUT_PIN		GPIO_PIN_4

#define LATCH_PORT		GPIO_PORTA_BASE
#define LATCH_PIN		GPIO_PIN_3

#define SER_OUT_PORT		GPIO_PORTA_BASE
#define SER_OUT_PIN		GPIO_PIN_2

#define ROW_SER_OUT_PORT	GPIO_PORTD_BASE
#define ROW_SER_OUT_PIN		GPIO_PIN_6

#define ROW_CLK_OUT_PORT	GPIO_PORTC_BASE
#define ROW_CLK_OUT_PIN		GPIO_PIN_5

#define ROW_LATCH_PORT		GPIO_PORTC_BASE
#define ROW_LATCH_PIN		GPIO_PIN_7

#define ROW_ENABLE_PORT		GPIO_PORTC_BASE
#define ROW_ENABLE_PIN		GPIO_PIN_6

#ifndef FAST_GPIOPinWrite
#define FAST_GPIOPinWrite(ulPort, ucPins, ucVal) HWREG((ulPort) + (GPIO_O_DATA + ((ucPins) << 2))) = (ucVal)
//#define FAST_GPIOPinWrite(ulPort, ucPins, ucVal) GPIOPinWrite((ulPort), (ucPins), (ucVal))
#endif
#endif
#endif
