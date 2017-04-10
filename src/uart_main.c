#include "driver_config.h"
#include "target_config.h"

#include "uart.h"
#include "string.h"
#include <stdio.h>

#include "timer32.h"
#include "gpio.h"
#include "adc.h"

// UART Send
#define bluePrint(str) UARTSend((uint8_t *) str"\n\r", strlen(str"\n\r"))

// LED Control
#define LED_PORT_R 0		// Port for red led
#define LED_BIT_R 7		// Bit on port for red led
#define LED_ON 0		// Level to set port to turn on led
#define LED_OFF 1		// Level to set port to turn off led

// Duty cycle macros
#define DUTY_CYCLE_STATIC	period*1855/2000
#define DUTY_CYCLE_FORWARD	period*1845/2000
#define DUTY_CYCLE_REVERSE	period*1864/2000

extern volatile uint32_t UARTCount;
extern volatile uint8_t UARTBuffer[BUFSIZE];

typedef enum {
	ST_UNLOCKED,
	ST_LOCKING,
	ST_LOCKED,
	ST_UNLOCKING
}state_t;

state_t state = ST_UNLOCKED;
char input;
const volatile int period = 960000;

char GetInput(void) {
	while(UARTCount == 0);
	char ret = (char) UARTBuffer[0];
	UARTCount = 0;
	return ret;
}

// -------------- States ---------------
void stateUnlocked(void) {
	UARTCount = 0;
	input = GetInput();

	if (input == '1') {
		state = ST_LOCKING;
	} else if (input == '3') { // received query
		bluePrint("2");
	}
}

void stateLocking(void) {
	setMatch_timer32PWM (1, 0, DUTY_CYCLE_FORWARD);
	for (volatile int i = 0; i < 1000000; i++);
	setMatch_timer32PWM (1, 0, DUTY_CYCLE_STATIC);
	GPIOSetValue(LED_PORT_R, LED_BIT_R, LED_ON);
	state = ST_LOCKED;
}

void stateLocked(void) {
	UARTCount = 0;
	input = GetInput();

	if (input == '2') {
		state = ST_UNLOCKING;
	} else if (input == '3') { // received query
		bluePrint("1");
	}
}

void stateUnlocking(void) {
	setMatch_timer32PWM (1, 0, DUTY_CYCLE_REVERSE);
	for (volatile int i = 0; i < 1000000; i++);
	setMatch_timer32PWM (1, 0, DUTY_CYCLE_STATIC);
	GPIOSetValue(LED_PORT_R, LED_BIT_R, LED_OFF);
	state = ST_UNLOCKED;
}

// ---------------- Main -----------------
int main (void) {
	// initializations
	UARTInit(UART_BAUD);
	GPIOInit();
	GPIOSetDir(LED_PORT_R, LED_BIT_R, 1 ); // set red LED to output
	GPIOSetValue(LED_PORT_R, LED_BIT_R, LED_OFF ); // make sure it's off

	// enable pwm with timer 1
	init_timer32PWM(1, period, MATCH0);
	setMatch_timer32PWM (1, 0, DUTY_CYCLE_STATIC);
	enable_timer32(1);
	LPC_SYSCON->SYSAHBCLKCTRL |= (1<<6);

	while (1) {
		switch (state) {
		case ST_UNLOCKED:
			stateUnlocked();
			break;
		case ST_LOCKING:
			stateLocking();
			break;
		case ST_LOCKED:
			stateLocked();
			break;
		case ST_UNLOCKING:
			stateUnlocking();
			break;
		default:
			//bluePrint("State machine error");
			state = ST_UNLOCKED;
			break;
		}
	}
}
