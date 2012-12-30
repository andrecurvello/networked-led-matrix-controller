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
#include <driverlib/systick.h>

#include <lwip/init.h>
#include <lwip/netif.h>
#include <lwip/dhcp.h>
#include <lwip/tcp_impl.h>
#include <netif/etharp.h>

#include <utils/uartstdio.h>

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "font.h"
#include "enc28j60.h"
#include "jenkins-api-client.h"
#include "led_matrix.h"

#define delayMs(ms) (SysCtlDelay(((SysCtlClockGet() / 3) / 1000)*ms))

static inline void cpu_init(void);
static inline void uart_init(void);
static inline void spi_init(void);
void timer0_int_handler(void);
void SysTickIntHandler(void);

static void enc28j60_comm_init(void);
static void status_callback(const char *name, const char *color);

static void indexDisplay(void);

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
uint16_t index_view[LED_MATRIX_ROWS][LED_MATRIX_COLS];
volatile static unsigned long events;
volatile static unsigned long tickCounter;
uint8_t curRow, curCol;
uint8_t error_project_count;

#define MAX_ERROR_PROJECTS 2
#define MAX_NAME_LEN 30

char error_projects[MAX_ERROR_PROJECTS][MAX_NAME_LEN];

uint8_t index_view_mode;

#define INDEX_VIEW_MODE_INDEX 	0
#define INDEX_VIEW_MODE_ERRORED	1

#define FLAG_SYSTICK	0
#define FLAG_UPDATE	1
#define FLAG_ENC_INT	2

#define TICK_MS                 250
#define SYSTICKHZ               (1000/TICK_MS)

const uint8_t mac_addr[] = { 0x00, 0xC0, 0x033, 0x50, 0x48, 0x12 };

static uint8_t index_view_counter;
static uint8_t current_error_project;

int
main(void) {
	struct netif netif;

	cpu_init();
        uart_init();
        spi_init();

	MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
	MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);
	MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);
	MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);

	MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);

	// Setup LEDs
	MAP_GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE, GPIO_PIN_1);
	MAP_GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE, GPIO_PIN_2);
	MAP_GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE, GPIO_PIN_3);

        // Setup Display pins
	MAP_GPIOPinTypeGPIOOutput(LATCH_PORT, LATCH_PIN);
	MAP_GPIOPinTypeGPIOOutput(SER_OUT_PORT, SER_OUT_PIN);
	MAP_GPIOPinTypeGPIOOutput(CLK_OUT_PORT, CLK_OUT_PIN);

	// Setup SysTick timer
	MAP_SysTickPeriodSet(MAP_SysCtlClockGet() / SYSTICKHZ);
	MAP_SysTickEnable();
	MAP_SysTickIntEnable();

	// Configure timer 
	MAP_TimerConfigure(TIMER0_BASE, TIMER_CFG_A_PERIODIC | TIMER_CFG_B_PERIODIC | TIMER_CFG_SPLIT_PAIR);
	MAP_TimerLoadSet(TIMER0_BASE, TIMER_A, ROM_SysCtlClockGet()/20000);
	MAP_TimerLoadSet(TIMER0_BASE, TIMER_B, 15000);//ROM_SysCtlClockGet());
	MAP_TimerPrescaleSet(TIMER0_BASE, TIMER_B, 150);

	MAP_IntEnable(INT_TIMER0A);
	MAP_IntEnable(INT_TIMER0B);

	MAP_TimerIntEnable(TIMER0_BASE, TIMER_TIMA_TIMEOUT | TIMER_TIMB_TIMEOUT);
	MAP_TimerEnable(TIMER0_BASE, TIMER_BOTH);

	MAP_GPIOIntTypeSet(GPIO_PORTE_BASE, ENC_INT, GPIO_FALLING_EDGE);
	MAP_GPIOPinIntClear(GPIO_PORTE_BASE, ENC_INT);
	MAP_GPIOPinIntEnable(GPIO_PORTE_BASE, ENC_INT);

	MAP_IntMasterEnable();

	FAST_GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1, 0);

	FAST_GPIOPinWrite(LATCH_PORT, LATCH_PIN, 0);
	FAST_GPIOPinWrite(SER_OUT_PORT, SER_OUT_PIN, 0);
	FAST_GPIOPinWrite(CLK_OUT_PORT, CLK_OUT_PIN, 0);

	displayInit();
	set_message("LOADING  ", 9);
	
	enc28j60_comm_init();
	enc_init(mac_addr);

	lwip_init();

	ip_addr_t ipaddr, netmask, gw;

	IP4_ADDR(&gw, 0,0,0,0);
	IP4_ADDR(&ipaddr, 0,0,0,0);
	IP4_ADDR(&netmask, 0, 0, 0, 0);


	netif_add(&netif, &ipaddr, &netmask, &gw, NULL, enc28j60_init, ethernet_input);
	netif_set_default(&netif);
	dhcp_start(&netif);

#if 0
	for(int i=0; i<8; i++) {
		for(int l=0; l<8; l++) {
			if( msg_mode == MODE_SCROLL ) {
				fb[i][l] = ((font[msg[next_char]-32][i] >> (7-l)) & 0x1) * COLOR(0,15,0); //msg_color[next_char];
			} else {
			//fb[i][l] = static_data[i][l];
				fb[i][l] = 0;
			}
		}
	}

	next_char++;
#endif

        UARTprintf("Welcome\n");

	//memcpy(fb, static_data, 128);

	//set_char(FONT_HAPPY_SMILEY, COLOR(0, 15, 0));

	unsigned long last_arp_time, last_tcp_time, last_dhcp_coarse_time,
		      last_dhcp_fine_time, last_status_time, last_change;

	last_change = last_status_time = last_arp_time = last_tcp_time = last_dhcp_coarse_time = last_dhcp_fine_time = 0;

	bool status_done = false;

	// Do nothing :-)
	while(true) {
		MAP_SysCtlSleep();
#if 0
		if(HWREGBITW(&events, FLAG_UPDATE) == 1 && msg_mode == MODE_INDEX) {
			HWREGBITW(&events, FLAG_UPDATE) = 0;
			memcpy(fb, index_view, 128);
		} else 
#endif
		if(HWREGBITW(&events, FLAG_UPDATE) == 1) {
			HWREGBITW(&events, FLAG_UPDATE) = 0;
			displayAnimTick();
		}
		if(HWREGBITW(&events, FLAG_SYSTICK) == 1) {
			HWREGBITW(&events, FLAG_SYSTICK) = 0;

			if( (tickCounter - last_arp_time) * TICK_MS >= ARP_TMR_INTERVAL) {
				etharp_tmr();
				last_arp_time = tickCounter;
			}

			if( (tickCounter - last_tcp_time) * TICK_MS >= TCP_TMR_INTERVAL) {
				tcp_tmr();
				last_tcp_time = tickCounter;
			}
			if( (tickCounter - last_dhcp_coarse_time) * TICK_MS >= DHCP_COARSE_TIMER_MSECS) {
				dhcp_coarse_tmr();
				last_dhcp_coarse_time = tickCounter;
			}

			if( (tickCounter - last_dhcp_fine_time) * TICK_MS >= DHCP_FINE_TIMER_MSECS) {
				dhcp_fine_tmr();
				last_dhcp_fine_time = tickCounter;
			}

			if( (tickCounter - last_status_time) * TICK_MS >= 20000) {
				ip_addr_t addr;
				IP4_ADDR(&addr, 10, 0, 0, 239);
				jenkins_get_status(addr, "10.0.0.239", &status_callback);
				curRow = curCol = 0;
				last_status_time = tickCounter;
				status_done = true;
			}
		}
		
		if( HWREGBITW(&events, FLAG_ENC_INT) == 1 ) {
			HWREGBITW(&events, FLAG_ENC_INT) = 0;
			enc_action(&netif);
		}
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

void timer0_int_handler(void) {
	MAP_TimerIntClear(TIMER0_BASE, TIMER_TIMA_TIMEOUT);
	displayTick();
}

void timer0b_int_handler(void) {
	MAP_TimerIntClear(TIMER0_BASE, TIMER_TIMB_TIMEOUT);

	if( displayCheckUpdate() ) {
		HWREGBITW(&events, FLAG_UPDATE) = 1;
	}
/*	v = MAP_GPIOPinRead(GPIO_PORTF_BASE, GPIO_PIN_3);
	FAST_GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_3, v ^ GPIO_PIN_3);*/
}

void SysTickIntHandler(void) {
	tickCounter++;
	HWREGBITW(&events, FLAG_SYSTICK) = 1;
}

static void
enc28j60_comm_init(void) {
	MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);
	MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
	MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
	MAP_GPIOPinTypeGPIOOutput(GPIO_PORTB_BASE, ENC_CS);
	//MAP_GPIOPinTypeGPIOOutput(GPIO_PORTA_BASE, SRAM_CS);
	//  MAP_GPIOPinTypeGPIOOutput(GPIO_PORTA_BASE, ENC_CS | ENC_RESET | SRAM_CS);
	MAP_GPIOPinTypeGPIOInput(GPIO_PORTE_BASE, ENC_INT);

	//  MAP_GPIOPinWrite(GPIO_PORTA_BASE, ENC_RESET, 0);
	MAP_GPIOPinWrite(ENC_CS_PORT, ENC_CS, ENC_CS);
	//MAP_GPIOPinWrite(GPIO_PORTA_BASE, SRAM_CS, SRAM_CS);
	MAP_IntEnable(INT_GPIOE);

	MAP_GPIOIntTypeSet(GPIO_PORTE_BASE, ENC_INT, GPIO_FALLING_EDGE);
	MAP_GPIOPinIntClear(GPIO_PORTE_BASE, ENC_INT);
	MAP_GPIOPinIntEnable(GPIO_PORTE_BASE, ENC_INT);
}

void GPIOPortEIntHandler(void) {
	uint8_t p = MAP_GPIOPinIntStatus(GPIO_PORTE_BASE, true) & 0xFF;

	MAP_GPIOPinIntClear(GPIO_PORTE_BASE, p);

	HWREGBITW(&events, FLAG_ENC_INT) = 1;
}

uint8_t spi_send(uint8_t c) {
	unsigned long val;
	MAP_SSIDataPut(SSI2_BASE, c);
	MAP_SSIDataGet(SSI2_BASE, &val);
	return (uint8_t)val;
}

uint32_t
sys_now(void) {
	return tickCounter;
}

void status_callback(const char *name, const char *color)
{
	if( curRow == 0 && curCol == 0) {
		//clearDisplay(index_view);
		memcpy(fb, index_view, FB_SIZE);
		error_project_count = 0;
		index_view_counter = 0;
		index_view_mode = INDEX_VIEW_MODE_INDEX;
		displaySetAnim(indexDisplay, 2);
	}
	/*UARTprintf("Project %s has color %s\n", name, color);
	UARTFlushTx(false);*/
	uint16_t c = 0;

	if( strncmp(color, "blue", 4) == 0) {
		c = COLOR(0, 15, 0);
	} else if( strncmp(color, "disabled", 8) == 0) {
		return;
		c = COLOR(15, 15, 0);
	} else if( strncmp(color, "red", 3) == 0) {
		c = COLOR(15, 0, 0);
		strncpy(error_projects[error_project_count], name, MAX_NAME_LEN);
		error_project_count++;
	}
	

	index_view[curRow][curCol] = c;

	curCol++;
	if( curCol >= LED_MATRIX_COLS ) {
		curCol = 0;
		curRow = (curRow+1) % 7;
	}
}

static void
indexDisplay(void)
{
	if( index_view_mode == INDEX_VIEW_MODE_INDEX ) {
		memcpy(fb, index_view, FB_SIZE);
		index_view_counter++;
		if( index_view_counter > 30 ) {
			index_view_counter = 0;
			if( error_project_count > 0 ) {
				displaySetAnim(indexDisplay, 1);
				index_view_mode = INDEX_VIEW_MODE_ERRORED;
				current_error_project = 0;
				displayScrollTickSetMessage(error_projects[0], strlen(error_projects[0]));
			}
		}
	} else if( index_view_mode == INDEX_VIEW_MODE_ERRORED) {
		if( displayScrollTick() ) {
			current_error_project++;
			if( current_error_project >= error_project_count ) {
				index_view_mode = INDEX_VIEW_MODE_INDEX;
				displaySetAnim(indexDisplay, 2);
			} else {
				displayScrollTickSetMessage(error_projects[current_error_project], 
							    strlen(error_projects[current_error_project]));
			}
		}
	}
}
