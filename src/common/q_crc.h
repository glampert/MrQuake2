/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#ifndef Q_CRC_H
#define Q_CRC_H

void CRC_Init(unsigned short * crcvalue);
void CRC_ProcessByte(unsigned short * crcvalue, qbyte data);
unsigned short CRC_Value(unsigned short crcvalue);
unsigned short CRC_Block(qbyte * start, int count);

#endif // Q_CRC_H
