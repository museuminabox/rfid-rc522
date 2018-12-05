/*
 * rfid.h
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

#ifndef RFID_H_
#define RFID_H_

#include <string.h>
#include "rc522.h"
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

// activate card
tag_stat find_tag(uint16_t *);

// select a card and get UID.
tag_stat select_tag_sn(uint8_t * sn, uint8_t * len, uint8_t *SAK);

// read block content and return as a string in ascii
tag_stat read_tag_str(uint8_t addr, unsigned char * str);

// read block content and return as bytes
tag_stat read_tag_raw(uint8_t addr, uint8_t * buff); 
#ifdef __cplusplus
}
#endif

#endif /* RFID_H_ */
