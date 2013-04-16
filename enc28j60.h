/**
 * @file enc28j60.h
 *
 * Prototype for ENC28J60 controller class. For use with the Stellaris
 * Launchpad, Stellaris-Pins library and a compatible ENC28J60 board.
 * 
 * It is possible to use this library without the Stellaris-Pins library, but
 * you get to write out the additional pin information yourself.
 */

#ifndef ENC28J60_STELLARIS_CHAPMAN_H_
#define ENC28J60_STELLARIS_CHAPMAN_H_

#include <stdint.h>
#include "enc28j60reg.h"

/* Last thing we need to get rid of: UARTprintf */
extern "C" void UARTprintf(const char *pcString, ...);
#define printf UARTprintf

namespace ENCJ_STELLARIS
{
	typedef enum {
		Reset
	} PinType;

	typedef enum {
		Low = 0,
		High = 1
	} PinValue;

	class ENC28J60;

	/** Abstract which acts both as HAL and IP abstraction layer.
	 *
	 * The basic idea is that the BusDriver provides the necessary methods for
         * both hardware setup and communication (SPI, GPIO), communication with
	 * the IP-stack, and basic utility methods (Delay, in this case).
	 *
	 * This class has no implementation. Users of the ENC28J60 driver must
	 * provide implementations of the methods.
	 * All the methods are static. For most methods a pointer to the ENC28J60-driver
	 * instance is provided, which allows the Bus Driver to deal with multiple driver
	 * instances and perform calls back to the ENC28J60 driver, if needed.
	 *
	 * The term "bus" is used to describe the three communication means towards the
	 * ENC28J60: The SPI-bus, the chip select line, and the reset line.
	 * The bus also includes the communication path towards the IP-stack. This is
         * dealt with by the OnReceive()-method.
	 */
	class BusDriver
	{
	public:
		/** Initialize the "bus".
		 *
	  	 * The underlying SPI and GPIO channels need to be initialized.
		 * In order to be compatible with the ENC28J60 driver, the SPI-bus must
		 * be initialized to 8-bit transfer.
		 *
	         * @param[in]	driver	Pointer to ENC28J60 driver calling.
		 */
		static void Init(ENC28J60 *driver);

		/** Select the ENC28J60 on the SPI-bus.
		 *
	         * @param[in]	driver	Pointer to ENC28J60 driver calling.
		 */
		static void ChipSelect(ENC28J60 *driver);

		/** De-select the ENC28J60 on the SPI-bus.
		 *
	         * @param[in]	driver	Pointer to ENC28J60 driver calling.
		 */
		static void ChipDeSelect(ENC28J60 *driver);


		/** Transmit and receive a single byte.
		 *
	         * @param[in]	driver	Pointer to ENC28J60 driver calling.
		 * @param[in]	msg	Byte to transmit.
		 * @return	Byte received.
		 */
		static uint8_t SpiSend(ENC28J60 *driver, uint8_t msg);

		/** Set a given GPIO-pin.
		 * 
	         * @param[in]	driver	Pointer to ENC28J60 driver calling.
		 * @param[in]	pin	The pin to set.
		 * @param[in]	value	The value to set (high/low).
		 */
		static void PinSet(ENC28J60 *driver, PinType pin, PinValue value);

		/** Delay for a number of miliseconds.
		 * 
	         * @param[in]	ms	Miliseconds to delay.
		 */
		static void Delay(uint32_t ms);

		
		/** Send data to the IP-stack.
		 *
		 * Called by the ENC28J60-driver when it receives data that needs to be
		 * passed on to the IP-stack.
		 * The implementer of this method must use ENC28J60::RBM(uint8_t,uint16_t) to read
		 * exactly @p data_count bytes.
		 * 
	         * @param[in]	driver		Pointer to ENC28J60 driver calling.
	         * @param[in]	data_count	Number of bytes available.
		 */
		static void OnReceive(ENC28J60 *driver, uint16_t data_count);
	};

	/** ENC28J60 driver
	 *
	 * Driver to control one ENC28J60. It uses the static methods in BusDriver to
	 * communicate with the ENC28J60.
	 *
	 * The driver performs no initialization in the constructor, the Init() methods is used for that.
	 * The reason for this is to allow ENC28J60 instances to be globally declared without
	 * running any code before the main-method has been reached.
	 */
	class ENC28J60
	{
	public:
		/** Initialize the ENC28J60.
		 *
		 * @param[in]	mac	The MAC addres to initialize the ENC28J60 with.
		 */
		void Init(const uint8_t *mac);

		/** Initialize the ENC28J60.
		 *
		 * @param[in]	mac		The MAC addres to initialize the ENC28J60 with.
		 * @param[in]	userData	User data pointer.
		 */
		void Init(const uint8_t *mac, void *userData);

		/** Request the ENC28J60 to transmit an ethernet frame.
		 *
		 * @param[in]	buf	Buffer containing frame to transmit.
		 * @param[in]	count	Size of the buffer.
		 * @return		True, if the transmission was successull. False, otherwise.
  		 */
		bool Send(const uint8_t *buf, uint16_t count);

		/** Soft-reset the ENC28J60.
		 */
		void Reset(void);

		/** Handle an interrupt signaled by the ENC28J60.
		 * 
		 */
		void Interrupt(void);

		/** Read from the ENC28J60 buffer memory.
		 *
		 * This methods should generally only be called from BusDriver::OnReceive(),
		 * and the exact number of bytes reported to BusDriver::OnReceive() should be read.
		 * However, they may be read with multiple calls to this method.
		 *
		 * Calling this method in any other context, or failing to read the correct number
		 * of bytes, might corrupt the buffer memory of the ENC28J60.
		 *
		 * @param[out]	buf	Buffer to fill with content.
		 * @param[in]	len	Number of bytes to read from the buffer memory.
  		 */
		void RBM(uint8_t *buf, uint16_t len);	// Read Buffer Memory

		void GetMACAddress(uint8_t *macAddr);

		void *GetUserData();

	private:
		/** Private Methods **/

		/* Setup */
		void InitConfig(const uint8_t *mac);

		uint8_t SPISend(uint8_t msg);

		void Receive(void);

		/* Low level register controls */

		uint8_t RCR(uint8_t reg);				// Read Control Register
		uint8_t RCRM(uint8_t reg);				// Read Control Register MAC/MII
		void WCR(uint8_t reg, uint8_t val);		// Write Control Register
		void WBM(const uint8_t *buf, uint16_t len);	// Write Buffer Memory

		void BFS(uint8_t reg, uint8_t mask);	// Bit Field Set
		void BFC(uint8_t reg, uint8_t mask);	// Bit Field Clear

		/* High level register controls */

		void SwitchBank(uint8_t new_bank);	// Switch active memory bank

		uint8_t ReadRegister(uint8_t reg, uint8_t bank);
		uint8_t ReadMIIRegister(uint8_t reg, uint8_t bank);
		void WriteRegister(uint8_t reg, uint8_t bank, uint8_t value);

		// Batch bit field set controls
		void BitsFieldSet(uint8_t reg, uint8_t bank, uint8_t mask);
		void BitsFieldClear(uint8_t reg, uint8_t bank, uint8_t mask);

		// PHY memory access
		uint16_t ReadPHY(uint8_t address);
		void WritePHY(uint8_t address, uint16_t value);

		// Mass memory operations
		void SetRXMemoryArea(uint16_t startAddr, uint16_t endAddr);
		void SetMACAddress(const uint8_t *macAddr);

	private:
		/* Private fields */
		uint8_t		activeBank;	// Current memory bank
		uint16_t	nextPacket;	// Next data packet ptr (to internal ENC28J60 memory)
		void		*userData;
	};


} // Namespace ENCJ_STELLARIS

#endif // ENC28J60_STELLARIS_CHAPMAN_H_
