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

#include "led-matrix-lib/LedMatrix.hpp"
#include "led-matrix-lib/LedMatrixSimpleFont.hpp"
#include "led-matrix-lib/TestAnimation.hpp"
#include "led-matrix-lib/PulseAnimation.hpp"
#include "led-matrix-lib/SPIFrameBuffer.hpp"

#include "httpd.hpp"

#include <mcu++/gpio.hpp>
#include <mcu++/stellaris_gpio.hpp>

#ifdef __cplusplus
extern "C" {
#endif
#include "enc28j60.h"
#include "jenkins-api-client.h"
//#include "led_matrix_config.h"
#ifdef __cplusplus
}
#endif

#define delayMs(ms) (SysCtlDelay(((SysCtlClockGet() / 3) / 1000)*ms))

static inline void cpu_init(void);
static inline void uart_init(void);
static inline void spi_init(void);

#ifdef __cplusplus
extern "C" {
#endif
void SysTickIntHandler(void);
void timer0_int_handler(void);
#ifdef __cplusplus
}
#endif

static void enc28j60_comm_init(void);
#ifdef __cplusplus
extern "C" {
#endif
static void status_callback(const char *name, const char *color);
#ifdef __cplusplus
}
#endif

volatile static unsigned long events;
volatile static unsigned long tickCounter;

#define FLAG_SYSTICK	0
#define FLAG_UPDATE	1
#define FLAG_ENC_INT	2

#define TICK_MS                 250
#define SYSTICKHZ               (1000/TICK_MS)

#ifdef __cplusplus
extern "C" {
#endif
uint8_t mac_addr[] = { 0x00, 0xC0, 0x033, 0x50, 0x48, 0x12 };
#ifdef __cplusplus
}
#endif

static uint8_t index_view_counter;
static uint8_t current_error_project;

#if 0
class LedConfig {
public:
	static const uint16_t Rows = 8;
	static const uint16_t Cols = 16;
	static const uint16_t Levels = 32;

	typedef MCU::StaticStellarisGPIO<GPIO_PORTC_BASE, 6> GPIORowEnable;
	typedef MCU::StaticStellarisGPIO<GPIO_PORTC_BASE, 7> GPIORowLatch;
	typedef MCU::StaticStellarisGPIO<GPIO_PORTC_BASE, 5> GPIORowClock;
	typedef MCU::StaticStellarisGPIO<GPIO_PORTD_BASE, 6> GPIORowOutput;

	typedef MCU::StaticStellarisGPIO<GPIO_PORTA_BASE, 2> GPIOColOutput;
	typedef MCU::StaticStellarisGPIO<GPIO_PORTA_BASE, 4> GPIOColClock;
	typedef MCU::StaticStellarisGPIO<GPIO_PORTA_BASE, 3> GPIOColLatch;
};

LedMatrixFrameBuffer<LedConfig>	frameBuffer;
#endif

typedef MCU::StaticStellarisGPIO<GPIO_PORTA_BASE, 3> ChipSelectPin;

class LedConfig {
public:
	static const uint16_t Rows = 16;
	static const uint16_t Cols = 16;
	static const uint16_t Levels = 32;

	static void SPIInit() {
		MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_SSI0);
		MAP_GPIOPinConfigure(GPIO_PA2_SSI0CLK);
		MAP_GPIOPinConfigure(GPIO_PA4_SSI0RX);
		MAP_GPIOPinConfigure(GPIO_PA5_SSI0TX);

		MAP_GPIOPinTypeSSI(GPIO_PORTA_BASE, GPIO_PIN_2 | GPIO_PIN_4 | GPIO_PIN_5);
		MAP_SSIConfigSetExpClk(SSI0_BASE, MAP_SysCtlClockGet(), SSI_FRF_MOTO_MODE_0, SSI_MODE_MASTER, 1000000, 8);
		MAP_SSIEnable(SSI0_BASE);
		unsigned long b;
		while(MAP_SSIDataGetNonBlocking(SSI0_BASE, &b)) {}
	}

	static void SpiSend(uint8_t c) {
		unsigned long val;
		MAP_SSIDataPut(SSI0_BASE, c);
		MAP_SSIDataGet(SSI0_BASE, &val);
	}

	static void SpiSelect() {
		ChipSelectPin::Write(0);
	}

	static void SpiDeSelect() {
		ChipSelectPin::Write(1);
	}
};

typedef LedMatrixNS::SPIFrameBuffer<LedConfig> FrameBuffer;

FrameBuffer				frameBuffer;
LedMatrixSimpleFont			defaultFont;
LedMatrix<FrameBuffer>			matrix(frameBuffer, defaultFont);

LedMatrixScrollAnimation<FrameBuffer>	scrollAnimation(defaultFont);
LedMatrixTestAnimation<FrameBuffer>	testAnimation(matrix, scrollAnimation);
PulseAnimation<FrameBuffer>		pulseAnimation;

class MyConnection : public HttpConnection {
public:
	MyConnection(struct tcp_pcb *pcb) 
		: HttpConnection(pcb),
		  handlingUrl(None),
		  state(ReceivingData)
	{}

private:
	void setRequest(char *method, char *path) {
		if( strcmp(path, "/PutRect") == 0 ) {
			handlingUrl = PutRect;
		} else if( strcmp(path, "/Pixel") == 0 ) {
			handlingUrl = Pixel;
		}

		if( strcmp(method, "GET") == 0) {
			requestMethod = Httpd::GET;
		}
	}

	int convert(const char *string)
	{
		int i;
		i=0;
		while(*string)
		{
			i = (i*10) + (*string - '0');
			string++;
		}
		return i;
	}

	void onHeader(char *key, char *val) {
		if( strcmp(key, "X") == 0 ) {
			x = convert(val);
		} else if( strcmp(key, "Y") == 0) {
			y = convert(val);
		} else if( strcmp(key, "Color") == 0) {
			parameters.putPixel.color = convert(val);
		} else if( strcmp(key, "Width") == 0) {
			parameters.putRect.width = convert(val);
		} else if( strcmp(key, "Height") == 0) {
			parameters.putRect.height = convert(val);
		}
	}

	void onHeaderDone() {
		if( handlingUrl == Pixel ) {
			UARTprintf("Drawing %d at (%d,%d)\r\n", parameters.putPixel.color, x, y);
			frameBuffer.putPixel(x, y, parameters.putPixel.color);
		} else if( handlingUrl == PutRect) {
			parameters.putRect.dataCount = parameters.putRect.width * parameters.putRect.height * 2;
			UARTprintf("Drawing from (%d,%d) -> (%d, %d)\r\n", x, y, 
					x + parameters.putRect.width,
					y + parameters.putRect.height);
			UARTprintf("Expecting %d bytes\r\n", parameters.putRect.dataCount);
		}

		if( handlingUrl != PutRect ) {
			const char *response = MyConnection::ResponseNotFound;

			if( handlingUrl != None ) {
				response = MyConnection::ResponseOk;
			}

			if( sendData(response, strlen(response)) == ERR_OK ) {
				state = SendingData;
			} else {
				delete this;
			}
		}
	}

	err_t onSent(uint16_t len) {
		if( state == SendingData) {
			delete this;
		}
		return ERR_OK;
	}

	void onBody(char *data, uint16_t len) {
		if( len > parameters.putRect.dataCount ) {
			parameters.putRect.dataCount = 0;
		} else {
			parameters.putRect.dataCount -= len;
		}
		UARTprintf("dataCount: %d\n", parameters.putRect.dataCount);
		if( parameters.putRect.dataCount == 0 ) {
			if( sendData(MyConnection::ResponseOk, strlen(MyConnection::ResponseOk)) == ERR_OK ) {
				state = SendingData;
			} else {
				delete this;
			}
		}
	}

private:
	typedef enum {
		None,
		PutRect,
		Pixel
	} URL;

	typedef enum {
		ReceivingData,
		SendingData,
	} State;

	URL 			handlingUrl;
	Httpd::RequestMethod 	requestMethod;
	State			state;

	uint16_t		x, y;
	union {
		struct {
			uint16_t color;
		} putPixel;

		struct {
			uint16_t width, height;
			uint32_t dataCount;
		} putRect;
	} parameters;

	static const char	ResponseOk[];
	static const char	ResponseNotFound[];
};

const char MyConnection::ResponseOk[] = "HTTP/1.1 200 OK\r\n";
const char MyConnection::ResponseNotFound[] = "HTTP/1.1 404 Not Found\r\n";

class MyWebServer : public Httpd {
private:
	HttpConnection *createConnection(struct tcp_pcb *pcb) {
		return new MyConnection(pcb);
	}
};

MyWebServer			httpd;

int
main(void) {
	struct netif netif;

	cpu_init();
	uart_init();
	spi_init();

	UARTprintf("Startup\n");

	MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
	MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);
	MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOC);
	MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD);
	MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
	MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);

	MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);

	// Setup LEDs
	MAP_GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE, GPIO_PIN_1);
	MAP_GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE, GPIO_PIN_2);
	MAP_GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE, GPIO_PIN_3);


	// Setup SysTick timer
	MAP_SysTickPeriodSet(MAP_SysCtlClockGet() / SYSTICKHZ);
	MAP_SysTickEnable();
	MAP_SysTickIntEnable();

	// Configure timer 
	MAP_TimerConfigure(TIMER0_BASE, TIMER_CFG_A_PERIODIC | TIMER_CFG_B_PERIODIC | TIMER_CFG_SPLIT_PAIR);
	//MAP_TimerLoadSet(TIMER0_BASE, TIMER_A, 1500);
	MAP_TimerLoadSet(TIMER0_BASE, TIMER_A, 65535);
	MAP_TimerLoadSet(TIMER0_BASE, TIMER_B, 15000);//ROM_SysCtlClockGet());
	MAP_TimerPrescaleSet(TIMER0_BASE, TIMER_A, 50);

	MAP_IntEnable(INT_TIMER0A);
	MAP_IntEnable(INT_TIMER0B);

	MAP_TimerIntEnable(TIMER0_BASE, TIMER_TIMA_TIMEOUT | TIMER_TIMB_TIMEOUT);
	MAP_TimerEnable(TIMER0_BASE, TIMER_BOTH);

	MAP_GPIOIntTypeSet(GPIO_PORTE_BASE, ENC_INT, GPIO_FALLING_EDGE);
	MAP_GPIOPinIntClear(GPIO_PORTE_BASE, ENC_INT);
	MAP_GPIOPinIntEnable(GPIO_PORTE_BASE, ENC_INT);

	MAP_IntMasterEnable();

	FAST_GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1, 0);

	frameBuffer.init();

	//MAP_GPIOPinTypeGPIOOutput(GPIO_PORTA_BASE, GPIO_PIN_3);
	ChipSelectPin::ConfigureDirection(MCU::GPIO::Output);
	ChipSelectPin::Write(1);

	LedMatrixColor color(0, 32, 0);
	LedMatrixColor redColor(32, 0, 0);
	frameBuffer.clear();

	char *msg = "#0020Hello #2000World ";

	scrollAnimation.setMessage(msg, strlen(msg));
	//matrix.setAnimation(&testAnimation, 3);
	//frameBuffer.putPixel(3,3, redColor);
	
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

	httpd.init();

        UARTprintf("Welcome\n");

	unsigned long last_arp_time, last_tcp_time, last_dhcp_coarse_time,
		      last_dhcp_fine_time, last_change;

	last_change = last_arp_time = last_tcp_time = last_dhcp_coarse_time = last_dhcp_fine_time = 0;

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
#if 1
		if(HWREGBITW(&events, FLAG_UPDATE) == 1) {
			HWREGBITW(&events, FLAG_UPDATE) = 0;
			//displayAnimTick();
			matrix.update();
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

		}
		
		if( HWREGBITW(&events, FLAG_ENC_INT) == 1 ) {
			HWREGBITW(&events, FLAG_ENC_INT) = 0;
			enc_action(&netif);
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



volatile static unsigned long oldTickCounter;
volatile static uint16_t t = 0;
#ifdef __cplusplus
extern "C" {
#endif
void timer0_int_handler(void) {
	MAP_TimerIntClear(TIMER0_BASE, TIMER_TIMA_TIMEOUT);
	matrix.update();
	t++;
	//while(1) {}
	//HWREGBITW(&events, FLAG_UPDATE) = 1;
	//displayTick();
	//if( tickCounter > oldTickCounter + 2) {
		//oldTickCounter = tickCounter;
	//}
}

void timer0b_int_handler(void) {
	MAP_TimerIntClear(TIMER0_BASE, TIMER_TIMB_TIMEOUT);

	/*if( displayCheckUpdate() ) {
		HWREGBITW(&events, FLAG_UPDATE) = 1;
	}*/
}

void SysTickIntHandler(void) {
	tickCounter++;
	HWREGBITW(&events, FLAG_SYSTICK) = 1;
}
#ifdef __cplusplus
}
#endif

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

#ifdef __cplusplus
extern "C" {
#endif
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
#ifdef __cplusplus
}
#endif

