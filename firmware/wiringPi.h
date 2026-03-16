// =============================================================================
//  wiringPi.h  --  IntelliSense stub for Windows/macOS development
//
//  This file is a STUB only.  It exists so that the firmware source files
//  can be edited with full IntelliSense on a non-Pi machine.
//
//  On the Raspberry Pi, the real WiringPi header is installed via:
//    sudo dpkg -i wiringpi_*_arm64.deb
//  and lives at /usr/local/include/wiringPi.h
//
//  DO NOT deploy this file to the Pi — the real header takes precedence
//  because /usr/local/include is searched before the project directory.
// =============================================================================

#ifndef WIRINGPI_STUB_H
#define WIRINGPI_STUB_H

// Pin mode constants
#define INPUT   0
#define OUTPUT  1
#define PWM_OUTPUT 2

// Digital level constants
#define LOW     0
#define HIGH    1

// Pull-up/down constants
#define PUD_OFF  0
#define PUD_DOWN 1
#define PUD_UP   2

#ifdef __cplusplus
extern "C" {
#endif

// Core setup — must be called before any other WiringPi function
int  wiringPiSetup(void);

// GPIO control
void pinMode      (int pin, int mode);
void digitalWrite (int pin, int value);
int  digitalRead  (int pin);
void pullUpDnControl(int pin, int pud);

// PWM
void pwmWrite(int pin, int value);

// Timing
void delay      (unsigned int howLong);
void delayMicroseconds(unsigned int howLong);

#ifdef __cplusplus
}
#endif

#endif // WIRINGPI_STUB_H
