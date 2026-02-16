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
 * - or via i2c port expander (8574/0x22; anode=VDD)
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

#include "vsr_global.h"

#include <Arduino.h>
#include <math.h>
#include "vsrdisplay.h"
#include <Wire.h>

/* vsrbleds class */

// Store i2c address
vsrBLEDs::vsrBLEDs(int numTypes, uint8_t addrArr[])
{
    _numTypes = min(4, numTypes);

    for(int i = 0; i < _numTypes * 2; i++) {
        _addrArr[i] = addrArr[i];
    }
}

void vsrBLEDs::begin(int numLEDs)
{
    bool foundSt = false;

    _nLEDs = numLEDs;
    if(_nLEDs > 3) _nLEDs = 3;

    for(int i = 0; i < _numTypes * 2; i += 2) {
        _address = _addrArr[i];

        Wire.beginTransmission(_address);
        if(!Wire.endTransmission(true)) {
        
            switch(_addrArr[i+1]) {
            case BLHW_PCF8574:
                foundSt = true;
                break;
            }

            if(foundSt) {
                _hwt = _addrArr[i+1];

                #ifdef VSR_DBG
                const char *tpArr[4] = { "PCF8574", "", "", "" };
                Serial.printf("Button LED driver: Detected %s (i2c)\n", tpArr[_hwt]);
                #endif

                break;
            }
        }
    }

    if(!foundSt) {
        #ifdef VSR_DBG
        Serial.println("No i2c button LED driver found, setting up fall-back GPIO pins");
        #endif
        
        _hwt =  BLHW_GPIO;
        _nLEDs = 3;

        _lpins[0] = BUTTON1_PWM_PIN;
        _lpins[1] = BUTTON2_PWM_PIN;
        _lpins[2] = BUTTON3_PWM_PIN;

        for(int i = 0; i < _nLEDs; i++) {
            pinMode(_lpins[i], OUTPUT);
            digitalWrite(_lpins[i], LOW);
        }
    }

    _sMask = (_nLEDs > 0) ? (1 << _nLEDs) - 1 : 0;

    setStates(0);
    on();
}

void vsrBLEDs::on()
{
    _off = false;
    setStates(_states);
}

void vsrBLEDs::off()
{
    _off = true;
    setStates(_states);
}

void vsrBLEDs::setStates(uint8_t states)
{
    
    _states = states & _sMask;
    
    switch(_hwt) {
    case BLHW_PCF8574:
        // Button LEDs are off in night mode
        Wire.beginTransmission(_address);
        Wire.write(~(_off ? 0x00 : (_nightmode ? 0x00 : _states)));
        Wire.endTransmission();
        break;
    case BLHW_GPIO:
        // Button LEDs are off in night mode
        for(int i = 0; i < _nLEDs; i++) {
            digitalWrite(_lpins[i], _off ? LOW : (_nightmode ? LOW : !!(_states & (1 << i))));
        }
        break;
    }
}

uint8_t vsrBLEDs::getStates()
{
    switch(_hwt) {
    case BLHW_PCF8574:
    case BLHW_GPIO:
    default:
        return _states;
    }
}

void vsrBLEDs::setNightMode(bool mymode)
{
    _nightmode = mymode;
    setStates(_states);
}

bool vsrBLEDs::getNightMode(void)
{
    return _nightmode;
}


/* vsrdisplay class */

// The segments' wiring to buffer bits
// This reflects the actual hardware wiring

// generic
#define S7G_T   0b00000001    // top
#define S7G_TR  0b00000010    // top right
#define S7G_BR  0b00000100    // bottom right
#define S7G_B   0b00001000    // bottom
#define S7G_BL  0b00010000    // bottom left
#define S7G_TL  0b00100000    // top left
#define S7G_M   0b01000000    // middle
#define S7G_DOT 0b10000000    // dot

static const uint16_t font7segGeneric[47] = {
    S7G_T|S7G_TR|S7G_BR|S7G_B|S7G_BL|S7G_TL,
    S7G_TR|S7G_BR,
    S7G_T|S7G_TR|S7G_B|S7G_BL|S7G_M,
    S7G_T|S7G_TR|S7G_BR|S7G_B|S7G_M,
    S7G_TR|S7G_BR|S7G_TL|S7G_M,
    S7G_T|S7G_BR|S7G_B|S7G_TL|S7G_M,
    S7G_BR|S7G_B|S7G_BL|S7G_TL|S7G_M,
    S7G_T|S7G_TR|S7G_BR,
    S7G_T|S7G_TR|S7G_BR|S7G_B|S7G_BL|S7G_TL|S7G_M,
    S7G_T|S7G_TR|S7G_BR|S7G_TL|S7G_M,
    S7G_T|S7G_TR|S7G_BR|S7G_BL|S7G_TL|S7G_M,
    S7G_BR|S7G_B|S7G_BL|S7G_TL|S7G_M,
    S7G_T|S7G_B|S7G_BL|S7G_TL,
    S7G_TR|S7G_BR|S7G_B|S7G_BL|S7G_M,
    S7G_T|S7G_B|S7G_BL|S7G_TL|S7G_M,
    S7G_T|S7G_BL|S7G_TL|S7G_M,
    S7G_T|S7G_BR|S7G_B|S7G_BL|S7G_TL,
    S7G_TR|S7G_BR|S7G_BL|S7G_TL|S7G_M,
    S7G_BL|S7G_TL,
    S7G_TR|S7G_BR|S7G_B|S7G_BL,
    S7G_T|S7G_BR|S7G_BL|S7G_TL|S7G_M,
    S7G_B|S7G_BL|S7G_TL,
    S7G_T|S7G_BR|S7G_BL,
    S7G_T|S7G_TR|S7G_BR|S7G_BL|S7G_TL,
    S7G_T|S7G_TR|S7G_BR|S7G_B|S7G_BL|S7G_TL,
    S7G_T|S7G_TR|S7G_BL|S7G_TL|S7G_M,
    S7G_T|S7G_TR|S7G_B|S7G_TL|S7G_M,
    S7G_T|S7G_TR|S7G_BL|S7G_TL,
    S7G_T|S7G_BR|S7G_B|S7G_TL|S7G_M,
    S7G_B|S7G_BL|S7G_TL|S7G_M,
    S7G_TR|S7G_BR|S7G_B|S7G_BL|S7G_TL,
    S7G_TR|S7G_BR|S7G_B|S7G_BL|S7G_TL,
    S7G_TR|S7G_B|S7G_TL,
    S7G_TR|S7G_BR|S7G_BL|S7G_TL|S7G_M,
    S7G_TR|S7G_BR|S7G_B|S7G_TL|S7G_M,
    S7G_T|S7G_TR|S7G_B|S7G_BL|S7G_M,
    S7G_DOT,
    S7G_M,        // 37 minus
    S7G_T,        // 38 coded as  1
    S7G_TR,       // 39           2
    S7G_BR,       // 40           3
    S7G_B,        // 41           4
    S7G_BL,       // 42           5
    S7G_TL,       // 43           6
    S7G_T|S7G_B,  // 44           7
    S7G_T|S7G_BR|S7G_B|S7G_BL|S7G_M,  // 45 8
    S7G_BR|S7G_B|S7G_BL|S7G_M,        // 46 9
};

static const struct dispConf {
    uint8_t  num_digs;       //   total number of digits/letters (max 4)
    uint8_t  max_bufPos;     //   highest buffer position to update
    uint8_t  loffset;        //   Offset from left (0 or 1) if display has more than 3 digits
    uint8_t  bufPosArr[4];   //   The buffer positions of each of the digits from left to right
    uint8_t  bufShftArr[4];  //   Shift-value for each digit from left to right
    const uint16_t *fontSeg; //   Pointer to font
} displays[VSR_DISP_NUM_TYPES] = {
  { 3, 3, 0, { 0, 1, 3 }, { 0, 0, 0 }, font7segGeneric },  // Native
};

// Store i2c address
vsrDisplay::vsrDisplay(uint8_t address)
{
    _address = address;
}

// Start the display
bool vsrDisplay::begin(int dispType)
{
    bool ret = true;

    // Check for display on i2c bus
    Wire.beginTransmission(_address);
    if(Wire.endTransmission(true)) {

        ret = false;

    } else {

        _dispType = dispType;
        _num_digs = displays[dispType].num_digs;
        _max_buf = displays[dispType].max_bufPos;
        _loffs = displays[dispType].loffset;
        _bufPosArr = displays[dispType].bufPosArr;
        _bufShftArr = displays[dispType].bufShftArr;

        _fontXSeg = displays[dispType].fontSeg;

    }

    directCmd(0x20 | 1); // turn on oscillator

    clearBuf();          // clear buffer
    setBrightness(15);   // setup initial brightness
    clearDisplay();      // clear display RAM
    on();                // turn it on

    return ret;
}

// Turn on the display
void vsrDisplay::on(bool force)
{
    if(_onCache > 0) return;
    if(force || !_nightmode || !_nmOff) {
        directCmd(0x80 | 1);
        _onCache = 1;
        _briCache = 0xfe;
    }
}

// Turn off the display
void vsrDisplay::off()
{   
    if(!_onCache) return;
    directCmd(0x80);
    _onCache = 0;
}

// Clear the buffer
void vsrDisplay::clearBuf()
{
    // must call show() to actually clear display

    for(int i = 0; i < 8; i++) {
        _displayBuffer[i] = 0;
    }
}

// Set display brightness
// Valid brighness levels are 0 to 15. Default is 15.
// 255 sets it to previous level
uint8_t vsrDisplay::setBrightness(uint8_t level, bool isInitial)
{
    if(level == 255)
        level = _brightness;    // restore to old val

    _brightness = setBrightnessDirect(level);

    if(isInitial)
        _origBrightness = _brightness;

    return _brightness;
}

uint8_t vsrDisplay::setBrightnessDirect(uint8_t level)
{
    if(level > 15)
        level = 15;

    if(level != _briCache) {
        directCmd(0xE0 | level);  // Dimming command
        _briCache = level;
    }

    return level;
}

uint8_t vsrDisplay::getBrightness()
{
    return _brightness;
}

void vsrDisplay::setNightMode(bool mymode)
{
    _nightmode = mymode;
    show();
}

bool vsrDisplay::getNightMode(void)
{
    return _nightmode;
}

void vsrDisplay::setNMOff(bool NMOff)
{
    _nmOff = NMOff;
}

#ifdef VSR_DIAG
void vsrDisplay::lampTest()
{
    if(_dispType >= 0) {
        Wire.beginTransmission(_address);
        Wire.write(0x00);  // start address
        for(int i = 0; i < 8; i++) {
            Wire.write(0xff);
            Wire.write(0xff);
        }
        Wire.endTransmission();
    }
}
#endif

// Show data in display --------------------------------------------------------


// Show the buffer
void vsrDisplay::show()
{
    int i;

    if(_nightmode) {
        if(_nmOff) {
            off();
            _oldnm = 1;
            return;
        } else {
            if(_oldnm < 1) {
                setBrightness(0);
                _oldnm = 1;
            }
        }
    } else if(!_nmOff) {
        if(_oldnm > 0) {
            setBrightness(_origBrightness);
        }
        _oldnm = 0;
    }

    if(_dispType >= 0) {
        Wire.beginTransmission(_address);
        Wire.write(0x00);  // start address
    
        for(i = 0; i <= _max_buf; i++) {
            Wire.write(_displayBuffer[i] & 0xFF);
            Wire.write(_displayBuffer[i] >> 8);
        }
    
        Wire.endTransmission();
    }
    
    if(_nmOff && (_oldnm > 0)) on();
    if(_nmOff) _oldnm = 0;
}


// Set data in buffer --------------------------------------------------------


// Write given text to buffer
void vsrDisplay::setText(const char *text)
{
    int idx = 0, pos = 0;
    int temp = 0;

    clearBuf();

    if(_dispType >= 0) {
        while(text[idx] && (pos < _num_digs)) {
            temp = getLEDChar(text[idx]) << (*(_bufShftArr + pos));
            idx++;
            if(text[idx] == '.') {
                temp |= (getLEDChar('.') << (*(_bufShftArr + pos)));
                idx++;
            }
            _displayBuffer[*(_bufPosArr + pos)] |= temp;
            pos++;
        }
    }
}

// Write given number to buffer
void vsrDisplay::setNumbers(int num1, int num2, int num3)
{
    uint16_t b;
    int s = _loffs;

    clearBuf();

    _curNums[0] = num1;
    _curNums[1] = num2;
    _curNums[2] = num3;

    if(_dispType >= 0) {
        for(int i = 0; i < 3; i++) {
            if(_curNums[i] < 0) {
                b = *(_fontXSeg + 37);
            } else if(_curNums[i] > 9) {
                b = *(_fontXSeg + ('H' - 'A' + 10));
            } else {
                b = *(_fontXSeg + (_curNums[i] % 10));
            }
            _displayBuffer[*(_bufPosArr + s)] |= (b << (*(_bufShftArr + s)));
            s++;
        }
    }
}

void vsrDisplay::setNumber(int num)
{
    if(num < 0) {
        setNumbers(-1, -1, -1);
    } else {
        int num1, num2, num3;
        if(num > 999) {
            char buf[8];
            sprintf(buf, "%.2f", (float)num/1000.0);
            setText(buf);
        } else {
            num1 = num / 100;
            num  %= 100;
            num2 = num / 10;
            num3 = num % 10;
            setNumbers(num1, num2, num3);
        }
    }
}

void vsrDisplay::setSpeed(int speed)
{
    if(speed < 0) {
        setText("---.");
    } else {
        char buf[4];
        sprintf(buf, "%3d.", speed);
        setText(buf);
    }
}

void vsrDisplay::setTemperature(float temp)
{
    char buf[8];
    int t;
    const char *myNan = "---";

    if(isnan(temp))          setText(myNan);
    else if(temp <= -100.0f) setText("LOW");
    else if(temp >= 1000.0f) setText("HI");
    else if(temp >= 100.0f || temp <= -10.0f) {
        t = (int)roundf(temp);
        sprintf(buf, "%d", t);
        setText(buf);
    } else {
        sprintf(buf, "%.1f", temp);
        setText(buf);
    }
}

// Query data ------------------------------------------------------------------


void vsrDisplay::getNumbers(int& num1, int& num2, int& num3)
{
    num1 = _curNums[0];
    num2 = _curNums[1];
    num3 = _curNums[2];
}

// Private functions ###########################################################

// Returns bit pattern for provided character
uint16_t vsrDisplay::getLEDChar(uint8_t value)
{
    if(value >= '0' && value <= '9') {
        return *(_fontXSeg + (value - '0'));
    } else if(value >= 'A' && value <= 'Z') {
        return *(_fontXSeg + (value - 'A' + 10));
    } else if(value >= 'a' && value <= 'z') {
        return *(_fontXSeg + (value - 'a' + 10));
    } else if(value == '.') {
        return *(_fontXSeg + 36);
    } else if(value == '-') {
        return *(_fontXSeg + 37);
    } else if(value >= 1 && value <= 9) {
        return *(_fontXSeg + 38 + (value - 1));
    } 

    return 0;
}

#if 0 // Unused currently
// Directly write to a column with supplied segments
// (leave buffer intact, directly write to display)
void vsrDisplay::directCol(int col, int segments)
{
    if(_dispType >= 0) {
        Wire.beginTransmission(_address);
        Wire.write(col * 2);  // 2 bytes per col * position
        Wire.write(segments & 0xFF);
        Wire.write(segments >> 8);
        Wire.endTransmission();
    }
}
#endif

// Directly clear the display
void vsrDisplay::clearDisplay()
{
    if(_dispType >= 0) {
        Wire.beginTransmission(_address);
        Wire.write(0x00);  // start address
    
        for(int i = 0; i <= _max_buf; i++) {
            Wire.write(0x00);
            Wire.write(0x00);
        }
    
        Wire.endTransmission();
    }
}

void vsrDisplay::directCmd(uint8_t val)
{
    if(_dispType >= 0) {
        Wire.beginTransmission(_address);
        Wire.write(val);
        Wire.endTransmission();
    }
}
