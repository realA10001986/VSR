/*
 * -------------------------------------------------------------------
 * Voltage Systems Regulator
 * (C) 2024-2026 Thomas Winischhofer (A10001986)
 * https://github.com/realA10001986/VSR
 * https://vsr.out-a-ti.me
 *
 * vsrBLEDs Class:   Button LEDs
 * vsrDisplay Class: VSR 3-digit Display
 *
 * vsrBLEDs:
 * - Either GPIO on ESP32
 * - or via i2c port expander (8574/0x22)
 * 
 * vsrDisplay:
 * Via HT16K33 (addr 0x70)
 * 
 * -------------------------------------------------------------------
 * License: MIT NON-AI
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
 * In addition, the following restrictions apply:
 * 
 * 1. The Software and any modifications made to it may not be used 
 * for the purpose of training or improving machine learning algorithms, 
 * including but not limited to artificial intelligence, natural 
 * language processing, or data mining. This condition applies to any 
 * derivatives, modifications, or updates based on the Software code. 
 * Any usage of the Software in an AI-training dataset is considered a 
 * breach of this License.
 *
 * 2. The Software may not be included in any dataset used for 
 * training or improving machine learning algorithms, including but 
 * not limited to artificial intelligence, natural language processing, 
 * or data mining.
 *
 * 3. Any person or organization found to be in violation of these 
 * restrictions will be subject to legal action and may be held liable 
 * for any damages resulting from such use.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, 
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF 
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. 
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY 
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, 
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE 
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef _vsrDisplay_H
#define _vsrDisplay_H

/* vsrBLEDs Class */

enum {
    BLHW_PCF8574 = 0, // i2c 0x22
    BLHW_GPIO         // direct GPIO
};

class vsrBLEDs {

    public:

        vsrBLEDs(int numTypes, uint8_t addrArr[]);
        void begin(int numLEDs);

        void on();
        void off();

        void setNightMode(bool mode);
        bool getNightMode();
        void setNMOff(bool NMOff);

        void setStates(uint8_t states);
        uint8_t getStates();

    private:

        int     _numTypes = 0;
        uint8_t _addrArr[4*2];    // up to 4 hw types fit here
        int8_t  _hwt = -1;
        
        uint8_t _address;

        bool    _off = false;
        bool    _nightmode = false;

        uint8_t _states = 0;

        int     _nLEDs = 0;
        uint8_t _sMask = 0;

        uint8_t   _lpins[3];
};

/* vsrDisplay Class */

// The supported display types: (Selection disabled by default)
// The display is a 3-digit 7-segment display
// For 4-digit displays, there are two entries in this list, one to display
// the number left-aligned, one for right-aligned.
// The idea is to remove the 4-tube display from the pcb and connect a 3-tube 
// one. Unfortunately, the pin assignments usually do not match, which makes 
// some re-wiring necessary.
//
// The display's i2c slave address is 0x70.


#define VSR_DISP_NUM_TYPES 1

enum dispTypes : uint8_t {
    SP_CS,             // Native
};

class vsrDisplay {

    public:

        vsrDisplay(uint8_t address);
        bool begin(int dispType);
        void on(bool force = false);
        void off();

        void clearBuf();

        uint8_t setBrightness(uint8_t level, bool isInitial = false);
        uint8_t setBrightnessDirect(uint8_t level) ;
        uint8_t getBrightness();

        void setNightMode(bool mode);
        bool getNightMode();
        void setNMOff(bool NMOff);

        void show();

        void setText(const char *text);
        
        void setNumbers(int num1, int num2, int num3);
        void getNumbers(int& num1, int& num2, int& num3);

        void setNumber(int num);

        void setSpeed(int speed);

        void setTemperature(float temp);

        #ifdef VSR_DIAG
        void lampTest();
        #endif

        void clearDisplay();                    // clears display RAM
        
    private:

        uint16_t getLEDChar(uint8_t value);
        #if 0
        void directCol(int col, int segments);  // directly writes column RAM
        #endif
        void directCmd(uint8_t val);

        uint8_t _address;
        uint16_t _displayBuffer[8];

        int8_t _onCache = -1;                   // Cache for on/off
        uint8_t _briCache = 0xfe;               // Cache for brightness

        int     _curNums[3] = { -1, -1, -1 };

        uint8_t _brightness = 15;
        uint8_t _origBrightness = 15;
        bool    _nightmode = false;
        bool    _nmOff = false;
        int     _oldnm = -1;

        int8_t   _dispType = -1;
        unsigned int  _num_digs;    //      total number of digits/letters (max 4)
        unsigned int  _max_buf;     //      highest buffer position
        uint8_t  _loffs = 0;        //      offset
        const uint8_t *_bufPosArr;  //      Array of buffer positions for digits left->right
        const uint8_t *_bufShftArr; //      Array of shift values for each digit

        const uint16_t *_fontXSeg;
};

#endif
