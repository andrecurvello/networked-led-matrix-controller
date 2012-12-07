#include <inc/hw_ints.h>
#include <inc/hw_types.h>
#include <inc/hw_memmap.h>
#include <inc/hw_gpio.h>
#include <driverlib/rom.h>
#include <driverlib/rom_map.h>
#include <driverlib/pin_map.h>
#include <driverlib/sysctl.h>
#include <driverlib/gpio.h>
#include <driverlib/timer.h>
#include <driverlib/interrupt.h>
#include <driverlib/uart.h>
#include <driverlib/ssi.h>

#include <utils/uartstdio.h>

#include <stdbool.h>
#include <stdint.h>

#include "font.h"

#define delayMs(ms) (SysCtlDelay(((SysCtlClockGet() / 3) / 1000)*ms))

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


#define FAST_GPIOPinWrite(ulPort, ucPins, ucVal) HWREG(ulPort + (GPIO_O_DATA + (ucPins << 2))) = ucVal

static inline void cpu_init(void);
static inline void uart_init(void);
static inline void spi_init(void);
static void shift_latch(void);
static void shift_out(uint8_t b);
static void shift_out_data(const uint16_t b[8], uint8_t shift, uint8_t threshold);
static void shift_out_row(uint8_t row, const uint16_t data[8], uint8_t threshold);
void timer0_int_handler(void);

static int current_intensity = 0;
static int current_row = 0;
static int current_color = 0;
const uint16_t static_data[8][8] = {
		{0xFF, 0xF0, 0xFF, 0x0F, 0xFF, 0x00, 0xFF, 0x00},
		{0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF},
		{0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00},
		{0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF},
		{0xF0, 0x00, 0xF0, 0x00, 0xF0, 0x00, 0xF0, 0x00},
		{0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF},
		{0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00},
		{0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF},
};
volatile uint16_t fb[8][8];
static volatile int counter = 0;
uint8_t msg[] = "0123456789";
const uint8_t msg_len = sizeof(msg)-1;
uint8_t msg_color[4] = {COLOR(15,0,0), COLOR(0,15,0), COLOR(15,10,0), COLOR(5,15,0)};
uint8_t next_char = 0;
uint8_t off = 0;

int
main(void) {
	cpu_init();
        uart_init();
        spi_init();

        UARTprintf("Welcome\n");

	MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
	MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);
	MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);
	MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);

	MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);
	MAP_IntMasterEnable();

	// Setup LEDs
	MAP_GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE, GPIO_PIN_1);
	MAP_GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE, GPIO_PIN_2);
	MAP_GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE, GPIO_PIN_3);

        // Setup Display pins
	MAP_GPIOPinTypeGPIOOutput(LATCH_PORT, LATCH_PIN);
	MAP_GPIOPinTypeGPIOOutput(SER_OUT_PORT, SER_OUT_PIN);
	MAP_GPIOPinTypeGPIOOutput(CLK_OUT_PORT, CLK_OUT_PIN);

	// Configure timer 
	MAP_TimerConfigure(TIMER0_BASE, TIMER_CFG_A_PERIODIC | TIMER_CFG_B_PERIODIC | TIMER_CFG_SPLIT_PAIR);
	MAP_TimerLoadSet(TIMER0_BASE, TIMER_A, ROM_SysCtlClockGet()/20000);
	MAP_TimerLoadSet(TIMER0_BASE, TIMER_B, 20000);//ROM_SysCtlClockGet());
	MAP_TimerPrescaleSet(TIMER0_BASE, TIMER_B, 255);

	MAP_IntEnable(INT_TIMER0A);
	MAP_IntEnable(INT_TIMER0B);

	MAP_TimerIntEnable(TIMER0_BASE, TIMER_TIMA_TIMEOUT | TIMER_TIMB_TIMEOUT);

	MAP_TimerEnable(TIMER0_BASE, TIMER_BOTH);

	FAST_GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1, 0);

	FAST_GPIOPinWrite(LATCH_PORT, LATCH_PIN, 0);
	FAST_GPIOPinWrite(SER_OUT_PORT, SER_OUT_PIN, 0);
	FAST_GPIOPinWrite(CLK_OUT_PORT, CLK_OUT_PIN, 0);

	shift_out(0x00);
	shift_out(0x00);
	shift_latch();


	for(int i=0; i<8; i++) {
		for(int l=0; l<8; l++) {
			//fb[i][l] = ((font[msg[next_char]-48][i] >> (7-l)) & 0x1) * COLOR(15,15,0); //msg_color[next_char];
			fb[i][l] = static_data[i][l];
		}
	}

	//memcpy(fb, static_data, 128);

	// Do nothing :-)
	while(true) {
#if 0
		//for(int l=0; l<15; l+=1) {
			for(int i=0; i<8; i++) {
				shift_out_row(ROW(i), fb[i], 14);
			}
		//}
#endif
		//delayMs(100);
		/*v = MAP_GPIOPinRead(GPIO_PORTF_BASE, GPIO_PIN_1);
		FAST_GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1, v ^ GPIO_PIN_1);*/
		//counter++;
#if 1
		if( counter > 10) {
			MAP_IntMasterDisable();
			counter = 0;
			for(int i=0; i<8; i++) {
				for(int l=0; l<7; l++) {
					fb[i][l] = fb[i][l+1];
				}
				fb[i][7] = ((font[msg[next_char]-48][i] >> (8-off)) & 0x1) * COLOR(5, 5, 0); //msg_color[next_char];
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
}

static inline void cpu_init(void) {
	int i;
	for(i=0; i<1000000; i++);

	// Setup for 16MHZ external crystal, use 200MHz PLL and divide by 4 = 50MHz
	MAP_SysCtlClockSet(SYSCTL_SYSDIV_4 | SYSCTL_USE_PLL | SYSCTL_OSC_MAIN |
			SYSCTL_XTAL_16MHZ);
}

static void
uart_init(void) {
  MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);

  // Configure PA0 and PA1 for UART
  MAP_GPIOPinConfigure(GPIO_PA0_U0RX);
  MAP_GPIOPinConfigure(GPIO_PA1_U0TX);
  MAP_GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);
  UARTStdioInitExpClk(0, 115200);
}

static void
spi_init(void) {
  MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);

  // Configure SSI1 for SPI RAM usage
  MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_SSI2);
  MAP_GPIOPinConfigure(GPIO_PB4_SSI2CLK);
  MAP_GPIOPinConfigure(GPIO_PB6_SSI2RX);
  MAP_GPIOPinConfigure(GPIO_PB7_SSI2TX);
  MAP_GPIOPinTypeSSI(GPIO_PORTB_BASE, GPIO_PIN_4 | GPIO_PIN_6 | GPIO_PIN_7);
  MAP_SSIConfigSetExpClk(SSI2_BASE, MAP_SysCtlClockGet(), SSI_FRF_MOTO_MODE_0,
                         SSI_MODE_MASTER, 1000000, 8);
  MAP_SSIEnable(SSI2_BASE);

  unsigned long b;
  while(MAP_SSIDataGetNonBlocking(SSI2_BASE, &b)) {}
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


void timer0_int_handler(void) {
	long v;
	MAP_TimerIntClear(TIMER0_BASE, TIMER_TIMA_TIMEOUT);

#if 1
	shift_out_row(ROW(current_row), fb[current_row], current_intensity);
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

void timer0b_int_handler(void) {
	long v;

	MAP_TimerIntClear(TIMER0_BASE, TIMER_TIMB_TIMEOUT);

	counter++;
/*	v = MAP_GPIOPinRead(GPIO_PORTF_BASE, GPIO_PIN_3);
	FAST_GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_3, v ^ GPIO_PIN_3);*/
}
