#include "StartupScreen.h"

StartupScreen::StartupScreen(N5110 &lcd) : _lcd(lcd) {}

void StartupScreen::show() {
    _lcd.clear();
    
    // Title
    _lcd.printString(" POOL GAME ", 12, 0);

    // Cue stick
    _lcd.drawLine(5, 24, 60, 24, 1);

    // Pool ball
    _lcd.drawCircle(70, 24, 3, FILL_BLACK);

    // Message
    _lcd.printString("Press Btn to", 0, 4);
    _lcd.printString("   Start", 18, 5);

    _lcd.refresh();
}
