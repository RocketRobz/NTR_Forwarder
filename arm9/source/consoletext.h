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

#include <nds/ndstypes.h>

class ConsoleText 
{
public:
	enum CHAR_SIZE {CHAR_SIZE_8PX, CHAR_SIZE_16PX};

	ConsoleText (int width, int height, CHAR_SIZE tileSize, u16* fontMap, u16 palette);

	void setPosition (int x, int y);
	void clearText (void);
	void clearText (int startRow, int startCol, int endRow, int endCol);
	void putText (const char* str);
	int  putText (const char* str, int startRow, int startCol, int endRow, int endCol, int curRow, int curCol);
	int  putText (const char* str, int startRow, int startCol, int endRow, int endCol)
	{
		return putText (str, startRow, startCol, endRow, endCol, row, col);
	}
	
	void putChar (char chr);
	void putChar (char chr, int row, int col);
	void putTile (int val, int row, int col, int palette);

private:
	u16 fontPal;
	int row, col;
	int width, height;
	int fontStart;
	u16* fontMap;
	CHAR_SIZE tileSize;
} ;
