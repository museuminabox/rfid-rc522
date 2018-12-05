/*
 * rfid.c
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
 *  Created on: 06.09.2013
 *      Author: alexs
 * 
 * version 1.51 / paulvh / July 2017
 * Fixed number of bugs and code clean-up.
 * 
 * version 1.65 / paulvh / october 2017
 * 	read_tag_raw() call added
 */

#include "rfid.h"

// buffer for 18 (maxRLEN) bytes
uint8_t buff[MAXRLEN];


// start the card activation
tag_stat find_tag(uint16_t * card_type) {
    tag_stat tmp;

    /* perform Transceive request to obtain card info
     * IDLE will follow POR (power on request) and communication failure
     * REQALL => all cards in the initial state will respond
     * REQIDL => cards that were NOT set into halt state will respond
     * 
     * Using REQALL assuming there is only 1 card in front of the reader
     * and you can leave it on the reader during testing
     *
     * MSB is sent first. The ATQA will NOT have a CRC attached.
     * */

    //if ((tmp=PcdRequest(PICC_REQIDL,buff))==TAG_OK) {

    if ((tmp=PcdRequest(PICC_REQALL,buff))==TAG_OK) {
        *card_type=(int)(buff[0]<<8|buff[1]);
    }

    return tmp;
}
/*
 * Will obtain the UID of the card and select the current card
 * Depending on the UID length (4/7/10 bits) more anticollision calls
 * are needed :
 * 
 * 0x93 = cascade level 1   return UID0 - UID3 or CT+UID0-UID2 + BCC
 * 0x95 = cascade level 2   return UID3 - UID6 or CT+UID3-UID5 + BCC
 * 0x97 = cascade level 3	return UID6 - UID9 + BCC
 *
 */
tag_stat select_tag_sn(uint8_t *sn, uint8_t * len, uint8_t *SAK)
{
    /* get basic UID from card */
    if (PcdAnticoll(PICC_ANTICOLL1,buff) != TAG_OK) return TAG_ERR;
  
    /* select the card */
    if (PcdSelect(PICC_ANTICOLL1,buff,SAK) != TAG_OK) return TAG_ERR;
    
    if (buff[0]== 0x88) {       // 0x88 is CT = Cascade Tag (means next level to get UID)
		
		//copy UID0, UID1, UID2
        memcpy(sn,&buff[1],3);
        
        // 0x95 => get cascade level 2 and select
        if (PcdAnticoll(PICC_ANTICOLL2,buff)!= TAG_OK) return TAG_ERR;
        
        if (PcdSelect(PICC_ANTICOLL2,buff,SAK) != TAG_OK) return TAG_ERR;

        if (buff[0]==0x88) 
        {
            // add UID3, UID4 and UID5
            memcpy(sn+3,&buff[1],3);
           
            // 0x97 => get even more extended serial number
            
            if (PcdAnticoll(PICC_ANTICOLL3,buff)!=TAG_OK)  return TAG_ERR;

            if (PcdSelect(PICC_ANTICOLL3,buff,SAK)!=TAG_OK) return TAG_ERR;
			
			// add UID6, UID7, UID8 UID9
            memcpy(sn+6,buff,4);
            *len=10;        // 10 byte UID (triple size UID) PSS
        }
        else
        {
            // add UID3, UID4, UID5 UID6
            memcpy(sn+3,buff,4);
            /* 7 byte UID (double size) Mifare Ultralight, Ultralight C, Desfire
             * Desfire EV1 and Mifare Plus (7 B UID)
             */
            *len=7;         // 7 byte Unique ID (double size UID)
        }
    }
    else
    {
		//copy UID0, UID1, UID2, UID3
        memcpy(sn,&buff[0],4);
        /* 4 byte non-unique ID (single byte UID) - discontinued end 2010
         * were set at manufacturing.
         * used on Mifare Classic 1K, Mifare Classic 4K and some Mifare Plus versions
         */
        *len=4;
    }
    return TAG_OK;
}


/* read a block and return value as raw bytes 
 * addr = block address that must have been authorised before
 * buff = buff (can hold at least 16 bytes)
 * 
 * return
 * Ok - TAG_OK
 * Crc _ TAG ERRCRC
 * error - TAG_ERR*/

tag_stat read_tag_raw(uint8_t addr, uint8_t *buff) 
{
    return(PcdRead(addr,buff));
}

/* read a block and return value as string 
 * addr = block address that must have been authorised before
 * str = string to copy (can hold at least 16 bytes)*/

tag_stat read_tag_str(uint8_t addr, unsigned char * str) {
    
    tag_stat tmp;
    char *p;
    uint8_t i;

    uint8_t buff[MAXRLEN];

    tmp=read_tag_raw(addr,buff);
    p= (char *) str;
    // printf("read return status %x\n",tmp);

    if (tmp == TAG_OK) 
    {
        for (i=0;i<16;i++) {
            sprintf(p,"%02x",(char)buff[i]);
            p+=2;
        }
    }

	else if (tmp == TAG_ERRCRC)
        sprintf(p,"CRC Error");
        
    else
    {
        sprintf(p,"Unknown error");
	}
  
    return tmp;
}


