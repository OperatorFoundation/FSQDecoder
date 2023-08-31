#ifndef _FSQVaricode_H
#define _FSQVaricode_H

#include <string>

#include "crc8.h"

std::string rx_text;
std::string	toprint;
std::string	station_calling;

char szestimate[40];
int	nibbles[199];
int	curr_nibble;
int	prev_nibble;

CRC8 crc;

#endif