/*
 * AVR-Code für Strommessung mit LTC2400
 *
 * - Bereichsumschaltung automatisch mit Hysterese oder manuell
 * - Averaging einstellbar
 * - Kommunikation über UART, 9600 bps 8N1
 * - Das Bit-Banging SPI ist ein Relikt aus einer alten Schaltung... Nicht nachmachen.
 *
 * April 2018, M. W. Bruker
 *
 */

#define F_CPU 3686400UL
#define BAUD 9600UL

#include <string.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/sleep.h>
#include <avr/pgmspace.h>
#include <util/setbaud.h>
#include <stdlib.h>
#include <util/delay.h>
#include "uart.h"

#define PORT_ADC PORTB
#define DDR_ADC DDRB
#define PIN_ADC PINB
#define PIN_ADC_CS PB2
#define PIN_ADC_MISO PB4
#define PIN_ADC_SCK PB5

#define PORT_RELAYS PORTD
#define DDR_RELAYS DDRD
#define PIN_RELAY_ENABLE PD5
#define PIN_RELAY_RANGE PD6

// pA per LSB
#define ADC_FACTOR0 0.01490116119 // 10 MOhm
#define ADC_FACTOR1 14.90116119 // 10 kOhm

// hysteresis for range switch, given in LSBs
#define RANGE0_THRESHOLD_HIGH 0x00cccccc // ~ 80% of max. value
#define RANGE1_THRESHOLD_LOW  0x00002752 // 3/4 of 0.08% of max. value

#define MAX_SAMPLES 16

const char versionString[] PROGMEM = "RIESLING 1-Kanal Autorange v0.5, M. Bruker, 2018/04/02";
unsigned char currentRange = 0;
unsigned char autoRange = 1;
unsigned char averagingSamples = 4;

ISR(PCINT0_vect)
{
	// used only to wake up from sleep state
}

inline static void adcSck()
{
	PORT_ADC |= (1 << PIN_ADC_SCK);
	_delay_loop_1(1);
	PORT_ADC &= ~(1 << PIN_ADC_SCK);
	_delay_loop_1(1);
}

int main()
{
	uart_init(UART_BAUD_SELECT(BAUD, F_CPU));
	
	DDR_ADC = (1 << PIN_ADC_CS)
		| (1 << PIN_ADC_SCK);
	PORT_ADC = 0;
	
	DDR_RELAYS |= (1 << PIN_RELAY_ENABLE)
		    | (1 << PIN_RELAY_RANGE);
	
	PORT_RELAYS |= (1 << PIN_RELAY_ENABLE);
	
	// PB4 triggers PCINT4 -> PCI0
	PCICR = (1 << PCIE0);
	PCMSK0 = (1 << PCINT4);
	
	set_sleep_mode(SLEEP_MODE_IDLE);
	sei();
	
	int32_t samples[MAX_SAMPLES];
	int32_t reading;
	for (uint8_t i = 0; i < MAX_SAMPLES; ++i)
		samples[i] = 0;
	uint8_t sampleId = 0;
	uint8_t dismissReading = 0;
	while (1) {
		int32_t number = 0;
		uint32_t mask = 0x00800000;
		
		// test EOC
		if (!(PIN_ADC & (1 << PIN_ADC_MISO))) {
			PCMSK0 = 0;
			// shift to DMY bit
			adcSck();
			// shift to SIG bit
			adcSck();
			uint8_t signBit = (PIN_ADC & (1 << PIN_ADC_MISO)) ? 1 : 0;
			// shift to EXR bit
			adcSck();
			uint8_t exrBit = (PIN_ADC & (1 << PIN_ADC_MISO)) ? 1 : 0;
			
			for (uint8_t i = 0; i < 29; ++i) {
				// 0-23: value, 24-28: sub LSBs and extra pulse to start new conversion
				adcSck();
				if (mask) {
					if (PIN_ADC & (1 << PIN_ADC_MISO))
						number |= mask;
					mask >>= 1;
				}
			}
			PCMSK0 = (1 << PCINT4);
			
			if (!signBit)
				number = (number ^ 0x00ffffff) + 1;

			if (autoRange) {
				if ((currentRange == 0) && (exrBit || (number > RANGE0_THRESHOLD_HIGH)) && signBit) {
					dismissReading = 1;
					currentRange = 1;
					PORT_RELAYS |= (1 << PIN_RELAY_RANGE);
					uart_puts("range 1\n");
					_delay_ms(10);
				} else if ((currentRange == 1) && (number < RANGE1_THRESHOLD_LOW)) {
					dismissReading = 1;
					currentRange = 0;
					PORT_RELAYS &= ~(1 << PIN_RELAY_RANGE);
					uart_puts("range 0\n");
					_delay_ms(10);
				}
			}
			
			if (currentRange == 1)
				number *= ADC_FACTOR1; // 100 pA resolution
			else
				number *= ADC_FACTOR0; // 1 pA resolution
			
			if (!signBit) {
				if (number > 200000) {
					number = 200000;
				}
				number = -number;
			} else if (exrBit) {
				number = 250000000;
			}
			samples[sampleId++] = number;
			
			if (!(sampleId % averagingSamples)) {
				sampleId = 0;
				reading = 0;
				if (!dismissReading) {
					for (uint8_t i = 0; i < averagingSamples; ++i)
						reading += samples[i];
					reading /= averagingSamples;
	
					char buffer[20];
					uart_putc('r');
					uart_putc(' ');
					ltoa(reading, buffer, 10);
					uart_puts(buffer);
					uart_putc('\n');
				} else
					dismissReading = 0;
			}
		}
	
		uint16_t c;
		while (!((c = uart_getc()) & UART_NO_DATA)) {
			if (c & UART_FRAME_ERROR)
				uart_puts("ERR Frame Error\n");
			else if (c & UART_OVERRUN_ERROR)
				uart_puts("ERR Overrun Error\n");
			else if (c & UART_BUFFER_OVERFLOW)
				uart_puts("ERR Buffer Overflow Error\n");
			else switch ((uint8_t) c) {
/*				case 'r': { // fetch current reading
					char buffer[20];
					ltoa(reading, buffer, 10);
					if (readingNegative)
						uart_putc('-');
					uart_puts(buffer);
					uart_putc('\n');
					break;
				}
*/				
				case 'a': { // autorange
					autoRange = 1;
					break;
				}
				case '0': { // range 0
					autoRange = 0;
					dismissReading = 1;
					currentRange = 0;
					PORT_RELAYS &= ~(1 << PIN_RELAY_RANGE);
					uart_puts("range 0\n");
					break;
				}
				case '1': { // range 1
					autoRange = 0;
					dismissReading = 1;
					currentRange = 1;
					PORT_RELAYS |= (1 << PIN_RELAY_RANGE);
					uart_puts("range 1\n");
					break;
				}
				case 'A': { // averaging = 1
					dismissReading = 1;
					averagingSamples = 1;
					break;
				}
				case 'B': { // averaging = 2
					dismissReading = 1;
					averagingSamples = 2;
					break;
				}
				case 'C': { // averaging = 4
					dismissReading = 1;
					averagingSamples = 4;
					break;
				}
				case 'D': { // averaging = 8
					dismissReading = 1;
					averagingSamples = 8;
					break;
				}
				case 'E': { // averaging = 16
					dismissReading = 1;
					averagingSamples = 16;
					break;
				}
				case 'v': {
					uart_putc('v');
					uart_putc(' ');
					uart_puts_p(versionString);
					uart_putc('\n');
					break;
				}
				
				default: {
				}
			}
		}
		
		sleep_mode();
	}
}
