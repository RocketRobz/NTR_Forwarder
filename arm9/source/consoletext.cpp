/*
    NitroHax -- Cheat tool for the Nintendo DS
    Copyright (C) 2008  Michael "Chishm" Chisholm

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "consoletext.h"
#include <string.h>

#define TAB_STOP 4
#define SCREEN_WIDTH 32

ConsoleText::ConsoleText (int width, int height, CHAR_SIZE tileSize, u16* fontMap, u16 palette)
{
	this->width = width;
	this->height = height;
	this->tileSize = tileSize;
	this->fontStart = 0;
	this->fontMap = fontMap;
	this->fontPal = (u16)(palette << 12);
	this->row = 0;
	this->col = 0;
}

void ConsoleText::clearText (void)
{
	setPosition (0, 0);
	for (int i = 0; i < width * height; i++) {
		putChar (' ');
	}
	setPosition (0, 0);
}

void ConsoleText::clearText (int startRow, int startCol, int endRow, int endCol)
{
	for (int i = startRow; i <= endRow; i++) {
		for (int j = startCol; j <= endCol; j++) {
			putChar (' ', i, j);
		}
	}
}


void ConsoleText::setPosition (int row, int col)
{
	this->row = row;
	this->col = col;
}

void ConsoleText::putText (const char* str)
{
	putText (str, 0, 0, height-1, width-1);
}

int ConsoleText::putText (const char* str, int startRow, int startCol, int endRow, int endCol, int curRow, int curCol)
{
	int pos = 0;
	int len = strlen (str);
	row = curRow;
	col = curCol;
	char curChar;
	
	while (pos < len) {
		curChar = str[pos];
		switch (curChar) {
			case '\r':
				col = startCol;
				break;
			case '\n':
				row++;
				col = startCol;
				break;
			case '\t':
				col += TAB_STOP;
				col = col - (col % TAB_STOP);
				break;
			default:
				if (col > endCol) {
					row++;
					col = startCol;
				}
				
				if (row > endRow) {
					// Overflowed the screen so bail out
					return row;
				}
				
				putChar (curChar, row, col);
				col++;
				break;
		}
		pos++;
	}
	
	return row;
}

void ConsoleText::putChar (char chr)
{
	putChar (chr, row, col);
	col++;
	if (col >= width) {
		row++;
		col = 0;
	}
	if (row >= height) {
		row = 0;
	}
}

void ConsoleText::putChar (char chr, int row, int col)
{
	if (tileSize == CHAR_SIZE_8PX) {
		fontMap[col + row * SCREEN_WIDTH] = fontPal | (u16)(chr + fontStart);
	} else {
		fontMap[col * 2 + row * 2 * SCREEN_WIDTH] = fontPal | (u16)(chr * 4 + fontStart);
		fontMap[(col * 2 + 1) + row * 2 * SCREEN_WIDTH] = fontPal | (u16)(chr * 4 + 1 + fontStart);
		fontMap[col * 2 + (row * 2 + 1) * SCREEN_WIDTH] = fontPal | (u16)(chr * 4 + 2 + fontStart);
		fontMap[(col * 2 + 1) + (row * 2 + 1) * SCREEN_WIDTH] = fontPal | (u16)(chr * 4 + 3 + fontStart);
	}
}

void ConsoleText::putTile (int val, int row, int col, int palette)
{
	u16 pal = (u16)(palette << 12);
	if (tileSize == CHAR_SIZE_8PX) {
		fontMap[col + row * SCREEN_WIDTH] = pal | (u16)val;
	} else {
		fontMap[col * 2 + row * 2 * SCREEN_WIDTH] = pal | (u16)(val * 4);
		fontMap[(col * 2 + 1) + row * 2 * SCREEN_WIDTH] = pal | (u16)(val * 4 + 1);
		fontMap[col * 2 + (row * 2 + 1) * SCREEN_WIDTH] = pal | (u16)(val * 4 + 2);
		fontMap[(col * 2 + 1) + (row * 2 + 1) * SCREEN_WIDTH] = pal | (u16)(val * 4 + 3);
	}
}
