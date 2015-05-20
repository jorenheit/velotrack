/* Copyright 2015 Joren Heit
   
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published
   by the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "TimerOne.h"
#define RECORD_TIME 6000 // milliseconds
#define RECORD_INTERVAL 2000 // microseconds
#define RECORD_DELAY 500 // steps

enum Code
{
    CONFIRM_MEASUREMENT = 0xCF,
    DEVICE_RUNNING = 0xCE,
};

// Function declarations
template <byte channel> void handler();
void record();
void transfer();
void start();
void stop(byte code = 0);
void reset();
void identify();
void about();
void takeAction(byte code);

// To calculate the number of samples in the buffer (at compile-time)
constexpr uint32_t number_of_samples(uint32_t time, uint32_t dt)
{
    return (1000 * time) / dt;
} 

// Buffer to hold measurement results
int buf[number_of_samples(RECORD_TIME, RECORD_INTERVAL)]{};

// Interrupt pins
byte const pinA = 2; // INT0
byte const pinB = 3; // INT1

enum Channels: byte 
{ 
    A = 0, 
    B = 1,
};

// Notification LED pins
byte const readyLED = 5;
byte const runningLED = 4;
byte const onoffLED = 6;

// Transition table for fast lookup
constexpr char transitionTable[4][4] = 
{ 
    {0, 1, -1, 0},
    {-1, 0, 0, 1},
    {1, 0, 0, -1},
    {0, -1, 1, 0}
};

// Everything used by interrupts
volatile bool running = false;
volatile byte state = 0;
volatile int count = 0;
volatile int bufIdx = 0;

// Interrupt handler for the encoder
template <byte channel>
void handler() // ISR
{
    static constexpr byte pinBit[2] = {4, 5};

    byte newState = (PINE & (1 << pinBit[channel])) 
        ? state | (1 << channel)
        : state & ~(1 << channel);

    count += transitionTable[state][newState];
    state = newState;
}

// Interrupt handler for the timer
void record() // ISR
{
    buf[bufIdx++] = count;
    count = 0;

    if (bufIdx == sizeof(buf) / sizeof(decltype(buf[0])))
        stop(CONFIRM_MEASUREMENT);
}

// Start a measurement
void start()
{
    if (running)
        return;

    reset();

    attachInterrupt(A, handler<A>, CHANGE);
    attachInterrupt(B, handler<B>, CHANGE);

    // Ready: wait for skater to start
    digitalWrite(readyLED, HIGH);
    while (count < RECORD_DELAY) {}

    // Passed the threshold: start recording
    count = 0;
    running = true;
    Timer1.resume();

    digitalWrite(readyLED, LOW);
    digitalWrite(runningLED, HIGH);
}

// Stop the measurement
void stop(byte code)
{
    if (!running)
        return;
  
    Timer1.stop();
    running = false;
    digitalWrite(runningLED, LOW);

    detachInterrupt(A);
    detachInterrupt(B);
    
    if (code)
        Serial.write(code);
}

// Reset everything
void reset()
{
    if (running)
        stop();

    count = 0;
    bufIdx = 0;
    state = 0;
    memset(buf, 0, sizeof(buf));
}

void transfer()
{
    if (running)
        return;
  
    // write buffer over serial line
    Serial.write(reinterpret_cast<byte*>(&buf[0]), sizeof(buf));
}

// Interaction with PC over serial connection
void takeAction(byte code)
{
    enum Actions
    {
        START_MEASUREMENT = 0,
        STOP_MEASUREMENT = 1,
        TRANSFER_BUFFER = 2,
    };

    switch (code)
    {
    case START_MEASUREMENT:
        start();
        break;

    case STOP_MEASUREMENT:
        stop();
        break;
        
    case TRANSFER_BUFFER:
        transfer();
        break;

    default:
        break;
    }
}

// setup + loop
void setup() 
{
    Serial.begin(9600);

    // Interrupt pins
    pinMode(pinA, INPUT);
    pinMode(pinB, INPUT);
    
    // Notification LED's
    pinMode(readyLED, OUTPUT);
    pinMode(runningLED, OUTPUT);
    pinMode(onoffLED, OUTPUT);
    digitalWrite(readyLED, LOW);
    digitalWrite(runningLED, LOW);
    digitalWrite(onoffLED, HIGH);

    // Recording Timer
    Timer1.initialize(RECORD_INTERVAL);
    Timer1.attachInterrupt(record);
    Timer1.stop();

    attachInterrupt(A, handler<A>, CHANGE);
    attachInterrupt(B, handler<B>, CHANGE);
    
    Serial.write(DEVICE_RUNNING);
}

void loop() 
{
    if (Serial.available())
        takeAction(Serial.read());
}
