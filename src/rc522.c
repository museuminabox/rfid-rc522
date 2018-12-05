/*
 * 
 * contains routines to access the RC522
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
 * 
 * Created on: 14.08.2013
 *      authorizer: alexs
 * 
 * version 1.50 / paulvh / July 2017
 * Fixed number of bugs and code clean-up.
 */

#include "rc522.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <bcm2835.h>

int     NoColor = 0;        // disables color output (if set)
/* Display in color
 * @param format : Message to display and optional arguments
 *                same as printf
 * @param level :  1 = RED, 2 = GREEN, 3 = YELLOW 4 = BLUE 5 = WHITE
 * 
 *  if NoColor was set, output is always WHITE.
 */
void p_printf (int level, char *format, ...)
{
    char        *col;
    int         coll=level;
    va_list arg;

    //allocate memory
    col = malloc(strlen(format) + 20);
    if (NoColor) coll = WHITE;

    switch(coll)
    {
        case RED:
            sprintf(col,REDSTR, format);
            break;
        case GREEN:
            sprintf(col,GRNSTR, format);
            break;
        case YELLOW:
            sprintf(col,YLWSTR, format);
            break;
        case BLUE:
            sprintf(col,BLUSTR, format);
            break;
        default:
            sprintf(col,"%s",format);
    }

    va_start (arg, format);
    vfprintf (stdout, col, arg);
    va_end (arg);
    fflush(stdout);
    // release memory
    free(col);
}

int extra_delay=0;			// set extra delay on card access calls

// perform init
void InitRc522(void)
{
    PcdReset();
}

// start of communication with a card
char PcdRequest(uint8_t req_code,uint8_t *pTagType)
{
    char   status;
    uint8_t   unLen;
    uint8_t   ucComMF522Buf[MAXRLEN];
    
    /*
     * B7       1 = start transmission of data (with transceive command)
     * B6 - B4  0 = LSB stored in position 0
     * B3       not used
     * B2 - B0  number of bits of last byte that will be transmitted
     */

    WriteRawRC(BitFramingReg,0x07);

    ucComMF522Buf[0] = req_code;

    /* write request with length 1, receive in buf and unlen = #bits received*/
    status = PcdComMF522(PCD_TRANSCEIVE,ucComMF522Buf,1,ucComMF522Buf,&unLen,1);

	/* expect 2 bytes or 16 bits with card type*/
    if ((status == TAG_OK) && (unLen == 0x10))
    {
        *pTagType     = ucComMF522Buf[0];
        *(pTagType+1) = ucComMF522Buf[1];
    }
    else if (status == TAG_COLLISION) {
//      printf("ATQA %02x%02x\n",ucComMF522Buf[0],ucComMF522Buf[1]);
    }
    else if (status != TAG_NOTAG) status = TAG_ERR;

    return status;
}

/* Perform anticollision to select a single card and get it's UID
 * sequence:
 *
 * 0x93  0x20   unique identifier is sent by card
 * 0x93  0x70 UID + 2 CRC to select te card (pcdselect handled))
 * 0x95 = cascade level 2 ( 7 bytes)
 * 0x97 = cascade level 3 ( 10 bytes)
 */
char PcdAnticoll(uint8_t cascade, uint8_t *pSnr)
{
    char   status;
    uint8_t   i,snr_check=0;
    uint8_t   unLen;
    uint8_t   ucComMF522Buf[MAXRLEN];
    uint8_t   pass=32;      // max 32 bits (ISo standard)
    uint8_t   collbits=0;

    i=0;
  
    /* 
     * LSB of the received bit is stored at bit position 0, the second on stored on ps 1
     * all bits of the last byte will be transferred */
    WriteRawRC(BitFramingReg,0x00);

	// bits received after collision are cleared
	WriteRawRC(CollReg,0x80);
	
    do {
        /* anti collision commands 0x93, 0x95 or 0x97 */
        ucComMF522Buf[0] = cascade;

        /* NVB = number of valid bits/bytes
         * 
         * NVB : 0x20 = no part of UID is sent = anticollision command
         * 
         * 0x20 => top 4 bits is byte count = 2 valid bytes to transmit: cascade (or SEL or CL) + NVB
         * byte count 2 = minimum, 7 max
         * 
         * 0x20 => bottom 4 bits is bit count = 0 : number of valid data bits (include SEL+NVB) modulo 8
         *     
         * normal way to connect at first, second the UID will be asked
         * with collbits set to the correct number of UID bits
         */
        ucComMF522Buf[1] = 0x20+collbits;

        //  WriteRawRC(0x0e,0);

        /* write Transceive command to card*/
        status = PcdComMF522(PCD_TRANSCEIVE,ucComMF522Buf,2+i,ucComMF522Buf,&unLen,1);

        if (status == TAG_COLLISION) {

            collbits=ReadRawRC(CollReg)&0x1f;

            /* 00 = bit collision of 32nd bit
             * that sets the length of the UID */
            if (collbits==0) collbits=32;

            i=(collbits-1)/8 +1;
// printf ("--- %02x %02x %02x %02x %d\n",ucComMF522Buf[0],ucComMF522Buf[1],ucComMF522Buf[2],ucComMF522Buf[3],unLen);

            ucComMF522Buf[i-1]|=(1<<((collbits-1)%8));  // move collbit to right place
            ucComMF522Buf[5]=ucComMF522Buf[3];
            ucComMF522Buf[4]=ucComMF522Buf[2];
            ucComMF522Buf[3]=ucComMF522Buf[1];
            ucComMF522Buf[2]=ucComMF522Buf[0];

            WriteRawRC(BitFramingReg,(collbits % 8));
// printf (" %d %d %02x %d\n",collbits,i,ucComMF522Buf[i+1],collbits % 8);
        }
    } while ( --pass > 0 && status == TAG_COLLISION);
// printf ("na %02x %02x %02x %02x %d\n",ucComMF522Buf[0],ucComMF522Buf[1],ucComMF522Buf[2],ucComMF522Buf[3],unLen);

    if (status == TAG_OK)   // 4 bytes as defined by ISO
    {
        for (i=0; i < 4; i++)
        {
            *(pSnr+i)  = ucComMF522Buf[i];
            snr_check ^= ucComMF522Buf[i];
        }
		// check BCC = block check characters match
        if (snr_check != ucComMF522Buf[i]) status = TAG_ERR;    
    }

    return status;
}
/*
 * Select a card based on the UID
 * 
 * cascade :
 * 0x93 cascade level 1
 * 0x95 cascade level 2
 * 0x97 cascade level 3
 *
 * pSnr : store received UID
 * SAK : store the ATS from the SAK
 *  
 * NVB = 7- =  PCD will transmit the UID CLn
 * 
 * SAK = Select acknowledgement
 * byte 1 = answer to select (ATS) to determine the card type
 * byte 2 and 3 are CRC
 *  
 * in case of double or triple UID only the last SAK is valid
 */

char PcdSelect(uint8_t cascade, uint8_t *pSnr, uint8_t *SAK)
{
    char   status;
    uint8_t   i;
    uint8_t   unLen;
    uint8_t   ucComMF522Buf[MAXRLEN];
	uint8_t   CRC_buff[2];
	
    ucComMF522Buf[0] = cascade;     // 0x93, 0x95 or 0x97
    ucComMF522Buf[1] = 0x70;        // 0x70 : SELECT command
    ucComMF522Buf[6] = 0;			// BCC
    
    for (i=0; i<4; i++)             // add UID & calc BCC
    {
        ucComMF522Buf[i+2] = *(pSnr+i);
        ucComMF522Buf[6]  ^= *(pSnr+i); 
    }
                                    // add 2 CRC-A bytes
    CalulateCRC(ucComMF522Buf,7,&ucComMF522Buf[7]);

    /* clear MIFARE Crypto-1  (just in case)*/
    ClearBitMask(Status2Reg,0x08);

    /* sent select command */
    status = PcdComMF522(PCD_TRANSCEIVE,ucComMF522Buf,9,ucComMF522Buf,&unLen,1);
              
    // expects 24 bytes response  (as for ISO)
    if ((status == TAG_OK) && (unLen == 0x18))
    {
        // calculate CRC
		CalulateCRC(ucComMF522Buf,1,CRC_buff);     // 1 byte used for CRC check
		
//  printf("debug %02x%02x %02x%02x   ",ucComMF522Buf[1],ucComMF522Buf[2],CRC_buff[0],CRC_buff[1]);
        
        // check CRC is correct
        if ((CRC_buff[0]!=ucComMF522Buf[1])||(CRC_buff[1]!=ucComMF522Buf[2])) 
			status = TAG_ERRCRC;
			
		else
		{
			*SAK = ucComMF522Buf[0];	// copy SAK
			status = TAG_OK; 
		}
    }
    else
       status = TAG_ERR;    

    return status;
}

/* Authorise access to a block address by the RC522
 * 
 * 0x60 key A /0x61 Key B
 *  
 * Format :
 * Authentication command ( 0x60 or 0x61)
 * Block address
 * Sector key byte 0
 * Sector key byte 1
 * Sector key byte 2
 * Sector key byte 3
 * Sector key byte 4
 * Sector key byte 5
 * Sector key byte 6
 * Card serial number byte 0
 * Card serial number byte 1
 * Card serial number byte 2
 * Card serial number byte 3
 * 
 * 12 bytes in total
 * 
 */
char PcdAuthState(uint8_t   auth_mode,uint8_t   addr,uint8_t *pKey,uint8_t *pSnr)
{
    char   status;
    uint8_t   unLen;
    uint8_t   ucComMF522Buf[MAXRLEN];
    
    memset(ucComMF522Buf,0,sizeof(ucComMF522Buf));  // clear buffer

    ucComMF522Buf[0] = auth_mode;
    ucComMF522Buf[1] = addr;
    
    memcpy(&ucComMF522Buf[2], pKey, 6);		// included Key A or KeyB
    memcpy(&ucComMF522Buf[8], pSnr, 4);		// include only first 4 bytes of UID
    
    status = PcdComMF522(PCD_AUTHENT,ucComMF522Buf,12,ucComMF522Buf,&unLen,1);
   
    // check that Crypt0-1 bit is set to indicate succesfull authorization
    if ((status != TAG_OK) || (!(ReadRawRC(Status2Reg) & 0x08)))
		status = TAG_ERR;   

    return(status);
}

/* peform reading a block from the card
 * addr = block number (must have been authorised for access before
 * p  = buffer to store 16 bytes from the block
 */
char PcdRead(uint8_t addr,uint8_t *p )
{
    char   status;
    uint8_t   unLen;
    uint8_t   i,ucComMF522Buf[MAXRLEN];
    uint8_t   CRC_buff[2];

    memset(ucComMF522Buf,0,sizeof(ucComMF522Buf));  // clear buffer
    
    ucComMF522Buf[0] = PICC_READ;               	// 0x30 = read
    ucComMF522Buf[1] = addr;            			// add address to read
    CalulateCRC(ucComMF522Buf,2,&ucComMF522Buf[2]); // add crc

	// perform read operation from card
    status = PcdComMF522(PCD_TRANSCEIVE,ucComMF522Buf,4,ucComMF522Buf,&unLen,1);

	// check status and received bits / bytes 0x90 => 144 / 8 => 18 bytes => 16 data bytes + 2 CRC
    if ((status == TAG_OK) && (unLen == 0x90))
    {
        // calculate CRC
		CalulateCRC(ucComMF522Buf,16,CRC_buff);     // 16 bytes should used for CRC check
		
//  printf("debug %02x%02x %02x%02x   ",ucComMF522Buf[16],ucComMF522Buf[17],CRC_buff[0],CRC_buff[1]);
        
        // check CRC is correct
        if ((CRC_buff[0]!=ucComMF522Buf[16])||(CRC_buff[1]!=ucComMF522Buf[17])) 
			status = TAG_ERRCRC;
			
        else
        {
			// copy 16 data bytes received
			for (i=0; i<16; i++)   *(p+i) = ucComMF522Buf[i];   
		}
    }
    else
		status = TAG_ERR;  

    return(status);
}
/* write block ( 16 bytes) to specific address */
char PcdWrite(uint8_t   addr, uint8_t *p )
{
    char   status;
    uint8_t   unLen;
    uint8_t   i,ucComMF522Buf[MAXRLEN];

    ucComMF522Buf[0] = PICC_WRITE;          // a0
    ucComMF522Buf[1] = addr;                // add addr + CRC
    CalulateCRC(ucComMF522Buf,2,&ucComMF522Buf[2]);

    status = PcdComMF522(PCD_TRANSCEIVE,ucComMF522Buf,4,ucComMF522Buf,&unLen,1);

	// check for ACK
    if ((status != TAG_OK) || (unLen != 4) || ((ucComMF522Buf[0] & 0x0F) != 0x0A))
    {   status = TAG_ERR;   }

    if (status == TAG_OK)
    {
        for (i=0; i<16; i++) ucComMF522Buf[i] = *(p+i);
        
        CalulateCRC(ucComMF522Buf,16,&ucComMF522Buf[16]);

        status = PcdComMF522(PCD_TRANSCEIVE,ucComMF522Buf,18,ucComMF522Buf,&unLen,1);
        
        // 0xA is ACK, any other value is NAK
        // must have received 4 bytes ??
        if ((status != TAG_OK) || (unLen != 4) || ((ucComMF522Buf[0] & 0x0F) != 0x0A))
			status = TAG_ERR;   
    }

    return(status);
}

/* halt or haltA /deselect the current using the selected card
 * 0x50 00 CRC-A
 */

char PcdHalt(void)
{
    uint8_t status;
    uint8_t unLen;
    uint8_t ucComMF522Buf[MAXRLEN];

    ucComMF522Buf[0] = PICC_HALT;       //0x50
    ucComMF522Buf[1] = 0;               //add CRC-A
    CalulateCRC(ucComMF522Buf,2,&ucComMF522Buf[2]);
                                        // sent to card
    status = PcdComMF522(PCD_TRANSCEIVE,ucComMF522Buf,4,ucComMF522Buf,&unLen,1);

	Pcd_stopcrypto1();
	
    return(status);
}

/* Restore, decrement or increment a value block
 * block must have been authorised before
 * act = PICC_INCREMENT or PICC_DECREMENT or PICC_RESTORE
 * */

char PcdValue(uint8_t act, uint8_t  addr, double value)
{
    uint8_t status;
    uint8_t unLen;
    uint8_t ucComMF522Buf[MAXRLEN];

	// sent command and block address
    ucComMF522Buf[0] = act;
    ucComMF522Buf[1] = addr;     
    
    //add CRC-A
    CalulateCRC(ucComMF522Buf,2,&ucComMF522Buf[2]);
    
    // sent to card 
    status = PcdComMF522(PCD_TRANSCEIVE,ucComMF522Buf,4,ucComMF522Buf,&unLen,1);
	
	if (status != TAG_OK)
	{
		p_printf(RED, "!!!! Can not sent command !!!!!\n");
		return(status);
	}
	
	// sent the amount
	ucComMF522Buf[0] = ((long long) value & (long long) 0xff);
	ucComMF522Buf[1] = ((long long) value & (long long) 0xff00) >> 8;
    ucComMF522Buf[2] = ((long long) value & (long long) 0xff0000) >> 16;
    ucComMF522Buf[3] = ((long long) value & (long long) 0xff000000) >> 24;
	
	//add CRC-A 
    CalulateCRC(ucComMF522Buf,4,&ucComMF522Buf[4]);
    
    /* sent to card 
     * 
     * !!! NO check on time out !!!
     * 
     * This necessary during data transmission with increment, decrement,
	 * transfer these commands time out as there is NAK responds
	 * to be expected from the PICC. 
	 */
    
    status = PcdComMF522(PCD_TRANSCEIVE,ucComMF522Buf,6,ucComMF522Buf,&unLen,0);
	
	if (status != TAG_OK) 	p_printf(RED, "!!!! Can not sent value !!!!\n");
	
    return(status);  
}

/* stop with crypto1 when ready with communication after authentication */
void  Pcd_stopcrypto1()
{
	
    /* clear MIFARE Crypto-1  
     * needed after authorisation otherwise no further card can be read*/
    ClearBitMask(Status2Reg,0x08);
}
/* check whether already authorized 
 * return
 * 0 = authorized
 * 1 = not authorized
 */
int PcdCheckAuth()
{
	if (ReadRawRC(Status2Reg) & 0x08) return(0);
	
	return(1);
}

/* ISO/IEC 13239 standard:
 *
 * CRC-A initial value of 0x6363 / not inverted after calc (set in PcdReset)
 * 
 * pIn = pointer to input values
 * len = number bytes in input values
 * pOut = store CRC calculation result.
 * 
 */
void CalulateCRC(uint8_t *pIn ,uint8_t   len,uint8_t *pOut )
{
    uint8_t   i,n;
   
    // clear the calcCRC is active and all data is processed bit 
    ClearBitMask(DivIrqReg,0x04);
    
    // cancel any other command that might be pending/executed
    WriteRawRC(CommandReg,PCD_IDLE);
    
    // flush read/write pointers and ERRORreg BufferOvfl bit
    SetBitMask(FIFOLevelReg,0x80);
   
	// fill the FIFO with the data
    for (i=0; i<len; i++)  WriteRawRC(FIFODataReg, *(pIn +i));  
    
    // start calculation of the CRC
    WriteRawRC(CommandReg, PCD_CALCCRC);
    
    // set a time out
    i = 0xFF;
    
    // loop until time out of CRC has been calculated bit has been set
    do
    {
        n = ReadRawRC(DivIrqReg);
        i--;
    }
    while ((i!=0) && !(n&0x04));
    
    // read the calucated CRC value (no check on timeout, CRC is wrong anyway)
    pOut [0] = ReadRawRC(CRCResultRegL);
    pOut [1] = ReadRawRC(CRCResultRegM);
}

/* perform a soft-reset and set the correct values */
char PcdReset(void)
{
    // perform a softreset
    WriteRawRC(CommandReg,PCD_RESETPHASE);      
    
    // give it time to settle
    usleep(10000);
    
    // wait for Soft power-down to finish
    while (ReadRawRC(CommandReg) & 0x10);
    
    // Turn antenna off
    PcdAntennaOff();
    
    // give it time to settle
    usleep(10000);
    
	/* setup internal timer (used in PcdComMF522 for timeout)
	 * 
	 * This TPrescaler is set to 0xa9 => 169 => 40 khz => 25us
	 * The reload value is 0x3e8 = 1000, as such :  25 ms
	 */

    /* timer mode 
     * B7    = 1  Timer start automatically at end of transmission
     * B6/B5 = 00 Non-gated mode (no dependency on MFIN or AUX1 pin)
     * B4    = 0  No automatic restart (comIRQReg TimeIRQ bit is set at 0)
     * B3-B0 = 0x0 Higher 4 bits of Tprescaler divider
     */
    WriteRawRC(TModeReg,0x80);

    /* B7 -B0 = Tprescaler LSB 7 bits of divider */
    WriteRawRC(TPrescalerReg,0xA9); // + TmodeReg[3...0]

    /* time reload value, lower 8 bits */
    WriteRawRC(TReloadRegL,0xE8);

    /* timer reload value, higher 8 bits */
    WriteRawRC(TReloadRegH,0x03);

    /* forces a 100% ASK modulation independent of modGsPReg  (Type-A)
     * ASK = Amplitude Shift Keying */
    WriteRawRC(TxASKReg,0x40); 

    /* Preset value
     * B7     0  CRC does not start with MSB first
     * B6     0  Not used
     * B5     1  transmitter can only be started if RF field is generated
     * B4     0  not used
     * B3     1  Polarity on MFIN is active high
     * B2     1  not used
     * B1 B0  01 CRC preset value 6363 (TYPE-A)*/
    WriteRawRC(ModeReg,0x2D);        //CRC initialize value 

    // Turn antenna on
    PcdAntennaOn();
    
    return(TAG_OK);
}

/* reads a value over SPI
 * Bit 7 is set to 1 indicating read operation
 * Bits 6 to 1 define the address 
 * Bit 0 is set to 0
 */
uint8_t ReadRawRC(uint8_t Address)
{
    char buff[2];
    
    buff[0] = ((Address<<1)&0x7E)|0x80;
    bcm2835_spi_transfern(buff,2);
    return (uint8_t)buff[1];
}

/* sent a value over SPI 
 * Bit 7 is set to 0 indicating write operation
 * Bits 6 to 1 define the address 
 * Bit 0 is set to 0
 */
uint8_t WriteRawRC(uint8_t Address, uint8_t value)
{
    char buff[2];
    
    buff[0] = (char)((Address<<1)&0x7E);
    buff[1] = (char)value;

    bcm2835_spi_transfern(buff,2);

    // return error register
    return(ReadRawRC(ErrorReg));
    
}

// set bits in RC522 registers
void SetBitMask(uint8_t   reg,uint8_t   mask)
{
    char   tmp = 0x0;
    tmp = ReadRawRC(reg);
    WriteRawRC(reg,tmp | mask);  // set bit mask

}

//clear bits in RC522 registers
void ClearBitMask(uint8_t   reg,uint8_t   mask)
{
    char   tmp = 0x0;
    tmp = ReadRawRC(reg);
    WriteRawRC(reg, tmp & ~mask);  // clear bit mask
}

// communicate with card
char PcdComMF522(uint8_t   Command,
		uint8_t *pIn ,				// buffer to sent
		uint8_t   InLenByte,		// num bytes to sent
		uint8_t *pOut ,				// buffer to receive
		uint8_t *pOutLenBit	,		// num bits stored in receive buffer
		int		CheckTimeout)		// check timeout =1 
{
	
    char   status = TAG_ERR;		// return code
    uint8_t   irqEn   = 0x00;		// IRQ register content
    uint8_t   waitFor = 0x00;		
    uint8_t   lastBits;
    uint8_t   n;
    uint32_t   i;
    uint8_t PcdErr;

    switch (Command)
    {
    case PCD_AUTHENT:
        irqEn   = 0x12; // enable Idle and low alert interrupts
        waitFor = 0x10;	// expect interrupt to continue (idle interrupt)
        break;
    case PCD_TRANSCEIVE:
        irqEn   = 0x77; // enable all alert except high alert interrupt
        waitFor = 0x30; // expect interrupt to continue (Idle and received end of valid data stream)
        break;
    default:
		p_printf(RED, "Fatal internal error in PcdComMF522. invalide command 0x%x\n",Command);
		//close_out(1);
        break;			
    }
    /*
     * B7   1 IRQ inverted with respect to Status1reg IRq bit
     * B6   1 allows transmitter interrupt request
     * B5   1 Allow receiver interrupt request
     * B4   1 allow idle interrupt request
     * B3   1 allow high alert interrupt request
     * B2   1 allows low alert interrupt request
     * B1   1 allow error interrupt request
     * B0   1 allows time interrupt request
     */
    WriteRawRC(ComIEnReg,irqEn|0x80);

    /* clear IRQ inverted respect to status1reg 
     * indicates that the marked bits in the ComIrqReg register are set */
    ClearBitMask(ComIrqReg,0x80);

    /* Flushbuffer / clear FIFO*/
    SetBitMask(FIFOLevelReg,0x80);

    /* sent command to set RC522 to Idle & cancel any pending execution */
    WriteRawRC(CommandReg,PCD_IDLE);

    /* write data in FIFO data for the length of buffer*/
    for (i=0; i<InLenByte; i++)  WriteRawRC(FIFODataReg, pIn [i]);

    /* sent the command */
    WriteRawRC(CommandReg, Command);

    /* start the transmission of data in Transceive mode*/
    if (Command == PCD_TRANSCEIVE) SetBitMask(BitFramingReg,0x80);

    i = 150;

    /* as long as not count down
     * Time out has not happened
     * and other possible wait for interrupts
     * n&01 = time out has happened (timeout was set in PcdReset as 24ms)
     * n&waitfor = good interrupts to happen
     */

    do
    {
        usleep(200);
 
        /* read interrupt register */
        n = ReadRawRC(ComIrqReg);
        i--;
    }
    while (i!=0 && (! (n&0x01)) && (!(n&waitFor)));

    /* removing start data transmission in transceive */
    if (Command == PCD_TRANSCEIVE) ClearBitMask(BitFramingReg,0x80);
    
    /* maybe something received as no count-down */
    if (i != 0)
    {
        PcdErr=ReadRawRC(ErrorReg);

        /* check for parity or buffer overflow/protocol err */
        if (!(PcdErr & 0x11))
        {
            status = TAG_OK;
            
            /* time out had triggered  (~24 ms) */
            if (n & irqEn & 0x01)
            {
				/* necessary to discard during data transmission with increment, decrement,
				 * transfer these commands time out as there is NAK responds
				 * to be expected. */
				if (CheckTimeout)  status = TAG_NOTAG;
			}
            else if (Command == PCD_TRANSCEIVE) 
            {
                /* get # of bytes received in FIFO */
                n = ReadRawRC(FIFOLevelReg);

                /* get # valid bits in the last byte 00 = all*/
                lastBits = ReadRawRC(ControlReg) & 0x07;

                /* determine the correct amount of bits */
                if (lastBits) *pOutLenBit = (n-1)*8 + lastBits;
                else *pOutLenBit = n*8;

                /* set counter start and maximum good */
                if (n == 0) n = 1;
                if (n > MAXRLEN) n = MAXRLEN;

                /* read the receive information */
                for (i=0; i < n; i++) {
                    pOut [i] = ReadRawRC(FIFODataReg);
                
                /* Based on field testing, usleep seems to be needed from some cards / systems
                 * The usleep code itself takes 70us and as such this takes 80us
                 * that is about the same as the printf underneath and seemed to be necessary
                 * during a field test.
                 * 
                 * don't ask me why.... */               
                     
                    //usleep(10);
                 
					//printf ("%s :%d .%02X\n",__func__,i, pOut[i]);
                }

                /* Given the comment above, an extra delay can now be requested on the command
                 * line with -E. The 80us mentioned above was resulting in 400us or 500us depending
                 * on the number of databytes that were received. 
                 * 
                 * allow for 500us extra delay for slow card to recover */
                if (extra_delay) usleep(500);	
            }
        }
        else 
            status = TAG_ERR;

        /* collision error detected ?*/
        if (PcdErr & 0x08) 
        {
            if (1) p_printf (RED,"Collision \n");
            status = TAG_COLLISION;
        }
    }
   
    SetBitMask(ControlReg,0x80);           // stop timer now
// printf("%s : status %02x, length of message %d, PCD err %02x\n"
// ,__func__, status,*pOutLenBit, PcdErr);

    return status;
}

/* Turn tX2RFE on, TX1RFE on (unless on already !)
 * output signal on pin TX1 and TX2 delivers the 13.56MHZ energy carrier modulated
 * by the transmission data*/
void PcdAntennaOn(void)
{
    // check if not on already
    if (!(ReadRawRC(TxControlReg) & 0x03))
            SetBitMask(TxControlReg, 0x03);
}

/* turn off antenna */
void PcdAntennaOff(void)
{
    ClearBitMask(TxControlReg, 0x03);
}
