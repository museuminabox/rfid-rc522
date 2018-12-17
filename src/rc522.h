/*
 *  
 * Header file to support routines to access the RC522
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * The program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>. 
 * 
 * Created on: 14.08.2013
 *      authorizer: alexs
 *
 * 
 * version 1.51 / paulvh / July 2017
 * Fixed number of bugs and code clean-up.
 */

//#include "main.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// color display enable
#define RED     1
#define GREEN   2
#define YELLOW  3
#define BLUE    4
#define WHITE   5

#define REDSTR "\e[1;31m%s\e[00m"
#define GRNSTR "\e[1;92m%s\e[00m"
#define YLWSTR "\e[1;93m%s\e[00m"
#define BLUSTR "\e[1;34m%s\e[00m"

/* print message in color */
void p_printf (int level, char *format, ...);

// the registers that are RFU (reserved for future use) have been left out.

//MF522 commands for the CommandReg
#define PCD_IDLE              0x00		// no action, cancels current command execution
#define PCD_AUTHENT           0x0E		// perform the MIFARE standard authentication as a reader
#define PCD_RECEIVE           0x08		// activate the receiver circuits
#define PCD_TRANSMIT          0x04		// transmits data from the FIFO buffer
#define PCD_TRANSCEIVE        0x0C		// transmits data from the FIFO buffer & activate receive after transmission
#define PCD_RESETPHASE        0x0F		// softreset command 
#define PCD_CALCCRC           0x03		// activates the CRC co-processor or perform a selftest

//Mifare_One basic function library
#define PICC_REQIDL           0x26      // REQA (idle card will respond)
#define PICC_REQALL           0x52      // WUPA (All cards will respond IDLE and Halt)
#define PICC_ANTICOLL1        0x93      // cascade level 1
#define PICC_ANTICOLL2        0x95      // cascade level 2
#define PICC_ANTICOLL3        0x97      // cascade level 3
#define PICC_AUTHENT1A        0x60      // authentication with KeyA
#define PICC_AUTHENT1B        0x61      // authentication with keyb
#define PICC_READ             0x30		// read 16 byte datablock
#define PICC_WRITE            0xA0		// write 16 byte datablock
#define PICC_UL_WRITE         0xA2		// write 4 byte page to Ultralight class cards
#define PICC_DECREMENT        0xC0      // decrement value block
#define PICC_INCREMENT        0xC1      // increment value block
#define PICC_RESTORE          0xC2      // move value block to internal data register
#define PICC_TRANSFER         0xB0      // move internal data-reg to value block
#define PICC_HALT             0x50      // + 00

//MF522 FIFO
#define DEF_FIFO_LENGTH       64        //FIFO size=64byte
#define MAXRLEN  			  18

/** MF522 registers */
// command and status
#define     CommandReg            0x01	// starts and stops command execution
#define     ComIEnReg             0x02	// enable and disable interrupt request control bits
#define     DivlEnReg             0x03	// enable and disable interrupt request control bits
#define     ComIrqReg             0x04	// interrupt request bits
#define     DivIrqReg             0x05	// interrupt request bits
#define     ErrorReg              0x06	// error bits showing the error status of last command
#define     Status1Reg            0x07	// communication status bits
#define     Status2Reg            0x08	// receiver and transmitter status bits
#define     FIFODataReg           0x09	// input and output of 64 byte FIFO buffer
#define     FIFOLevelReg          0x0A	// number of bytes stored in the FIFO buffer
#define     WaterLevelReg         0x0B	// level for FIFO underflow and overflow warning
#define     ControlReg            0x0C	// miscellaneus control reigsters
#define     BitFramingReg         0x0D	// adjustments for bit-oriented frames
#define     CollReg               0x0E	// bit position of the first bit-collision detected on RF int.
// command
#define     ModeReg               0x11	// defines general modes for transmitting and receiving
#define     TxModeReg             0x12	// defines transmission data rate and framing
#define     RxModeReg             0x13	// defines reception data rate and framing
#define     TxControlReg          0x14	// controls the logical behavior of the antenna driver TX1/ TX2
#define     TxASKReg              0x15	// controls the setting of the transmission modulation
#define     TxSelReg              0x16	// selects the internal sources for the antenna driver
#define     RxSelReg              0x17	// selects the internal receiver settings
#define     RxThresholdReg        0x18	// selects thresholds for the bit decoder
#define     DemodReg              0x19	// defines demodulator settings
#define     MiTXReg               0x1C	// controls some MIFARE communication transmit parameters
#define     MiRXReg               0x1D	// controls some MIFARE communication receive parameters
#define     SerialSpeedReg        0x1F	// selects the speed for the serial UART interface
// configuration
#define     CRCResultRegM         0x21	// shows the MSB value for the CRC calculation
#define     CRCResultRegL         0x22	// shows the LSB value for the CRC calculation
#define     ModWidthReg           0x24	// controls the ModWidth settings
#define     RFCfgReg              0x26	// configures the receiver gain
#define     GsNReg                0x27	// selects the conductance of the antenna on TX1/TX2 modulation
#define     CWGsCfgReg            0x28	// defines the conductance of the p-driver output
#define     ModGsCfgReg           0x29  // defines the conductance of the p-driver output
#define     TModeReg              0x2A  // defines settings for the internal timer
#define     TPrescalerReg         0x2B  // defines settings for the internal timer
#define     TReloadRegH           0x2C  // defines the 16 bit timer reload value
#define     TReloadRegL           0x2D  // defines the 16 bit timer reload value
#define     TCounterValueRegH     0x2E  // shows the 16 bit timer reload value
#define     TCounterValueRegL     0x2F  // shows the 16 bit timer reload value
// test register
#define     TestSel1Reg           0x31  // general test signal configuration
#define     TestSel2Reg           0x32  // general test signal configuration and PRBS control	
#define     TestPinEnReg          0x33	// enables pin output driver on pins D1 to D7
#define     TestPinValueReg       0x34	// defines values for D1 to D7 when used as I/O bus
#define     TestBusReg            0x35	// shows the status of the internal test bus
#define     AutoTestReg           0x36	// controls the digital self test
#define     VersionReg            0x37	// shows the software version
#define     AnalogTestReg         0x38	// controls the pins AUX1 and AUX2
#define     TestDAC1Reg           0x39	// defines the test value for testDAC1
#define     TestDAC2Reg           0x3A  // defines the test value for testDAC2
#define     TestADCReg            0x3B	// shows the value of ADC I and Q channels

/////////////////////////////////////////////////////////////////////
//MF522 status
/////////////////////////////////////////////////////////////////////
#define     TAG_OK                (0)
#define     TAG_NOTAG             (1)
#define     TAG_ERR               (2)
#define     TAG_ERRCRC            (3)
#define     TAG_COLLISION         (4)
typedef char tag_stat;

/////////////////////////////////////////////////////////////////////
//MF522 command routines
/////////////////////////////////////////////////////////////////////

// perform init
void InitRc522(void);
char PcdReset(void);

// set and clear bits in registers
void SetBitMask(uint8_t   reg,uint8_t   mask);
void ClearBitMask(uint8_t   reg,uint8_t   mask);

// read and write a register
uint8_t WriteRawRC(uint8_t   Address, uint8_t   value);
uint8_t ReadRawRC(uint8_t   Address);

// switch antenna on or off
void PcdAntennaOn(void);
void PcdAntennaOff(void);

// stop RC522 from sending in Crypto-1 encrypted communication
void Pcd_stopcrypto1();

/* check whether already authorized */
int PcdCheckAuth();

/////////////////////////////////////////////////////////////////////
//card supporting routines
/////////////////////////////////////////////////////////////////////

// enable extra delay between card access calls
extern int extra_delay;

/* Decrement, Increment or restore a value block */
char PcdValue(uint8_t act, uint8_t  addr, double value);

// perform request to start card
char PcdRequest(unsigned char req_code,unsigned char *pTagType);

// communicate with card
char PcdComMF522(uint8_t   Command,
                 uint8_t *pIn ,
                 uint8_t   InLenByte,
                 uint8_t *pOut ,
                 uint8_t  *pOutLenBit,
                 int	  CheckTimeout);
                 
void CalulateCRC(uint8_t *pIn ,uint8_t   len,uint8_t *pOut );

// get the UID and select the right card
char PcdAnticoll(uint8_t , uint8_t *);
char PcdSelect(uint8_t , uint8_t *, uint8_t *);

// authorise access to a block and set Crypto-1 encrypted communication
char PcdAuthState(unsigned char auth_mode,unsigned char addr,unsigned char *pKey,unsigned char *pSnr);

// read and write card block
char PcdWrite(unsigned char addr,unsigned char *pData);
char PcdRead(unsigned char addr,unsigned char *pData);

// special write for Mifare Ultralight class of cards (inc. NTAG2xx)
// Writes a single page - 4 bytes
char PcdWritePage(unsigned char addr,unsigned char *pData);

// put current card on hold
char PcdHalt(void);

/////////////////////////////////////////////////////////////////////
//Global variables
/////////////////////////////////////////////////////////////////////

extern int extra_delay;			// set extra delay on card access calls

#ifdef __cplusplus
}
#endif
