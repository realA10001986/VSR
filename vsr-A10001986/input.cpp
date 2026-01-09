/*
 * -------------------------------------------------------------------
 * Voltage Systems Regulator
 * (C) 2024-2026 Thomas Winischhofer (A10001986)
 * https://github.com/realA10001986/VSR
 * https://vsr.out-a-ti.me
 *
 * Pushwheel_I2C Class, VSRButton Class: I2C-Pushwheel and Button handling
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
 * -------------------------------------------------------------------
 */

#include "vsr_global.h"

#include <Arduino.h>

#include "input.h"

// MCP23017 registers
#define IOX_IODIRA  0x00
#define IOX_IOPOLA  0x02
#define IOX_GPINTA  0x04
#define IOX_DEFVALA 0x06
#define IOX_INTCONA 0x08
#define IOX_IOCON   0x0a
#define IOX_PUA     0x0c
#define IOX_GPIOA   0x12
#define IOX_OLATA   0x14

#define OPEN    false
#define CLOSED  true

static void defaultDelay(unsigned long mydelay)
{
    delay(mydelay);
}

/*
 * Pushwheel_I2C class (for pushwheels and buttons)
 */

Pushwheel_I2C::Pushwheel_I2C(int numTypes, uint8_t addrArr[])
{
    _numTypes = min(4, numTypes);

    for(int i = 0; i < _numTypes * 2; i++) {
        _addrArr[i] = addrArr[i];
    }    

    _scanInterval = 50;
    _holdTime = 500;

    _customDelayFunc = defaultDelay;

    // Init the three buttons
    for(int i = 0; i < 3; i++) {
        _but[i].bState = VSRBUS_IDLE;
    }
}

// Initialize I2C
void Pushwheel_I2C::begin(unsigned int scanInterval, unsigned int holdTime, void (*myDelay)(unsigned long))
{
    bool foundSt = false;
    
    _scanInterval = scanInterval;
    _holdTime = holdTime;

    for(int i = 0; i < _numTypes * 2; i += 2) {

        _address = _addrArr[i];

        Wire.beginTransmission(_address);
        if(!Wire.endTransmission(true)) {
        
            switch(_addrArr[i+1]) {
            case PW_MCP23017:
                foundSt = true;
                break;
            }

            if(foundSt) {
                _st = _addrArr[i+1];
    
                #ifdef VSR_DBG
                const char *tpArr[8] = { "MCP23017", "", "", "" };
                Serial.printf("Pushwheel/Button driver: Detected %s\n", tpArr[_st]);
                #endif
    
                break;
            }

        }
    }

    switch(_st) {
    case PW_MCP23017:

        // Hardware config:
        // A0-A2: [out] pushwheel coms
        // A3-B4: [in]  pushwheels positions 0-9

        // _pinMask: 1 = input, 0 = output
        _pinMask = 0b1111100011111111;
                
        // Set BANK to 0, all others to default
        write8(IOX_IOCON, 0b00000000);
        write8(IOX_IOCON+1, 0b00000000);
    
        // A0-A2 are output
        // A3-B7 are input (10x from push wheel; 3x from buttons)
        write16(IOX_IODIRA, _pinMask);
    
        // Config pull-ups for inputs
        write16(IOX_PUA, _pinMask);
    
        write16(IOX_IOPOLA, 0x0000);  // Polarity: Pin state = bit state
        break;
     default:
        #ifdef VSR_DBG
        Serial.println("Pushwheel/button driver hardware not found");
        #endif
        break;
    }

    #ifdef VSR_DBG
    Serial.println("Initializing direct GPIO buttons");
    #endif

    _vsrbutton[0].begin(BUTTON1_IO_PIN, true, true);  // active low, pullup
    _vsrbutton[1].begin(BUTTON2_IO_PIN, true, true);  // active low, pullup
    _vsrbutton[2].begin(BUTTON3_IO_PIN, true, true);  // active low, pullup

    _vsrbutton[0].setTiming(50, _holdTime);
    _vsrbutton[1].setTiming(50, _holdTime);
    _vsrbutton[2].setTiming(50, _holdTime);
        
    _customDelayFunc = myDelay;
}

void Pushwheel_I2C::addEventListener(void (*listener)(int, ButState))
{
    for(int i = 0; i < 3; i++) {
        _vsrbutton[i].attachPressDown(listener);
        _vsrbutton[i].attachPressEnd(listener);
        _vsrbutton[i].attachLongPressStart(listener);
        _vsrbutton[i].attachLongPressStop(listener);
    }
}

// Scan controls and update all states
// Returns true if any of the pushwheels was changed
// Buttons are handled through event instead
bool Pushwheel_I2C::scanControls()
{
    bool stChanged = false;

    if((millis() - _scanTime) > _scanInterval) {
        stChanged = doScan();
        _scanTime = millis();
    }

    return stChanged;
}

void Pushwheel_I2C::getValues(int& num1, int& num2, int& num3)
{
    num1 = _pushWheel[0];
    num2 = _pushWheel[1];
    num3 = _pushWheel[2];
}

/*
 * Private
 */

// Hardware scan & update states
bool Pushwheel_I2C::doScan()
{
    uint16_t pinVals[3][3];
    bool     repeat, ret = false;
    int      maxRetry = 5;
    int      c, d, i;
    int8_t   i8;
    uint8_t  u8;

    do {

        repeat = false;

        for(d = 0; d < 3; d++) {

            for(c = 0; c < 3; c++) {          // 3 pushwheels

                pinVals[d][c] = 0;
                
                switch(_st) {
                case PW_MCP23017:
                    u8 = ~(1 << c);
                    // Switch current COM to OUT, others to IN
                    // (Keeping other COMs but current as OUT and 
                    // setting them HIGH overpowers the current 
                    // com on LOW; effect is that three identical
                    // digits result in 0 being read.
                    write8(IOX_IODIRA, 0b11111000 | (u8 & 0x07));
                    // Set com to LOW
                    write8(IOX_GPIOA, u8);
                    // Read input    
                    pinVals[d][c] = (read16(IOX_GPIOA) ^ _pinMask) & _pinMask;
                    break;
                // ...
                }

            }

            if(d < 2) (*_customDelayFunc)(5);

        }

        for(c = 0; c < 3; c++) {
            if((pinVals[0][c] != pinVals[1][c]) || (pinVals[0][c] != pinVals[2][c])) {
                repeat = true;
                break;
            }
        }

    } while(maxRetry-- && repeat);

    #ifdef VSR_DBG
    if(repeat || maxRetry < 4) {
        Serial.printf("pushwheel input not stable (%d)\n", 5-maxRetry);
    }
    #endif
    
    // Evaluate pushwheel positions
    i8 = _pushWheel[0];
    _pushWheel[0] = getPinWheelVal(0, pinVals[0][0]);
    ret |= (i8 != _pushWheel[0]);
    i8 = _pushWheel[1];
    _pushWheel[1] = getPinWheelVal(1, pinVals[0][1]);
    ret |= (i8 != _pushWheel[1]);
    i8 = _pushWheel[2];
    _pushWheel[2] = getPinWheelVal(2, pinVals[0][2]);
    ret |= (i8 != _pushWheel[2]);

    // Now for the buttons:
    for(i = 0; i < 3; i++) {
        _vsrbutton[i].scan(i);
    }

    return ret;
}

int8_t Pushwheel_I2C::getPinWheelVal(int idx, uint16_t pins)
{    
    uint16_t temp = 0;

    switch(_st) {
    case PW_MCP23017:
        // Convert "pins" to bits 10-0 representing digits 10-0.
        temp = ((pins << 5) | (pins >> 11)) & 0x03ff;
        break;
    }
    
    for(int i = 0, j = 1; i < 10; i++, j <<= 1) {
        if(temp & j) return i;
    }
    return -1;
}

void Pushwheel_I2C::prepareRead(uint16_t regno)
{
    Wire.beginTransmission(_address);
    Wire.write((uint8_t)(regno));
    Wire.endTransmission(false);
}

uint16_t Pushwheel_I2C::read16(uint16_t regno)
{
    uint16_t value = 0;
    size_t i2clen = 0;

    prepareRead(regno);

    i2clen = Wire.requestFrom(_address, (uint8_t)2);

    if(i2clen > 0) {
        value = (Wire.read() << 8);
        value |= Wire.read();
    }

    return value;
}

uint8_t Pushwheel_I2C::read8(uint16_t regno)
{
    uint16_t value = 0;

    prepareRead(regno);

    Wire.requestFrom(_address, (uint8_t)1);

    value = Wire.read();

    return value;
}

void Pushwheel_I2C::write16(uint16_t regno, uint16_t value)
{
    Wire.beginTransmission(_address);
    Wire.write((uint8_t)(regno));
    Wire.write((uint8_t)(value >> 8));
    Wire.write((uint8_t)(value & 0xff));
    Wire.endTransmission();
}

void Pushwheel_I2C::write8(uint16_t regno, uint8_t value)
{
    Wire.beginTransmission(_address);
    Wire.write((uint8_t)(regno));
    Wire.write((uint8_t)(value & 0xff));
    Wire.endTransmission();
}


/*
 * VSRButton class
 */

VSRButton::VSRButton()
{
}

/* pin: The pin to be used
 * activeLow: Set to true when the input level is LOW when the button is pressed, Default is true.
 * pullupActive: Activate the internal pullup when available. Default is true.
 */
void VSRButton::begin(const int pin, const bool activeLow, const bool pullupActive, const bool pulldownActive)
{
    _pin = pin;

    _buttonPressed = activeLow ? LOW : HIGH;
  
    pinMode(pin, pullupActive ? INPUT_PULLUP : (pulldownActive ? INPUT_PULLDOWN : INPUT));
}


// debounce: Number of millisec that have to pass by before a click is assumed stable
// lPress:   Number of millisec that have to pass by before a long press is detected
void VSRButton::setTiming(const int debounceDur, const int lPressDur)
{
    _debounceDur = debounceDur;
    _longPressDur = lPressDur;
}

// Register function for press-down event
void VSRButton::attachPressDown(void (*newFunction)(int, ButState))
{
    _pressDownFunc = newFunction;
}

// Register function for short press event
void VSRButton::attachPressEnd(void (*newFunction)(int, ButState))
{
    _pressEndFunc = newFunction;
}

// Register function for long press start event
void VSRButton::attachLongPressStart(void (*newFunction)(int, ButState))
{
    _longPressStartFunc = newFunction;
}

// Register function for long press stop event
void VSRButton::attachLongPressStop(void (*newFunction)(int, ButState))
{
    _longPressStopFunc = newFunction;
}

// Check input of the pin and advance the state machine
void VSRButton::scan(int idx)
{
    unsigned long now = millis();
    unsigned long waitTime = now - _startTime;
    bool active = (digitalRead(_pin) == _buttonPressed);
    
    switch(_state) {
    case VSRBUS_IDLE:
        if(active) {
            transitionTo(VSRBUS_PRESSED);
            #ifdef VSR_DBG
            Serial.printf("->PRESSED %d\n", idx);
            #endif
            _startTime = now;
        }
        break;

    case VSRBUS_PRESSED:
        if((!active) && (waitTime < _debounceDur)) {  // de-bounce
            transitionTo(_lastState);
        } else if((active) && (waitTime > _longPressDur)) {
            #ifdef VSR_DBG
            Serial.printf("->LONGPRESS %d\n", idx);
            #endif
            if(_longPressStartFunc) _longPressStartFunc(idx, VSRBUS_HOLD);
            transitionTo(VSRBUS_HOLD);
        } else {
            if(!_wasPressed) {
                if(_pressDownFunc) _pressDownFunc(idx, VSRBUS_PRESSED);
                _wasPressed = true;
            }
            if(!active) {
                transitionTo(VSRBUS_RELEASED);
                #ifdef VSR_DBG
                Serial.printf("->RELEASED %d\n", idx);
                #endif
                _startTime = now;
            }
        }
        break;

    case VSRBUS_RELEASED:
        if((active) && (waitTime < _debounceDur)) {  // de-bounce
            transitionTo(_lastState);
        } else if(!active) {
            if(_pressEndFunc) _pressEndFunc(idx, VSRBUS_RELEASED);
            reset();
        }
        break;
  
    case VSRBUS_HOLD:
        if(!active) {
            transitionTo(VSRBUS_HOLDEND);
            _startTime = now;
        }
        break;

    case VSRBUS_HOLDEND:
        if((active) && (waitTime < _debounceDur)) { // de-bounce
            transitionTo(_lastState);
        } else if(waitTime >= _debounceDur) {
            if(_longPressStopFunc) _longPressStopFunc(idx, VSRBUS_HOLDEND);
            reset();
        }
        break;

    default:
        transitionTo(VSRBUS_IDLE);
        break;
    }
}

/*
 * Private
 */

void VSRButton::reset(void)
{
    _state = VSRBUS_IDLE;
    _lastState = VSRBUS_IDLE;
    _startTime = 0;
    _wasPressed = false;
}

// Advance to new state
void VSRButton::transitionTo(ButState nextState)
{
    _lastState = _state;
    _state = nextState;
}
