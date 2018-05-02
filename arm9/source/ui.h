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
#include <stdarg.h>
#include "cheat.h"
#include "consoletext.h"

//#define DEMO

class Sprite;


class UserInterface
{
public:
	UserInterface (void);
	~UserInterface ();
	
	struct FileInfo {
		std::string filename;
		bool isDirectory;
	};
	
	bool TWLBoostCPU;
	
	enum TEXT_TYPE {TEXT_TITLE, TEXT_INFO};
	
	void showMessage (TEXT_TYPE textType, const char* str, ...);
	void showMessage (const char* str, ...);
	void clearMessage (TEXT_TYPE textType);
	void clearMessage (void);
	
	void cheatMenu (CheatFolder* gameCodes) {
		cheatMenu (gameCodes, NULL);
	}
	void cheatMenu (CheatFolder* gameCodes, CheatFolder* top);
	
	std::string fileBrowser (const char* extension);

#ifdef DEMO
	void demo (void) ;
#endif
	
private:
	static const int FONT_PALETTE = 1;
	
 	static const int BUTTON_PALETTE_ON = 4;
 	static const int BUTTON_PALETTE_OFF = 5;
 	static const int BUTTON_PALETTE_FILE = 6;
 	static const int BUTTON_PALETTE_FOLDER = 7;
 	enum BUTTON_BG_OFFSETS {BUTTON_BG_OFFSET = 16, BUTTON_BG_FOLDER = BUTTON_BG_OFFSET + 0, 
 		BUTTON_BG_ON = BUTTON_BG_OFFSET + 60, BUTTON_BG_OFF = BUTTON_BG_OFFSET + 120,
 		BUTTON_BG_FILE = BUTTON_BG_OFFSET + 180, BUTTON_BG_NONE = 0};

	enum MENU_CONTROLS {MENU_END1 = 0, MENU_END2 = 1, MENU_BACK = 5, MENU_TICK1 = 9, MENU_TICK2 = 10,
		MENU_CROSS1 = 14, MENU_CROSS2 = 15};

	static const int SCROLLBAR_PALETTE = 2;
 	enum SCROLLBAR_OFFSETS {SCROLLBAR_OFFSET = 4, SCROLLBAR_UP = SCROLLBAR_OFFSET + 0, SCROLLBAR_BAR = SCROLLBAR_OFFSET + 4, 
 		SCROLLBAR_DOWN = SCROLLBAR_OFFSET + 8, SCROLLBAR_BOX = SCROLLBAR_OFFSET + 12};
	
	static const int TEXTBOX_PALETTE = 3;
	enum TEXTBOX_OFFSETS {TEXTBOX_OFFSET = 1, TEXTBOX_NW = TEXTBOX_OFFSET + 0, TEXTBOX_N = TEXTBOX_OFFSET + 1, 
		TEXTBOX_NE = TEXTBOX_OFFSET + 2, TEXTBOX_W = TEXTBOX_OFFSET + 3, TEXTBOX_C = TEXTBOX_OFFSET + 4, 
		TEXTBOX_E = TEXTBOX_OFFSET + 5, TEXTBOX_SW = TEXTBOX_OFFSET + 6, TEXTBOX_S = TEXTBOX_OFFSET + 7,
		TEXTBOX_SE = TEXTBOX_OFFSET + 8};

 	static const int GO_BUTTON_PALETTE = 8;
 	static const int GO_BUTTON_OFFSET = 256;
 	 	static const int BLANK_TILE = 0;
 	
	static const int NUM_CURSORS = 4;

	// Console elements
	ConsoleText* topText;
	u16* textboxMap;
	ConsoleText* subText;

	// GUI elements
	Sprite* scrollbox;
	Sprite* cursor[NUM_CURSORS];
	u16* guiSubMap;
	int scrollboxPosition;
	
	// Menu elements
	struct MENU_LEVEL {
		int top;
		int selected;
		int bottom;
	};

	MENU_LEVEL menuLevel;
	
	void writeTextBox (TEXT_TYPE textType, const char* str, va_list args);
	void drawBox (int startRow, int startCol, int endRow, int endCol);
	void clearBox (void);
	void clearBox (int startRow, int startCol, int endRow, int endCol);
	static void wordWrap (char* str, int height, int width);
	
	void showCheatFolder (std::vector<CheatBase*> &contents);

	std::vector<UserInterface::FileInfo> getDirContents (const char* extension);
	static bool fileInfoPredicate (const FileInfo& lhs, const FileInfo& rhs);
	void showFileFolder (std::vector<UserInterface::FileInfo> &contents);

 	void putGuiTile (int val, int row, int col, int palette, bool doubleSize);
	void clearFolderBackground(void);
 	void showCursor (bool visible);
 	void setCursorPosition (int offset);
 	void putButtonBg (BUTTON_BG_OFFSETS buttonBg, int position);	void showScrollbar (bool visisble);
	void setScrollbarPosition (int offset, int listLength);
	void showGoButton (bool visible, int left, int top);
} ;

class Sprite 
{
public:
	Sprite (int width, int height, const u16* spriteData, const u16* paletteData);
	
	void showSprite (bool visible);
	void setPosition (int left, int top);

public:
	static void init (void);

private:
	int top;
	int left;
	int num;
	u16 attrib0, attrib1, attrib2;
	bool visible;
	
	void updateAttribs (void);

private:
	static int getSpriteNum (void);
} ;

extern UserInterface ui;

