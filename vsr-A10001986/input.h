/*
 * -------------------------------------------------------------------
 * Voltage Systems Regulator
 * (C) 2024 Thomas Winischhofer (A10001986)
 * https://github.com/realA10001986/VSR
 * https://vsr.out-a-ti.me
 *
 * Pushwheel_I2C Class, VSRButton Class: I2C-Keypad and Button handling
 * 
 * -------------------------------------------------------------------
 * License: MIT
 * 
 * Permission is hereby granted, free of charge, to any person 
 * obtaining a copy of this software and associated documentation 
 * files (the "Software"), to deal in the Software without restriction, 
 * including without limitation the rights to use, copy, modify, 
 * merge, publish, distribute, sublicense, and/or sell copies of the 
 * Software, and to permit persons to whom the Software is furnished to 
 * do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be 
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, 
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF 
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. 
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY 
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, 
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE 
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * -------------------------------------------------------------------
 */

#ifndef _VSRINPUT_H
#define _VSRINPUT_H

#include <Wire.h>

typedef enum {
    VSRBUS_IDLE,
    VSRBUS_PRESSED,
    VSRBUS_HOLD,
    VSRBUS_RELEASED,
    VSRBUS_HOLDEND
} ButState;

struct ButStruct {
    ButState      bState;
    unsigned long startTime;
};

/*
 * VSRButton class
 */

class VSRButton {
  
    public:
        VSRButton();

        void begin(const int pin, const boolean activeLow = true, const bool pullupActive = true);
      
        void setTicks(const int debounceTs, const int lPressTs);
      
        void attachPressDown(void (*newFunction)(int, ButState));       // pressed down
        void attachPressEnd(void (*newFunction)(int, ButState));        // released after "press" (=short)
        void attachLongPressStart(void (*newFunction)(int, ButState));  // "Hold" started (pressed > holdTime)
        void attachLongPressStop(void (*newFunction)(int, ButState));   // released after "hold" (=long)

        void scan(int idx = 0);

    private:

        void reset(void);
        void transitionTo(ButState nextState);

        void (*_pressDownFunc)(int, ButState) = NULL;
        void (*_pressEndFunc)(int, ButState) = NULL;
        void (*_longPressStartFunc)(int, ButState) = NULL;
        void (*_longPressStopFunc)(int, ButState) = NULL;

        int _pin;
        
        unsigned int _debounceTicks = 50;
        unsigned int _longPressTicks = 800;
      
        int _buttonPressed;
      
        ButState _state     = VSRBUS_IDLE;
        ButState _lastState = VSRBUS_IDLE;
      
        unsigned long _startTime = 0;
        bool    _wasPressed = false;
};

/*
 * Pushwheel_I2C class 
 * (For pushwheels and buttons)
 */

// Hardware types
enum {
   PW_MCP23017 = 0
};

class Pushwheel_I2C {

    public:

        Pushwheel_I2C(int numTypes, uint8_t addrArr[]);

        void begin(unsigned int scanInterval, unsigned int holdTime, void (*myDelay)(unsigned long));

        void addEventListener(void (*listener)(int, ButState));

        bool scanControls();

        void getValues(int& num1, int& num2, int& num3);

    private:

        bool     doScan();

        int8_t   getPinWheelVal(int idx, uint16_t pins);
        
        void     advanceState(int idx, bool kstate);
        void     transitionTo(int idx, ButState nextState);

        void     (*_butEventListener)(int, ButState);

        void     prepareRead(uint16_t regno);
        uint16_t read16(uint16_t regno);
        uint8_t  read8(uint16_t regno);
        void     write16(uint16_t regno, uint16_t value);
        void     write8(uint16_t regno, uint8_t value);

        int     _numTypes = 0;
        uint8_t _addrArr[4*2];    // up to 4 hw types fit here
        int8_t  _st = -1;

        unsigned int  _scanInterval;
        unsigned int  _holdTime;
        
        uint8_t       _address;

        unsigned long _scanTime = 0;

        uint16_t      _pinMask = 0;

        int8_t        _pushWheel[3];

        ButStruct     _but[3];

        // Ptr to custom delay function
        void (*_customDelayFunc)(unsigned long) = NULL;

        #ifndef VSR_BUTTONS_I2C
        VSRButton     _vsrbutton[3];
        #endif
};

#endif
