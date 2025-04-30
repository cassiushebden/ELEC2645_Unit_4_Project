#ifndef STARTUPSCREEN_H
#define STARTUPSCREEN_H

#include "N5110.h"

class StartupScreen {
public:
    StartupScreen(N5110 &lcd);  // Constructor that only takes the LCD reference
    void show();  // Method to show the startup screen
    
private:
    N5110 &_lcd;  // LCD display object
};

#endif
