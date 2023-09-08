/*
 * project.c
 *
 * Main file
 *
 * Authors: Peter Sutton, Luke Kamols, Jarrod Bennett
 * Modified by Sanchit Jain
 */ 


#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>

#define F_CPU 8000000UL
#include <util/delay.h>

#include "game.h"
#include "display.h"
#include "ledmatrix.h"
#include "buttons.h"
#include "serialio.h"
#include "terminalio.h"
#include "timer0.h"

// Function prototypes - these are defined below (after main()) in the order
// given here
void initialise_hardware(void);
void start_screen(void);
void new_game(void);
void play_game(void);
void handle_game_over(void);

uint8_t seven_seg_data[6] = {6,91,79,102,109,125};
uint8_t step_count[10] = {63,6,91,79,102,109,125,7,127,111};
uint8_t dice_num[6] = {1,2,3,4,5,6};
uint8_t count = 0, count2 = 0, countp = 0;
uint8_t roll_flag = 0;
volatile uint8_t stopwatch_timing = 0;
volatile uint8_t seven_seg_cc = 0;
volatile uint8_t digits_displayed = 1;


void two_play_game(void);


/////////////////////////////// main //////////////////////////////////
int main(void) {
	// Setup hardware and call backs. This will turn on 
	// interrupts.
	initialise_hardware();
	
	// Show the splash screen message. Returns when display
	// is complete.
	start_screen();
	
	// Loop forever and continuously play the game.
	while(1) {
		new_game();
		play_game();
		handle_game_over();
	}
}

void initialise_hardware(void) {
	ledmatrix_setup();
	init_button_interrupts();
	// Setup serial port for 19200 baud communication with no echo
	// of incoming characters
	init_serial_stdio(19200,0);
	
	init_timer0();
	
		DDRA = 0xFF;
		DDRC = 0x01;
		
		
		OCR1A = 4599; /* Clock divided by 8 - count for 4500 cycles */
		TCCR1A = 0; /* CTC mode */
		TCCR1B = (1<<WGM12)|(1<<CS11);
		
		TIMSK1 = (1<<OCIE1A);
		
		TIFR1 = (1<<OCF1A);
		
		/* Set up interrupt to occur on rising edge of pin D2 (start/stop button) */
		EICRA = (1<<ISC01)|(1<<ISC00);
		EIMSK = (1<<INT0);
		EIFR = (1 <<INTF0);	
	
	
	// Turn on global interrupts
	sei();
}

void start_screen(void) {
	// Clear terminal screen and output a message
	clear_terminal();
	move_terminal_cursor(10,10);
	printf_P(PSTR("Snakes and Ladders"));
	move_terminal_cursor(10,12);
	printf_P(PSTR("CSSE2010/7201 A2 by <Sanchit Jain> - <47461688>"));
	
	// Output the static start screen and wait for a push button 
	// to be pushed or a serial input of 's'
	start_display();
	
	// Wait until a button is pressed, or 's' is pressed on the terminal
	while(1) {
		// First check for if a 's' is pressed
		// There are two steps to this
		// 1) collect any serial input (if available)
		// 2) check if the input is equal to the character 's'
		char serial_input = -1;
		if (serial_input_available()) {
			serial_input = fgetc(stdin);
		}
		// If the serial input is 's', then exit the start screen
		if (serial_input == 's' || serial_input == 'S') {
			break;
		}
		// Next check for any button presses
		int8_t btn = button_pushed();
		if (btn != NO_BUTTON_PUSHED) {
			break;
		}
	}
}

void new_game(void) {
	// Clear the serial terminal
	clear_terminal();
	
	// Initialize the game and display
	initialise_game();
	
	// Clear a button push or serial input if any are waiting
	// (The cast to void means the return value is ignored.)
	(void)button_pushed();
	clear_serial_input_buffer();
	
	count2 = 0;
}

void play_game(void) {
	
	uint32_t last_flash_time, current_time;
	uint8_t btn; // The button pushed
	uint8_t n = 0, flag = 0, count1 = 0;
	
	char srl = -1;

	
	last_flash_time = get_current_time();
	
	if (flag == 0) {
		printf_P(PSTR("\nDice not rolled yet : 0"));
		PORTA = 63;
		PORTC = seven_seg_cc;
	}
	
	if (serial_input_available()) {
		srl = fgetc(stdin);
		
		if (srl == '2') {
			two_play_game();
		}
	}
	
	// We play the game until it's over
	while(!is_game_over()) {
				
		// We need to check if any button has been pushed, this will be
		// NO_BUTTON_PUSHED if no button has been pushed
		btn = button_pushed();
		
		while(btn == BUTTON3_PUSHED && countp == 0) {
			countp++;
			btn = button_pushed();
			if(btn == BUTTON3_PUSHED) {
				countp=0;
			}
		}
		
		if (btn == BUTTON0_PUSHED) {
			// If button 0 is pushed, move the player 1 space forward
			// YOU WILL NEED TO IMPLEMENT THIS FUNCTION
			move_player_n(1);
			last_flash_time = get_current_time();
			//INCREMENTING THE NUMBER OF TURNS FOR THE PLAYER
			count2++;
		}
		
		if (btn == BUTTON1_PUSHED) {
			move_player_n(2);
			last_flash_time = get_current_time();
			//INCREMENTING THE NUMBER OF TURNS FOR THE PLAYER
			count2++;
		}
		
		if (serial_input_available()) {
			srl = fgetc(stdin);
			
			if (srl == 'd' || srl == 'D') {
				move_player(1,0);
				last_flash_time = get_current_time();
			}
			
			if (srl == 'a' || srl == 'A') {
				move_player(-1,0);
				last_flash_time = get_current_time();
			}
			
			if (srl == 'w' || srl == 'W') {
				move_player(0,1);
				last_flash_time = get_current_time();
			}
			
			if (srl == 's' || srl == 'S') {
				move_player(0,-1);
				last_flash_time = get_current_time();
			}
			
			if (srl == 'r' || srl == 'R') {
				count1++;
				stopwatch_timing ^= 1;
				digits_displayed = 1;
				//
				if (count1 == 1) {
					printf("\n\n");
					printf_P(PSTR("--------------------------------\n"));
					printf_P(PSTR("Dice STARTED ROLLING\n"));
				}
				else {
					printf("\n");
					printf_P(PSTR("Dice STOPPED ROLLING\n"));
					printf_P(PSTR("--------------------------------\n"));
				}
			}
			
			if (count1 == 2) {
				n = dice_num[(count/10)%10];
				move_player_n(n);
				printf("\n");
				printf_P(PSTR("Dice Rolled is : %d"), n);
				flag = 1;
				last_flash_time = get_current_time();
				count1 = 0;
				//INCREMENTING THE NUMBER OF TURNS FOR THE PLAYER
				count2++;
				roll_flag = 1;
			}
		}
		
		
		
		if (btn == BUTTON2_PUSHED) {
			count1++;
			stopwatch_timing ^= 1;
			digits_displayed = 1;
			n = dice_num[(count/10)%10];
			
			if (count1 == 1) {
				printf("\n\n");
				printf_P(PSTR("--------------------------------\n"));
				printf_P(PSTR("Dice STARTED ROLLING\n"));
			}
			else {
				printf("\n");
				printf_P(PSTR("Dice STOPPED ROLLING\n"));
				printf_P(PSTR("--------------------------------\n"));
			}
			
			if (count1 == 2)	{
				//n = (rand() % 6) + 1;
				move_player_n(n);
				printf("\n");
				printf_P(PSTR("Dice Rolled is : %d"), n);
				last_flash_time = get_current_time();
				count1 = 0;
				flag = 1;
				//INCREMENTING THE NUMBER OF TURNS FOR THE PLAYER
				count2++;
				roll_flag = 1;
			}
		}
		
		
		
		current_time = get_current_time();
		if (current_time >= last_flash_time + 500) {
			// 500ms (0.5 second) has passed since the last time we
			// flashed the cursor, so flash the cursor
			flash_player_cursor();
			
			// Update the most recent time the cursor was flashed
			last_flash_time = current_time;
			
		}
	}
	// We get here if the game is over.
}

void handle_game_over(void) {
	clear_terminal();
	
	if (is_game_over() == 1) {
		move_terminal_cursor(10,12);
		printf_P(PSTR("PLAYER 1 HAS WON THE GAME"));
	}
	else {
		move_terminal_cursor(10,12);
		printf_P(PSTR("PLAYER 1 HAS LOST THE GAME"));		
	}
	
	move_terminal_cursor(10,14);
	printf_P(PSTR("GAME OVER"));
	move_terminal_cursor(10,15);
	printf_P(PSTR("Press a button to start again"));
	
	char st = -1;
	
	while(1) {
		
		if (serial_input_available()) {
			st = fgetc(stdin);
			
			if (st == 's') {
				break;
			}
			
			if (st == 'S') {
				break;
			}
		}
		
		if(button_pushed() != NO_BUTTON_PUSHED) {
			break;
		}
		// wait
	}
	
}

// NOTE : THE FOLLOWING CODE IS TAKEN FROM LAB 15

ISR(INT0_vect) {
	/* Toggle whether the stopwatch is running or not */
	stopwatch_timing ^= 1;
	
	/* Ensure the digits are displayed */
	digits_displayed = 1;
}

/* This interrupt handler will get called every 10ms.
** We do two things - update out stopwatch count, and
** output to the other seven segment display digit. 
** We display seconds on the left digit; 
** tenths of seconds on the right digit.
*/
ISR(TIMER1_COMPA_vect) {
	/* If the stopwatch is running then increment time. 
	** If we've reached 60, then wrap this around to 0. 
	*/
	if(stopwatch_timing) {
		count++;
		if(count == 60) {
			count = 0;
		}
	}

	seven_seg_cc = 1 ^ seven_seg_cc;
	
	if(digits_displayed) {
		/* Display a digit */
		
// 		if (flag1 == 0) {
// 			PORTA = 63;
// 		}
		
		if(seven_seg_cc == 0) {
			/* Display rightmost digit - tenths of seconds */
			if (count == 0 && roll_flag == 0)	{
				PORTA = 63;
			}
			else {
				PORTA = seven_seg_data[(count/10)%10];
			}
		} 
		else {
			
			// LEFT SIDE OF SEVEN SEGEMENT TO PRINTED
			// START PRINTING FROM ZERO
			
			if ((count2/10)%10 == 0)	{
				PORTA = step_count[count2];
			}
			else {
				PORTA = step_count[(count2/10)%10];
			}
		}
				/* Output the digit selection (CC) bit */
		PORTC = seven_seg_cc;	
	} else {
		/* No digits displayed -  display is blank */
		PORTA = 0;
	}
}

void two_play_game(void) {
	
}