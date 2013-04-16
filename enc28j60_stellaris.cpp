#include "enc28j60_stellaris.h"
#include "enc28j60.h"

#include <netif/etharp.h>
#include <string.h>

err_t
StellarisENC28J60::lwip_init(struct netif *netif) {
	ENCJ_STELLARIS::ENC28J60 *driver = (ENCJ_STELLARIS::ENC28J60*)netif->state;

	netif->hwaddr_len = 6;
	netif->name[0] = 'e';
	netif->name[1] = 'n';
	netif->output = etharp_output;
	netif->linkoutput = StellarisENC28J60::lwip_link_out;
	netif->mtu = 1500;
	netif->flags = NETIF_FLAG_ETHARP;
#ifdef LWIP_NETIF_STATUS_CALLBACK
	netif->status_callback = StellarisENC28J60::lwip_status_callback;
#endif

	//memcpy(netif->hwaddr, mac_addr, 6);
	driver->GetMACAddress(netif->hwaddr);

	return ERR_OK;
}

err_t
StellarisENC28J60::lwip_link_out(struct netif *netif, struct pbuf *p) {
	ENCJ_STELLARIS::ENC28J60 *driver = (ENCJ_STELLARIS::ENC28J60*)netif->state;

	uint8_t frame[1514];
	uint8_t *frame_ptr = &frame[0];
	struct pbuf *b;

	for(b = p; b != NULL; b = b->next) {
		//printf("Copying %d bytes from %p to %p\n", b->len, b->payload, frame_ptr);
		memcpy(frame_ptr, b->payload, b->len);
		frame_ptr += b->len;
	}

	driver->Send(frame, p->tot_len);

	return ERR_OK;
}

void
StellarisENC28J60::lwip_status_callback(struct netif *netif) {
	if( netif->flags & NETIF_FLAG_UP ) {
		printf("IP: %d.%d.%d.%d\n",
				ip4_addr1_16(&netif->ip_addr),
				ip4_addr2_16(&netif->ip_addr),
				ip4_addr3_16(&netif->ip_addr),
				ip4_addr4_16(&netif->ip_addr) );
	}
}


void
ENCJ_STELLARIS::BusDriver::ChipSelect(ENCJ_STELLARIS::ENC28J60 *driver) {
	MAP_GPIOPinWrite(StellarisENC28J60Configuration::PIN_CHIP_SELECT_BASE, StellarisENC28J60Configuration::PIN_CHIP_SELECT, 0);
}

void
ENCJ_STELLARIS::BusDriver::ChipDeSelect(ENCJ_STELLARIS::ENC28J60 *driver) {
	MAP_GPIOPinWrite(StellarisENC28J60Configuration::PIN_CHIP_SELECT_BASE, StellarisENC28J60Configuration::PIN_CHIP_SELECT, StellarisENC28J60Configuration::PIN_CHIP_SELECT);
}

uint8_t
ENCJ_STELLARIS::BusDriver::SpiSend(ENCJ_STELLARIS::ENC28J60 *driver, uint8_t msg) {
	unsigned long val;
	MAP_SSIDataPut(StellarisENC28J60Configuration::SPI_SSI_BASE, msg);
	MAP_SSIDataGet(StellarisENC28J60Configuration::SPI_SSI_BASE, &val);
	return (uint8_t)val;
}

void
ENCJ_STELLARIS::BusDriver::PinSet(ENCJ_STELLARIS::ENC28J60 *driver, ENCJ_STELLARIS::PinType pin, ENCJ_STELLARIS::PinValue value) {
}

void
ENCJ_STELLARIS::BusDriver::Init(ENCJ_STELLARIS::ENC28J60 *driver) {
	// Configure SSI2 for ENC28J60 usage
	// GPIO_PB4 is CLK
	// GPIO_PB6 is RX
	// GPIO_PB7 is TX
	MAP_SysCtlPeripheralEnable(StellarisENC28J60Configuration::SPI_PORT_PERIPHERAL);
	MAP_SysCtlPeripheralEnable(StellarisENC28J60Configuration::SPI_SSI_PERIPHERAL);
	MAP_GPIOPinConfigure(StellarisENC28J60Configuration::SPI_SSI_CLK_CONF);
	MAP_GPIOPinConfigure(StellarisENC28J60Configuration::SPI_SSI_RX_CONF);
	MAP_GPIOPinConfigure(StellarisENC28J60Configuration::SPI_SSI_TX_CONF);
	MAP_GPIOPinTypeSSI(StellarisENC28J60Configuration::SPI_PORT_BASE, StellarisENC28J60Configuration::SPI_SSI_CLK_PIN | StellarisENC28J60Configuration::SPI_SSI_TX_PIN | StellarisENC28J60Configuration::SPI_SSI_RX_PIN);
	MAP_SSIConfigSetExpClk(StellarisENC28J60Configuration::SPI_SSI_BASE, MAP_SysCtlClockGet(), SSI_FRF_MOTO_MODE_0,
			SSI_MODE_MASTER, 1000000, 8);
	MAP_SSIEnable(StellarisENC28J60Configuration::SPI_SSI_BASE);

	unsigned long b;
	while(MAP_SSIDataGetNonBlocking(StellarisENC28J60Configuration::SPI_SSI_BASE, &b)) {}

	MAP_SysCtlPeripheralEnable(StellarisENC28J60Configuration::PIN_CHIP_SELECT_PERIPH);
	MAP_SysCtlPeripheralEnable(StellarisENC28J60Configuration::PIN_INT_PERIPH);
	MAP_GPIOPinTypeGPIOOutput(StellarisENC28J60Configuration::PIN_CHIP_SELECT_BASE, StellarisENC28J60Configuration::PIN_CHIP_SELECT);

	MAP_GPIOPinTypeGPIOInput(StellarisENC28J60Configuration::PIN_INT_BASE, StellarisENC28J60Configuration::PIN_INT);

	MAP_GPIOPinWrite(StellarisENC28J60Configuration::PIN_CHIP_SELECT_BASE, StellarisENC28J60Configuration::PIN_CHIP_SELECT, StellarisENC28J60Configuration::PIN_CHIP_SELECT);
	MAP_IntEnable(StellarisENC28J60Configuration::PIN_INT_INT);

	MAP_GPIOIntTypeSet(StellarisENC28J60Configuration::PIN_INT_BASE, StellarisENC28J60Configuration::PIN_INT, GPIO_FALLING_EDGE);
	MAP_GPIOPinIntClear(StellarisENC28J60Configuration::PIN_INT_BASE, StellarisENC28J60Configuration::PIN_INT);
	MAP_GPIOPinIntEnable(StellarisENC28J60Configuration::PIN_INT_BASE, StellarisENC28J60Configuration::PIN_INT);
}

void
ENCJ_STELLARIS::BusDriver::Delay(uint32_t ms) {
	MAP_SysCtlDelay(((MAP_SysCtlClockGet()/3)/1000)*ms);
}

void
ENCJ_STELLARIS::BusDriver::OnReceive(ENCJ_STELLARIS::ENC28J60 *driver, uint16_t data_count) {
	struct pbuf *p, *q;
	uint8_t frame[1514];
	struct netif *netif;
	uint8_t *frame_ptr = &frame[0];

	netif = (struct netif *)driver->GetUserData();
	driver->RBM(frame, data_count);

	p = pbuf_alloc(PBUF_LINK, data_count, PBUF_POOL);
	if( p != NULL ) {
		for(q = p; q != NULL; q = q->next) {
			memcpy(q->payload, frame_ptr, q->len);
			frame_ptr += q->len;
		}

		netif->input(p, netif);
	}
}
