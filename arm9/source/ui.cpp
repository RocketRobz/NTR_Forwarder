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

#include <nds.h>
#include <stdio.h>
#include <string.h>
#include <vector>
#include <stack>
#include <algorithm>
#include <unistd.h>
#include <dirent.h>
#include "ui.h"
#include "bios_decompress_callback.h"

#include "font.h"
#include "bgtop.h"
#include "bgsub.h"
#include "scrollbar.h"
#include "cursor.h"
#include "textbox.h"
#include "button_off.h"
#include "button_on.h"
#include "button_folder.h"
#include "button_file.h"
#include "button_go.h"
#include "button_go_boost.h"

#define MAX_CHARS_PER_SCREEN 768

#define CONSOLE_SCREEN_WIDTH 32
#define CONSOLE_SCREEN_HEIGHT 24
#define CONSOLE_SUB_START_ROW 24
#define MAP_SIZE 1024

#define FIRST_ROW 0
#define FIRST_COL 0
#define LAST_ROW (CONSOLE_SCREEN_HEIGHT - 1)
#define LAST_COL (CONSOLE_SCREEN_WIDTH - 1)

#define MENU_LAST_COL 28
#define MENU_NUM_ITEMS 10
#define MENU_FIRST_ROW 0
#define MENU_FIRST_COL 0
#define MENU_LAST_ROW (MENU_FIRST_ROW + (MENU_NUM_ITEMS - 1))

#define MENU_CONTROLS_ROW 11

#define TITLE_BOX_START_ROW 8
#define TITLE_BOX_END_ROW 12

#define TEXT_BOX_START_ROW 13
#define TEXT_BOX_END_ROW 23

#define TOUCH_GRID_SIZE 16

#define SCROLLBAR_START MENU_FIRST_ROW
#define SCROLLBAR_END (MENU_FIRST_ROW + MENU_NUM_ITEMS - 1)
#define SCROLLBAR_COL 15

#define GO_BUTTON_COL 8
#define GO_BUTTON_ROW 21
#define GO_BUTTON_WIDTH 16
#define GO_BUTTON_HEIGHT 3

#define CURSOR_VERTICAL_OFFSET (-8) // Compensate for position of cursor image within bitmap file

#define TILE_SIZE 32
#define PALETTE_SIZE 16

// button combinations for the menus
#define BUTTON_LINE_UP KEY_UP
#define BUTTON_LINE_DOWN KEY_DOWN
#define BUTTON_PAGE_UP KEY_L
#define BUTTON_PAGE_DOWN KEY_R
#define BUTTON_SELECT KEY_A
#define BUTTON_BACK KEY_B
#define BUTTON_EXIT KEY_START
#define BUTTON_BOOST KEY_SELECT
#define BUTTON_ENABLE_ALL KEY_X
#define BUTTON_DISABLE_ALL KEY_Y

#define MENU_PAGE_SCROLL 6

#define CHEAT_MENU_FOLDER_UP -1

using namespace std;

const char CHEAT_MENU_FOLDER_UP_NAME[] = " [..]";

UserInterface ui;

void vramcpy (void* dest, const void* src, int size) 
{
	u16* destination = (u16*)dest;
	u16* source = (u16*)src;
	while (size > 0) {
		*destination++ = *source++;
		size-=2;
	}
}

UserInterface::UserInterface (void) 
{
	videoSetMode(MODE_0_2D | DISPLAY_BG0_ACTIVE | DISPLAY_BG1_ACTIVE | DISPLAY_BG2_ACTIVE);
	// BG0 = backdrop
	// BG1 = text box background & border
	// BG2 = text
	vramSetBankA (VRAM_A_MAIN_BG_0x06000000);
	REG_BG0CNT = BG_MAP_BASE(0) | BG_COLOR_16 | BG_TILE_BASE(2) | BG_PRIORITY(2);
	REG_BG1CNT = BG_MAP_BASE(2) | BG_COLOR_16 | BG_TILE_BASE(4) | BG_PRIORITY(1);
	REG_BG2CNT = BG_MAP_BASE(4) | BG_COLOR_16 | BG_TILE_BASE(6) | BG_PRIORITY(0);
	BG_PALETTE[0]=0;   
	BG_PALETTE[255]=0xffff;

	videoSetModeSub(MODE_0_2D | DISPLAY_BG0_ACTIVE | 
		DISPLAY_BG1_ACTIVE | DISPLAY_BG2_ACTIVE | DISPLAY_SPR_ACTIVE | DISPLAY_SPR_1D);
	// BG0 = backdrop
	// BG1 = scrollbar & highlights
	// BG2 = text
	vramSetBankC (VRAM_C_SUB_BG_0x06200000);
	REG_BG0CNT_SUB = BG_MAP_BASE(0) | BG_COLOR_16 | BG_TILE_BASE(2) | BG_PRIORITY(2);
	REG_BG1CNT_SUB = BG_MAP_BASE(2) | BG_COLOR_16 | BG_TILE_BASE(4) | BG_PRIORITY(1);
	REG_BG2CNT_SUB = BG_MAP_BASE(4) | BG_COLOR_16 | BG_TILE_BASE(6) | BG_PRIORITY(0);
	
	// Set up background image
	swiDecompressLZSSVram ((void*)bgtopTiles, (void*)CHAR_BASE_BLOCK(2), 0, &decompressBiosCallback);
	swiDecompressLZSSVram ((void*)bgsubTiles, (void*)CHAR_BASE_BLOCK_SUB(2), 0, &decompressBiosCallback);
	vramcpy (&BG_PALETTE[0], bgtopPal, bgtopPalLen);
	vramcpy (&BG_PALETTE_SUB[0], bgsubPal, bgsubPalLen);
	u16* bgMapTop = (u16*)SCREEN_BASE_BLOCK(0);
	u16* bgMapSub = (u16*)SCREEN_BASE_BLOCK_SUB(0);
	for (int i = 0; i < CONSOLE_SCREEN_WIDTH*CONSOLE_SCREEN_HEIGHT; i++) {
		bgMapTop[i] = (u16)i;
		bgMapSub[i] = (u16)i;
	}

	// Copy text box for BG1 - use palette 3
	vramcpy (&BG_PALETTE[TEXTBOX_PALETTE * PALETTE_SIZE], textboxPal, textboxPalLen);
	vramcpy ((void*)(CHAR_BASE_BLOCK(4) + TEXTBOX_OFFSET * TILE_SIZE), textboxTiles, textboxTilesLen);
	textboxMap = (u16*)SCREEN_BASE_BLOCK(2);
	// Clear tile 0
	for (int i = 0; i < TILE_SIZE/2; i++) {
		((u16*)CHAR_BASE_BLOCK(4))[i] = 0;
	}
	clearBox();

	// Copy the font into top screen's tile base for BG2
	u16* fontDestMain = (u16*)CHAR_BASE_BLOCK(6);
	swiDecompressLZSSVram ((void*)fontTiles, fontDestMain, 0, &decompressBiosCallback);
	vramcpy (&BG_PALETTE[FONT_PALETTE * PALETTE_SIZE], fontPal, fontPalLen);
	
	// Copy the font into sub screen's tile base for BG2
	u16* fontDestSub = (u16*)CHAR_BASE_BLOCK_SUB(6);
	swiDecompressLZSSVram ((void*)fontTiles, fontDestSub, 0, &decompressBiosCallback);
	vramcpy (&BG_PALETTE_SUB[FONT_PALETTE * PALETTE_SIZE], fontPal, fontPalLen);
	BG_OFFSET_SUB[2].x = -4;
	BG_OFFSET_SUB[2].y = -4;

	u16* fontMapTop = (u16*)SCREEN_BASE_BLOCK(4);
	u16* fontMapSub = (u16*)SCREEN_BASE_BLOCK_SUB(4);

	// Initialise consoles
	topText = new ConsoleText (CONSOLE_SCREEN_WIDTH, CONSOLE_SCREEN_HEIGHT, 
		ConsoleText::CHAR_SIZE_8PX, fontMapTop, FONT_PALETTE);
	subText = new ConsoleText (MENU_LAST_COL - MENU_FIRST_COL + 1, CONSOLE_SCREEN_HEIGHT,
		ConsoleText::CHAR_SIZE_8PX, fontMapSub, FONT_PALETTE);
	for (int i = 0; i < MAP_SIZE; i++) {
		fontMapSub[i] = 0;
	}
	
	// Set up the GUI BG
	u16* tileDestSub = (u16*)CHAR_BASE_BLOCK_SUB(4);
	guiSubMap = (u16*)SCREEN_BASE_BLOCK_SUB(2);
	BG_OFFSET[1].x = 0;
	BG_OFFSET[1].y = 0;
	// Create a double size blank tile
	for (int i = 0; i < 4 * TILE_SIZE; i++) {
		tileDestSub[i] = 0;
	}
	// Clear GUI tile map
	for (int i = 0; i < CONSOLE_SCREEN_WIDTH * CONSOLE_SCREEN_HEIGHT; i++) {
		guiSubMap[i] = 0;
	}
	
	// Load scroll bar data, use palette 1
	vramcpy (&tileDestSub[SCROLLBAR_OFFSET * TILE_SIZE / 2], scrollbarTiles, scrollbarTilesLen);
	vramcpy (&BG_PALETTE_SUB[SCROLLBAR_PALETTE * PALETTE_SIZE], scrollbarPal, scrollbarPalLen);
	
	// Load button backgrounds
	vramcpy (&tileDestSub[BUTTON_BG_FOLDER * TILE_SIZE / 2], button_folderTiles, button_folderTilesLen);
	vramcpy (&BG_PALETTE_SUB[BUTTON_PALETTE_FOLDER * PALETTE_SIZE], button_folderPal, button_folderPalLen);
	vramcpy (&tileDestSub[BUTTON_BG_ON * TILE_SIZE / 2], button_onTiles, button_onTilesLen);
	vramcpy (&BG_PALETTE_SUB[BUTTON_PALETTE_ON * PALETTE_SIZE], button_onPal, button_onPalLen);
	vramcpy (&tileDestSub[BUTTON_BG_OFF * TILE_SIZE / 2], button_offTiles, button_offTilesLen);
	vramcpy (&BG_PALETTE_SUB[BUTTON_PALETTE_OFF * PALETTE_SIZE], button_offPal, button_offPalLen);
	vramcpy (&tileDestSub[BUTTON_BG_FILE * TILE_SIZE / 2], button_fileTiles, button_fileTilesLen);
	vramcpy (&BG_PALETTE_SUB[BUTTON_PALETTE_FILE * PALETTE_SIZE], button_filePal, button_filePalLen);

	// Load go button
	vramcpy (&tileDestSub[GO_BUTTON_OFFSET * TILE_SIZE / 2], button_goTiles, button_goTilesLen);
	vramcpy (&BG_PALETTE_SUB[GO_BUTTON_PALETTE * PALETTE_SIZE], button_goPal, button_goPalLen);

	// Erase any text already on screen
	topText->clearText();
	subText->clearText();
	
	// Set up sub screen sprites
	vramSetBankD (VRAM_D_SUB_SPRITE);
	Sprite::init();
	
	for (int i = 0; i < NUM_CURSORS; i++) {
		cursor[i] = new Sprite (64, 32, (const u16*)&(cursorTiles[256 * i]), cursorPal);
	}

	scrollbox = new Sprite (16, 16, (const u16*)&(scrollbarTiles[96]), scrollbarPal);
	showScrollbar (false);
}

UserInterface::~UserInterface () 
{
	delete scrollbox;
	delete topText;
	delete subText;
	for (int i = 0; i < NUM_CURSORS; i++) {
		delete cursor[i];
	}
}

void UserInterface::putGuiTile (int val, int row, int col, int palette, bool doubleSize) 
{
	u16 pal = (u16)(palette << 12);
	if (!doubleSize) {
		guiSubMap[col + row * CONSOLE_SCREEN_WIDTH] = pal | (u16)val;
	} else {
		guiSubMap[col  + row * CONSOLE_SCREEN_WIDTH] = pal | (u16)(val);
		guiSubMap[(col + 1) + row * CONSOLE_SCREEN_WIDTH] = pal | (u16)(val + 1);
		guiSubMap[col + (row + 1) * CONSOLE_SCREEN_WIDTH] = pal | (u16)(val + 2);
		guiSubMap[(col + 1) + (row + 1) * CONSOLE_SCREEN_WIDTH] = pal | (u16)(val + 3);
	}
}

void UserInterface::showScrollbar (bool visible)
{
	if (visible) {
		for (int i = SCROLLBAR_START + 1; i <= SCROLLBAR_END - 1; i++) {
			// vertical bar
			putGuiTile (SCROLLBAR_BAR, i*2, SCROLLBAR_COL * 2, SCROLLBAR_PALETTE, true);
		}
		// up arrow
		putGuiTile (SCROLLBAR_UP, SCROLLBAR_START*2, SCROLLBAR_COL * 2, SCROLLBAR_PALETTE, true);
		// down arrow
		putGuiTile (SCROLLBAR_DOWN, SCROLLBAR_END*2, SCROLLBAR_COL * 2, SCROLLBAR_PALETTE, true);
	} else {
		for (int i = SCROLLBAR_START; i <= SCROLLBAR_END ; i++) {
			// vertical bar
			putGuiTile (BLANK_TILE, i*2, SCROLLBAR_COL * 2, 0, true);
		}
	}

	scrollboxPosition = (SCROLLBAR_START + 1) * TOUCH_GRID_SIZE;
	scrollbox->setPosition (scrollboxPosition, (CONSOLE_SCREEN_WIDTH - 2) * 8);
	scrollbox->showSprite (visible);
}

void UserInterface::setScrollbarPosition (int offset, int listLength)
{
	scrollboxPosition = ((SCROLLBAR_END - SCROLLBAR_START - 2) * TOUCH_GRID_SIZE * offset / listLength) + (SCROLLBAR_START + 1) * TOUCH_GRID_SIZE;
	scrollbox->setPosition (scrollboxPosition, (CONSOLE_SCREEN_WIDTH - 2) * 8);
}

void UserInterface::showCursor (bool visible)
{
	for (int i = 0; i < NUM_CURSORS; i++) {
		cursor[i]->setPosition (16, 64 * i);
		cursor[i]->showSprite (visible);
	}
}

void UserInterface::setCursorPosition (int offset)
{
	for (int i = 0; i < NUM_CURSORS; i++) {
		cursor[i]->setPosition (offset + CURSOR_VERTICAL_OFFSET, 64 * i);
	}
}

void UserInterface::drawBox (int startRow, int startCol, int endRow, int endCol)
{
	for (int i = startCol + 1; i < endCol; i++) {
		textboxMap[startRow * CONSOLE_SCREEN_WIDTH + i] = TEXTBOX_N | (TEXTBOX_PALETTE << 12);
		textboxMap[endRow * CONSOLE_SCREEN_WIDTH + i] = TEXTBOX_S | (TEXTBOX_PALETTE << 12);		
	}
	for (int i = startRow + 1; i < endRow; i++) {
		textboxMap[i * CONSOLE_SCREEN_WIDTH + startCol] = TEXTBOX_W | (TEXTBOX_PALETTE << 12);
		textboxMap[i * CONSOLE_SCREEN_WIDTH + endCol] = TEXTBOX_E | (TEXTBOX_PALETTE << 12);		
	}
	textboxMap[startRow * CONSOLE_SCREEN_WIDTH + startCol] = TEXTBOX_NW | (TEXTBOX_PALETTE << 12);
	textboxMap[startRow * CONSOLE_SCREEN_WIDTH + endCol] = TEXTBOX_NE | (TEXTBOX_PALETTE << 12);
	textboxMap[endRow * CONSOLE_SCREEN_WIDTH + startCol] = TEXTBOX_SW | (TEXTBOX_PALETTE << 12);
	textboxMap[endRow * CONSOLE_SCREEN_WIDTH + endCol] = TEXTBOX_SE | (TEXTBOX_PALETTE << 12);
	for (int i = startRow + 1; i < endRow; i++) {
		for (int j = startCol + 1; j < endCol; j++) {
			textboxMap[i * CONSOLE_SCREEN_WIDTH + j] = TEXTBOX_C | (TEXTBOX_PALETTE << 12);
		}
	}
}

void UserInterface::clearBox (int startRow, int startCol, int endRow, int endCol)
{
	for (int i = startRow; i <= endRow; i++) {
		for (int j = startCol; j <= endCol; j++) {
			textboxMap[i * CONSOLE_SCREEN_WIDTH + j] = 0 | (TEXTBOX_PALETTE << 12);
		}
	}
}

void UserInterface::clearBox (void)
{
	for (int i = 0; i < CONSOLE_SCREEN_WIDTH*CONSOLE_SCREEN_HEIGHT; i++) {
		textboxMap[i] = 0 | (TEXTBOX_PALETTE << 12);
	}
}


void UserInterface::wordWrap (char* str, int height, int width)
{
	int maxLen = 768;
	char wrappedText[768];
	char *srcPos = str;
	char *destPos = wrappedText;
	char *wordStart;
	char *wordEnd;
	int numLines = 1;
	int spaceLeft = width;
	int wordLength;
	
	memset (wrappedText, 0, maxLen);
	
	while (*srcPos != '\0' && destPos < (wrappedText + maxLen)) {
		while (*srcPos && isspace(*srcPos)) {
			if (*srcPos == '\n') {
				*destPos++ = '\n';
				spaceLeft = width;
				++numLines;
			}
			++srcPos;		// find start of word
		}
		if (*srcPos == '\0') {
			break;
		}
		wordStart = srcPos;
		while (*srcPos && !isspace(*srcPos)) {
			++srcPos;		// find end of word
		}
		wordEnd = srcPos;
		wordLength = wordEnd - wordStart;
		if ((wordLength > spaceLeft) && (wordLength <= width)) {
			// Word needs to be put on a new line
			*destPos++ = '\n';
			spaceLeft = width;
			++numLines;
		} else if (spaceLeft < width) {
			// Not at the start of a line
			*destPos++ = ' ';
			spaceLeft = spaceLeft - 1;
		}
		memcpy (destPos, wordStart, wordLength);
		destPos += wordLength;
		spaceLeft -= wordLength;
		while (spaceLeft < 0) {
			spaceLeft += width;
		}
	}
	if (numLines <= height) {
		strcpy (str, wrappedText);
	}
}

void UserInterface::writeTextBox (TEXT_TYPE textType, const char* str, va_list args)
{
	char dispStr[MAX_CHARS_PER_SCREEN];
	int len;
	len = vsniprintf(dispStr, MAX_CHARS_PER_SCREEN, str, args);
	int lastRow;
	
	clearMessage (textType);
	if (textType == TEXT_TITLE) {
		wordWrap (dispStr, TITLE_BOX_END_ROW - TITLE_BOX_START_ROW - 2, LAST_COL - FIRST_COL - 2);
		lastRow = topText->putText (dispStr, TITLE_BOX_START_ROW + 1, FIRST_COL + 1, 
			TITLE_BOX_END_ROW - 1, LAST_COL - 1, TITLE_BOX_START_ROW + 1, FIRST_COL + 1);
		drawBox (TITLE_BOX_START_ROW, FIRST_COL, lastRow + 1, LAST_COL);
	} else {
		wordWrap (dispStr, TEXT_BOX_END_ROW - TEXT_BOX_START_ROW - 2, LAST_COL - FIRST_COL - 2);
		lastRow = topText->putText (dispStr, TEXT_BOX_START_ROW + 1, FIRST_COL + 1, 
			TEXT_BOX_END_ROW - 1, LAST_COL - 1, TEXT_BOX_START_ROW + 1, FIRST_COL + 1);
		drawBox (TEXT_BOX_START_ROW, FIRST_COL, lastRow + 1, LAST_COL);
	}
}	

void UserInterface::showMessage (const char* str, ...)
{
	va_list args;

	va_start(args, str);
	writeTextBox (TEXT_INFO, str, args);
	va_end(args);
}

void UserInterface::showMessage (TEXT_TYPE textType, const char* str, ...)
{
	va_list args;

	va_start(args, str);
	writeTextBox (textType, str, args);
	va_end(args);
}

void UserInterface::clearMessage (TEXT_TYPE textType)
{
	if (textType == TEXT_TITLE) {
		topText->clearText (TITLE_BOX_START_ROW, FIRST_COL, TITLE_BOX_END_ROW, LAST_COL);
		clearBox (TITLE_BOX_START_ROW, FIRST_COL, TITLE_BOX_END_ROW, LAST_COL);
	} else {
		topText->clearText (TEXT_BOX_START_ROW, FIRST_COL, TEXT_BOX_END_ROW, LAST_COL);
		clearBox (TEXT_BOX_START_ROW, FIRST_COL, TEXT_BOX_END_ROW, LAST_COL);
	}
}

void UserInterface::clearMessage (void)
{
	clearMessage (TEXT_TITLE);
	clearMessage (TEXT_INFO);
}

void UserInterface::putButtonBg (BUTTON_BG_OFFSETS buttonBg, int position) 
{
	int palette;
	
	if (buttonBg != BUTTON_BG_NONE) {
		if (buttonBg == BUTTON_BG_FOLDER) {
			palette = BUTTON_PALETTE_FOLDER;
		} else if (buttonBg == BUTTON_BG_FILE) {
			palette = BUTTON_PALETTE_FILE;
		} else if (buttonBg == BUTTON_BG_ON) {
			palette = BUTTON_PALETTE_ON;
		} else {
			palette = BUTTON_PALETTE_OFF;
		}
		for (int col = 0; col < 30; col++) {
			for (int row = 0; row  < 2; row++) {
				putGuiTile (buttonBg + col + row * 30, row + position , col, palette, false);
			}
		}
	} else {
		for (int col = 0; col < 30; col++) {
			for (int row = 0; row  < 2; row++) {
				putGuiTile (BLANK_TILE, row + position , col, 0, false);
			}
		}
	}
}

void UserInterface::clearFolderBackground (void)
{
	for (int i = MENU_FIRST_ROW; i <= MENU_LAST_ROW; i++) {
		putButtonBg (BUTTON_BG_NONE, i * 2);
	}
}

void UserInterface::showGoButton (bool visible, int left, int top)
{
	if (visible) {
		for (int col = 0; col < GO_BUTTON_WIDTH; col++) {
			for (int row = 0; row  < GO_BUTTON_HEIGHT; row++) {
				putGuiTile (GO_BUTTON_OFFSET + col + row * GO_BUTTON_WIDTH, row + top, col + left, GO_BUTTON_PALETTE, false);
			}
		}
	} else {
		for (int col = 0; col < GO_BUTTON_WIDTH; col++) {
			for (int row = 0; row  < GO_BUTTON_HEIGHT; row++) {
				putGuiTile (BLANK_TILE, row + top, col + left, 0, false);
			}
		}
	}
}


void UserInterface::showCheatFolder (std::vector<CheatBase*> &contents)
{
	int menuLength;
	
	menuLevel.bottom = menuLevel.top + MENU_NUM_ITEMS - 1;
	
	if (menuLevel.bottom >= (int)contents.size()) {
		menuLevel.bottom = contents.size() - 1;
	}
	
	menuLength = menuLevel.bottom - menuLevel.top;
	
	subText->clearText(MENU_FIRST_ROW, 0, MENU_LAST_ROW*2, MENU_LAST_COL);

	clearFolderBackground();
	
	for (int curItem = 0; curItem <= menuLength; curItem++) {
		if (CHEAT_MENU_FOLDER_UP == menuLevel.top + curItem) {
				putButtonBg (BUTTON_BG_FOLDER, (MENU_FIRST_ROW + curItem) * 2);
			subText->putText (CHEAT_MENU_FOLDER_UP_NAME, (MENU_FIRST_ROW + curItem) * 2,
				MENU_FIRST_COL, (MENU_FIRST_ROW + curItem) * 2, MENU_LAST_COL, (MENU_FIRST_ROW + curItem) * 2, MENU_FIRST_COL);
		} else {
			CheatCode* cheatCode = dynamic_cast<CheatCode*>(contents[menuLevel.top + curItem]);
			
			subText->putText (contents[menuLevel.top + curItem]->getName(), (MENU_FIRST_ROW + curItem) * 2,
				MENU_FIRST_COL, (MENU_FIRST_ROW + curItem) * 2, MENU_LAST_COL, (MENU_FIRST_ROW + curItem) * 2, MENU_FIRST_COL);
			
			if (cheatCode) {
				if (cheatCode->getEnabledStatus()) {
					putButtonBg (BUTTON_BG_ON, (MENU_FIRST_ROW + curItem) * 2);
				} else {
					putButtonBg (BUTTON_BG_OFF, (MENU_FIRST_ROW + curItem) * 2);
				}
			} else {
				putButtonBg (BUTTON_BG_FOLDER, (MENU_FIRST_ROW + curItem) * 2);
			}
		}
	}
}

#define CHEAT_MENU_TOP (gameCodes==top?0:CHEAT_MENU_FOLDER_UP)

void UserInterface::cheatMenu (CheatFolder* gameCodes, CheatFolder* top)
{
	touchPosition touchXY;
	menuLevel.top = 0;
	menuLevel.selected = 0;
	int pressed;
	
	std::stack<UserInterface::MENU_LEVEL> menuLevelStack;

	std::vector<CheatBase*> contents = gameCodes->getContents();
	
	showScrollbar(true);
	showCheatFolder (contents);
	showCursor (true);
	showGoButton(true, GO_BUTTON_COL, GO_BUTTON_ROW);

	do {
		swiWaitForVBlank();
		scanKeys();
		pressed = keysDown();
		touchRead(&touchXY);

		// Touch screen stuff
		if (pressed & KEY_TOUCH) {
			if ((touchXY.px < (SCROLLBAR_COL * TOUCH_GRID_SIZE)) && 
				(touchXY.py >= (MENU_FIRST_ROW * TOUCH_GRID_SIZE)) &&
				(touchXY.py < ((MENU_FIRST_ROW + menuLevel.bottom - menuLevel.top + 1) * TOUCH_GRID_SIZE))) 
			{
				// Touched an item
				menuLevel.selected = (touchXY.py / TOUCH_GRID_SIZE) + menuLevel.top - MENU_FIRST_ROW;
				pressed = BUTTON_SELECT;
			}
			if (touchXY.px >= (SCROLLBAR_COL * TOUCH_GRID_SIZE) && touchXY.px < ((SCROLLBAR_COL+1) * TOUCH_GRID_SIZE)) {
				if ((touchXY.py >= (SCROLLBAR_START) * TOUCH_GRID_SIZE) && (touchXY.py < (SCROLLBAR_START + 1) * TOUCH_GRID_SIZE)) {
					// Touched up arrow
					pressed = BUTTON_LINE_UP;
				} else if ((touchXY.py >= SCROLLBAR_END * TOUCH_GRID_SIZE) && (touchXY.py < (SCROLLBAR_END + 1) * TOUCH_GRID_SIZE)) {
					// Touched down arrow
					pressed = BUTTON_LINE_DOWN;
				} else if ((touchXY.py >= (SCROLLBAR_START + 1) * TOUCH_GRID_SIZE) && (touchXY.py < scrollboxPosition)) {
					// Touched scrollbar above scrollbox
					pressed = BUTTON_PAGE_UP;
				} else if ((touchXY.py >= scrollboxPosition + TOUCH_GRID_SIZE) && (touchXY.py < (SCROLLBAR_END) * TOUCH_GRID_SIZE)) {
					// Touched scrollbar below scrollbox
					pressed = BUTTON_PAGE_DOWN;
				}
			}
			if ((touchXY.px >= (GO_BUTTON_COL * TOUCH_GRID_SIZE / 2)) && 
				(touchXY.px < ((GO_BUTTON_COL + GO_BUTTON_WIDTH) * TOUCH_GRID_SIZE / 2)) &&
				(touchXY.py >= (GO_BUTTON_ROW * TOUCH_GRID_SIZE / 2)) && 
				(touchXY.py < ((GO_BUTTON_ROW + GO_BUTTON_HEIGHT) * TOUCH_GRID_SIZE / 2)))
			{
				// Touched the go button
				pressed = BUTTON_EXIT;
			}
		}

		if (pressed == BUTTON_LINE_DOWN) {
			if (menuLevel.selected < (int)contents.size()-1) {
				menuLevel.selected ++;
				if (menuLevel.selected > menuLevel.bottom) {
					menuLevel.top ++;
					showCheatFolder (contents);
				}
			}
		} else if (pressed == BUTTON_LINE_UP) {
			if (menuLevel.selected > CHEAT_MENU_TOP) {
				menuLevel.selected --;
				if (menuLevel.selected < menuLevel.top) {
					menuLevel.top --;
					showCheatFolder (contents);
				}
			}
		} else if (pressed == BUTTON_BOOST) {
			TWLBoostCPU = true;
		} else if (pressed == BUTTON_SELECT) {
			if (menuLevel.selected == CHEAT_MENU_FOLDER_UP) {
				CheatFolder* cheatFolder = gameCodes->getParent();
				if (!cheatFolder || gameCodes == top) {
					// Back out of top level folder
					break;
				} else {
					gameCodes = cheatFolder;
					if (!menuLevelStack.empty()) {
						menuLevel = menuLevelStack.top();
						menuLevelStack.pop();
					} else {
						menuLevel.top = CHEAT_MENU_TOP;
						menuLevel.selected = CHEAT_MENU_TOP;
					}
					contents = gameCodes->getContents();
					showCheatFolder (contents);
				}
			} else {
				CheatCode* cheatCode = dynamic_cast<CheatCode*>(contents[menuLevel.selected]);
				CheatFolder* cheatFolder = dynamic_cast<CheatFolder*>(contents[menuLevel.selected]);
				if (cheatCode) {
					cheatCode->toggleEnabled();
					showCheatFolder (contents);
				} else if (cheatFolder) {
					menuLevelStack.push(menuLevel);
					gameCodes = cheatFolder;
					menuLevel.top = CHEAT_MENU_TOP;
					menuLevel.selected = CHEAT_MENU_TOP;
					contents = gameCodes->getContents();
					showCheatFolder (contents);
				}
			}
		} else if (pressed == BUTTON_BACK) {
			CheatFolder* cheatFolder = gameCodes->getParent();
			if (!cheatFolder || gameCodes == top) {
				// Back out of top level folder
				break;
			} else {
				if (!menuLevelStack.empty()) {
					menuLevel = menuLevelStack.top();
					menuLevelStack.pop();
				} else {
					menuLevel.top = CHEAT_MENU_TOP;
					menuLevel.selected = CHEAT_MENU_TOP;
				}
				gameCodes = cheatFolder;
				contents = gameCodes->getContents();
				showCheatFolder (contents);
			}
		} else if (pressed == BUTTON_PAGE_UP) {
			menuLevel.selected -= MENU_PAGE_SCROLL;
			if (menuLevel.selected < CHEAT_MENU_TOP) {
				menuLevel.selected = CHEAT_MENU_TOP;
			}
			menuLevel.top -= MENU_PAGE_SCROLL;
			if (menuLevel.top < CHEAT_MENU_TOP) {
				menuLevel.top = CHEAT_MENU_TOP;
			}
			showCheatFolder (contents);
		} else if (pressed == BUTTON_PAGE_DOWN) {
			menuLevel.selected += MENU_PAGE_SCROLL;
			if (menuLevel.selected > (int)contents.size()-1) {
				menuLevel.selected = (int)contents.size()-1;
			}
			menuLevel.top += MENU_PAGE_SCROLL;
			if (menuLevel.top > (int)contents.size()-MENU_NUM_ITEMS) {
				menuLevel.top = (int)contents.size()-MENU_NUM_ITEMS;
			}
			if (menuLevel.top < CHEAT_MENU_TOP) {
				menuLevel.top = CHEAT_MENU_TOP;
			}
			showCheatFolder (contents);
		} else if (pressed == BUTTON_ENABLE_ALL) {
			gameCodes->enableAll(true);
			showCheatFolder (contents);
		} else if (pressed == BUTTON_DISABLE_ALL) {
			gameCodes->enableAll(false);
			showCheatFolder (contents);
		}
		setCursorPosition ((MENU_FIRST_ROW + menuLevel.selected - menuLevel.top) * TOUCH_GRID_SIZE);
		setScrollbarPosition (CHEAT_MENU_TOP + menuLevel.selected, (int)contents.size()-1);
		showMessage (TEXT_TITLE, "%s", gameCodes->getName());
		if (contents[menuLevel.selected]->note.empty()) {
			showMessage (TEXT_INFO, "%s", contents[menuLevel.selected]->getName());
		} else {
			showMessage (TEXT_INFO, "%s\n\n%s", contents[menuLevel.selected]->getName(), contents[menuLevel.selected]->getNote());
		}
	} while (!(pressed == BUTTON_EXIT));

	clearMessage ();
	clearFolderBackground();
	showCursor (false);
	showScrollbar (false);
	showGoButton(false, GO_BUTTON_COL, GO_BUTTON_ROW);
	subText->clearText ();
}


bool UserInterface::fileInfoPredicate (const FileInfo& lhs, const FileInfo& rhs)
{
	if (!lhs.isDirectory && rhs.isDirectory) {
		return false;
	}
	if (lhs.isDirectory && !rhs.isDirectory) {
		return true;
	}
	return strcasecmp(lhs.filename.c_str(), rhs.filename.c_str()) < 0;
}

std::vector<UserInterface::FileInfo> UserInterface::getDirContents (const char* extension) 
{
	std::vector<FileInfo> files;
	DIR* dir;
	struct stat st;
	bool isDirectory;
	string  filename;
	int len;
	int extLen = 0;
	UserInterface::FileInfo fileInfo;
	
	if (extension) {
		extLen = strlen (extension);
	}
	
	dir = opendir  (".");
	
	while(true) {
		
		struct dirent* pent = readdir(dir);
		if(pent == NULL) break;
		
		stat(pent->d_name, &st);		
		filename = pent->d_name;
		isDirectory = (st.st_mode & S_IFDIR) ? true : false;
		len = filename.length();
		
		if ( (isDirectory && strcmp(filename.c_str(), ".") != 0) || extension == NULL || !strcasecmp(extension, &filename.c_str()[len-extLen])) {
			fileInfo.filename = filename;
			fileInfo.isDirectory = isDirectory;
			files.push_back (fileInfo);
		}
	}

	closedir (dir);
	
	std::sort(files.begin(), files.end(), UserInterface::fileInfoPredicate);
	
	return files;
}

void UserInterface::showFileFolder (std::vector<UserInterface::FileInfo> &contents)
{
	int menuLength;
	
	menuLevel.bottom = menuLevel.top + MENU_LAST_ROW;
	
	if (menuLevel.bottom >= (int)contents.size()) {
		menuLevel.bottom = contents.size() - 1;
	}
	
	menuLength = menuLevel.bottom - menuLevel.top;
	
	subText->clearText(MENU_FIRST_ROW, 0, MENU_LAST_ROW*2, MENU_LAST_COL);
	
	for (int i = MENU_FIRST_ROW; i <= MENU_LAST_ROW; i++) {
		putButtonBg (BUTTON_BG_NONE, i * 2);
	}

	for (int curItem = 0; curItem <= menuLength; curItem++) {
		subText->putText (contents[menuLevel.top + curItem].filename.c_str(), (MENU_FIRST_ROW + curItem) * 2,
			MENU_FIRST_COL, (MENU_FIRST_ROW + curItem) * 2, MENU_LAST_COL, (MENU_FIRST_ROW + curItem) * 2, MENU_FIRST_COL);

		if (contents[menuLevel.top + curItem].isDirectory) {
			putButtonBg (BUTTON_BG_FOLDER, (MENU_FIRST_ROW + curItem) * 2);
		} else {
			putButtonBg (BUTTON_BG_FILE, (MENU_FIRST_ROW + curItem) * 2);
		}
	}
}

std::string UserInterface::fileBrowser (const char* extension)
{
	touchPosition touchXY;
	menuLevel.top = 0;
	menuLevel.selected = 0;
	int pressed;
	std::vector<UserInterface::FileInfo> contents;
	std::string filename;
	char* cwd = new char[PATH_MAX];
	std::stack<UserInterface::MENU_LEVEL> menuLevelStack;

	contents = getDirContents (extension);
	
	showScrollbar (true);
	showFileFolder (contents);
	showCursor (true);

	if (extension) {
		showMessage (TEXT_TITLE, "Open *%s file", extension);
	} else {
		showMessage (TEXT_TITLE, "Open file");
	}
	
	do {
		swiWaitForVBlank();
		scanKeys();
		pressed = keysDown();
		touchRead(&touchXY);

		// Touch screen stuff
		if (pressed & KEY_TOUCH) {
			if ((touchXY.px < (SCROLLBAR_COL * TOUCH_GRID_SIZE)) && 
				(touchXY.py >= (MENU_FIRST_ROW * TOUCH_GRID_SIZE)) &&
				(touchXY.py < ((MENU_FIRST_ROW + menuLevel.bottom - menuLevel.top + 1) * TOUCH_GRID_SIZE))) 
			{
				// Touched an item
				menuLevel.selected = (touchXY.py / TOUCH_GRID_SIZE) + menuLevel.top - MENU_FIRST_ROW;
				pressed = BUTTON_SELECT;
			}
			if (touchXY.px >= (SCROLLBAR_COL * TOUCH_GRID_SIZE) && touchXY.px < ((SCROLLBAR_COL+1) * TOUCH_GRID_SIZE)) {
				if ((touchXY.py >= (SCROLLBAR_START) * TOUCH_GRID_SIZE) && (touchXY.py < (SCROLLBAR_START + 1) * TOUCH_GRID_SIZE)) {
					// Touched up arrow
					pressed = BUTTON_LINE_UP;
				} else if ((touchXY.py >= SCROLLBAR_END * TOUCH_GRID_SIZE) && (touchXY.py < (SCROLLBAR_END + 1) * TOUCH_GRID_SIZE)) {
					// Touched down arrow
					pressed = BUTTON_LINE_DOWN;
				} else if ((touchXY.py >= (SCROLLBAR_START + 1) * TOUCH_GRID_SIZE) && (touchXY.py < scrollboxPosition)) {
					// Touched scrollbar above scrollbox
					pressed = BUTTON_PAGE_UP;
				} else if ((touchXY.py >= scrollboxPosition + TOUCH_GRID_SIZE) && (touchXY.py < (SCROLLBAR_END) * TOUCH_GRID_SIZE)) {
					// Touched scrollbar below scrollbox
					pressed = BUTTON_PAGE_DOWN;
				}
			}
		}

		if (pressed == BUTTON_LINE_DOWN) {
			if (menuLevel.selected < (int)contents.size()-1) {
				menuLevel.selected ++;
				if (menuLevel.selected > menuLevel.bottom) {
					menuLevel.top ++;
					showFileFolder (contents);
				}
			}
		} else if (pressed == BUTTON_LINE_UP) {
			if (menuLevel.selected > 0) {
				menuLevel.selected --;
				if (menuLevel.selected < menuLevel.top) {
					menuLevel.top --;
					showFileFolder (contents);
				}
			}
		} else if (pressed == BUTTON_BOOST) {
			TWLBoostCPU = true;
		} else if (pressed == BUTTON_SELECT) {
			if (contents[menuLevel.selected].isDirectory) {
				if (contents[menuLevel.selected].filename == "..") {
					chdir("..");
					if (!menuLevelStack.empty()) {
						menuLevel = menuLevelStack.top();
						menuLevelStack.pop();
					} else {
						menuLevel.top = 0;
						menuLevel.selected = 0;
					}
				} else {
					chdir (contents[menuLevel.selected].filename.c_str());
					menuLevelStack.push(menuLevel);
					menuLevel.top = 0;
					menuLevel.selected = 0;
				}
				contents = getDirContents(extension);
				showFileFolder (contents);
			} else {
				filename = contents[menuLevel.selected].filename.c_str();
				break;
			}
		} else if (pressed == BUTTON_BACK) {
			chdir ("..");
			if (!menuLevelStack.empty()) {
				menuLevel = menuLevelStack.top();
				menuLevelStack.pop();
			} else {
				menuLevel.top = 0;
				menuLevel.selected = 0;
			}
			contents = getDirContents(extension);
			showFileFolder (contents);
		} else if (pressed == BUTTON_PAGE_UP) {
			menuLevel.selected -= MENU_PAGE_SCROLL;
			if (menuLevel.selected < 0) {
				menuLevel.selected = 0;
			}
			menuLevel.top -= MENU_PAGE_SCROLL;
			if (menuLevel.top < 0) {
				menuLevel.top = 0;
			}
			showFileFolder (contents);
		} else if (pressed == BUTTON_PAGE_DOWN) {
			menuLevel.selected += MENU_PAGE_SCROLL;
			if (menuLevel.selected > (int)contents.size()-1) {
				menuLevel.selected = (int)contents.size()-1;
			}
			menuLevel.top += MENU_PAGE_SCROLL;
			if (menuLevel.top > (int)contents.size()-MENU_NUM_ITEMS) {
				menuLevel.top = (int)contents.size()-MENU_NUM_ITEMS;
			}
			if (menuLevel.top < 0) {
				menuLevel.top = 0;
			}
			showFileFolder (contents);
		} 
		setCursorPosition ((MENU_FIRST_ROW + menuLevel.selected - menuLevel.top) * TOUCH_GRID_SIZE);
		setScrollbarPosition (menuLevel.selected, (int)contents.size()-1);
		getcwd (cwd, PATH_MAX);
		showMessage (TEXT_INFO, "%s\n%s", cwd, contents[menuLevel.selected].filename.c_str());
	} while (!(pressed == BUTTON_EXIT));

	delete [] cwd;

	clearMessage ();
	clearFolderBackground();
	showCursor (false);
	showScrollbar (false);
	subText->clearText ();
	
	return filename;
}


#ifdef DEMO
struct DEMO_STUFF 
{
	const char name[40];
	int icon;
};

void UserInterface::demo (void) 
{
	const DEMO_STUFF demoMenu[] =  { 
		{"Track Hacks (WiFi)", BUTTON_BG_FOLDER}, 
		{"Shy Guy Codes", BUTTON_BG_FOLDER},
		{"Press Y for safe DC", BUTTON_BG_ON},
		{"Speed Codes", BUTTON_BG_FOLDER},
		{"WiFi Win Loss Modifier", BUTTON_BG_FOLDER},
		{"Item Codes: Always have...", BUTTON_BG_FOLDER},
		{"ALWAYS USE CART", BUTTON_BG_FOLDER},
		{"Always Play as CHARACTER", BUTTON_BG_FOLDER},
		{"CRAZY SOUNDS", BUTTON_BG_OFF},
		{"Always Play on an UNTEXTURED track", BUTTON_BG_OFF}
	};
	
	int menuLength;
	
	menuLength = sizeof(demoMenu) / sizeof (DEMO_STUFF);
	
	subText->clearText(0, 0, MENU_LAST_ROW, MENU_LAST_COL);
	
	showScrollbar(true);
	showCursor (true);
	for (int col = 0; col < GO_BUTTON_WIDTH; col++) {
		for (int row = 0; row  < GO_BUTTON_HEIGHT; row++) {
			putGuiTile (GO_BUTTON_OFFSET + col + row * GO_BUTTON_WIDTH, row + GO_BUTTON_ROW, col + GO_BUTTON_COL, GO_BUTTON_PALETTE, false);
		}
	}

	setCursorPosition ((MENU_FIRST_ROW + menuLength - menuLevel.top - 1) * TOUCH_GRID_SIZE);
	setScrollbarPosition (menuLength - 1, menuLength - 1);
	showMessage (TEXT_TITLE, "Mario Kart DS (USA)");

	for (int curItem = 0; curItem < menuLength; curItem++) {
		subText->putText (demoMenu[curItem].name, (MENU_FIRST_ROW + curItem) * 2,
			MENU_FIRST_COL, (MENU_FIRST_ROW + curItem) * 2, MENU_LAST_COL, (MENU_FIRST_ROW + curItem) * 2, MENU_FIRST_COL);
		
		putButtonBg ((BUTTON_BG_OFFSETS)demoMenu[curItem].icon, (MENU_FIRST_ROW + curItem) * 2);

		showMessage (demoMenu[curItem].name);
	}
}
#endif

// Sprite
int Sprite::getSpriteNum (void)
{
	static int nextNum = 0;
	return nextNum++;
}

void Sprite::init (void)
{
	for (int i = 0; i < 128; i++) {
		OAM_SUB[i*4] = (2<<8);
	}
}

Sprite::Sprite (int width, int height, const u16* spriteData, const u16* paletteData)
{
	u16 size_attrib;
	u16 shape_attrib;
	
	int spriteDataOffset;
	
	num = getSpriteNum();
	
	spriteDataOffset = num * 64; // Assume 64 tiles per sprite
	
	for (int i = 0; i < PALETTE_SIZE; i++) {
		SPRITE_PALETTE_SUB[PALETTE_SIZE * num + i] = paletteData[i] | 0x8000;
	}
	vramcpy (SPRITE_GFX_SUB + TILE_SIZE / sizeof(u16) * spriteDataOffset, spriteData, 64 * TILE_SIZE);	// 64 tiles per sprite
	
	if (width == height) {
		shape_attrib = ATTR0_SQUARE;
		if (width >= 64) {
			size_attrib = ATTR1_SIZE_64;
		} else if (width >= 32) {
			size_attrib = ATTR1_SIZE_32;
		} else if (width >= 16) {
			size_attrib = ATTR1_SIZE_16;
		} else {
			size_attrib = ATTR1_SIZE_8;
		}
	} else if (width > height) {
		shape_attrib = ATTR0_WIDE;
		if (width >= 64) {
			size_attrib = ATTR1_SIZE_64;
		} else if ((width >= 32) && (height >= 16)) {
			size_attrib = ATTR1_SIZE_32;
		} else if (width >= 32) {
			size_attrib = ATTR1_SIZE_16;
		} else {
			size_attrib = ATTR1_SIZE_8;
		}
	} else {
		shape_attrib = ATTR0_TALL;
		if (height >= 64) {
			size_attrib = ATTR1_SIZE_64;
		} else if ((height >= 32) && (width >= 16)) {
			size_attrib = ATTR1_SIZE_32;
		} else if (height >= 32) {
			size_attrib = ATTR1_SIZE_16;
		} else {
			size_attrib = ATTR1_SIZE_8;
		}
	}
	
	attrib0 = ATTR0_NORMAL | ATTR0_TYPE_NORMAL | ATTR0_COLOR_16 | shape_attrib; 
	attrib1 = size_attrib;
	attrib2 = spriteDataOffset | ATTR2_PALETTE(num);
	
	showSprite (false);
	setPosition(0,0);
}

void Sprite::showSprite (bool visible)
{
	this->visible = visible;
	attrib0 =  (attrib0 & ~(3<<8)) | (visible ? 0 : (2<<8));
	updateAttribs();
}

void Sprite::setPosition (int top, int left)
{
	this->top = top;
	this->left = left;
	updateAttribs();
}

void Sprite::updateAttribs (void) 
{
	OAM_SUB[num*4 + 0] = attrib0 | (top  & 0xFF);
	OAM_SUB[num*4 + 1] = attrib1 | (left & 0x1FF);
	OAM_SUB[num*4 + 2] = attrib2;
}

