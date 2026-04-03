#define F_CPU 8000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// ================= LCD CONFIGURATION =================
#define LCD_DATA_PORT PORTA
#define LCD_DATA_DDR DDRA
#define LCD_CTRL_PORT PORTB
#define LCD_CTRL_DDR DDRB
#define LCD_RS PB0
#define LCD_EN PB1

// ================= KEYPAD CONFIGURATION =================
#define KEYPAD_PORT PORTD
#define KEYPAD_PIN PIND
#define KEYPAD_DDR DDRD

// Keypad Rows (Output)
#define ROW1 PD0
#define ROW2 PD1
#define ROW3 PD2
#define ROW4 PD3

// Keypad Columns (Input with Pull-up)
#define COL1 PD4
#define COL2 PD5
#define COL3 PD6
#define COL4 PD7

// Keypad Layout
const char keypad[4][4] = {
	{'7', '8', '9', '/'},
	{'4', '5', '6', '*'},
	{'1', '2', '3', '-'},
	{'C', '0', '=', '+'}
};

// ================= CALCULATOR STATES =================
typedef enum {
	CALC_INPUT_FIRST,
	CALC_INPUT_SECOND,
	CALC_SHOW_RESULT,
	CALC_ERROR
} CalcState;

// ================= GLOBAL VARIABLES =================
float first_num = 0.0;
float second_num = 0.0;
float result = 0.0;
char operation = ' ';
char display_buffer[17] = "0";
CalcState state = CALC_INPUT_FIRST;
uint8_t decimal_entered = 0;

// ================= FUNCTION PROTOTYPES =================
// LCD Functions
void LCD_Pulse(void);
void LCD_Command(unsigned char cmd);
void LCD_Data(unsigned char data);
void LCD_String(const char *str);
void LCD_Init(void);
void LCD_Clear(void);
void LCD_SetCursor(uint8_t row, uint8_t col);
void LCD_DisplayNumber(float num);
void LCD_UpdateFirstLine(void);

// Keypad Functions
void Keypad_Init(void);
char Keypad_GetKey(void);

// Calculator Functions
void Calculator_Reset(void);
void Calculator_Display(void);
void Calculator_AddDigit(char digit);
void Calculator_SetOperation(char op);
void Calculator_Calculate(void);
void Calculator_Clear(void);
void Calculator_UpdateFirstLine(void);

// ================= LCD FUNCTIONS =================
void LCD_Pulse(void) {
	LCD_CTRL_PORT |= (1 << LCD_EN);
	_delay_us(1);
	LCD_CTRL_PORT &= ~(1 << LCD_EN);
	_delay_us(50);
}

void LCD_Command(unsigned char cmd) {
	LCD_DATA_PORT = (LCD_DATA_PORT & 0x0F) | (cmd & 0xF0);
	LCD_CTRL_PORT &= ~(1 << LCD_RS);
	LCD_Pulse();
	LCD_DATA_PORT = (LCD_DATA_PORT & 0x0F) | ((cmd << 4) & 0xF0);
	LCD_Pulse();
	if(cmd == 0x01 || cmd == 0x02) _delay_ms(2);
	else _delay_us(40);
}

void LCD_Data(unsigned char data) {
	LCD_DATA_PORT = (LCD_DATA_PORT & 0x0F) | (data & 0xF0);
	LCD_CTRL_PORT |= (1 << LCD_RS);
	LCD_Pulse();
	LCD_DATA_PORT = (LCD_DATA_PORT & 0x0F) | ((data << 4) & 0xF0);
	LCD_Pulse();
	_delay_us(40);
}

void LCD_String(const char *str) {
	while(*str) LCD_Data(*str++);
}

void LCD_Init(void) {
	LCD_CTRL_DDR |= (1 << LCD_RS) | (1 << LCD_EN);
	LCD_DATA_DDR = 0xF0;
	_delay_ms(50);
	
	LCD_CTRL_PORT &= ~(1 << LCD_RS);
	LCD_DATA_PORT = 0x30; LCD_Pulse(); _delay_ms(5);
	LCD_DATA_PORT = 0x30; LCD_Pulse(); _delay_us(150);
	LCD_DATA_PORT = 0x30; LCD_Pulse(); _delay_us(150);
	LCD_DATA_PORT = 0x20; LCD_Pulse(); _delay_us(150);
	
	LCD_Command(0x28);
	LCD_Command(0x0C);
	LCD_Command(0x01);
	LCD_Command(0x06);
	_delay_ms(2);
}

void LCD_Clear(void) {
	LCD_Command(0x01);
	_delay_ms(2);
}

void LCD_SetCursor(uint8_t row, uint8_t col) {
	uint8_t address = (row == 0) ? 0x80 + col : 0xC0 + col;
	LCD_Command(address);
}

// ================= DISPLAY FORMATTING =================
void LCD_DisplayNumber(float num) {
	char buffer[17];
	char temp_buffer[17];
	int i, len, decimal_pos = -1;
	
	// Convert float to string with max 6 decimal places
	dtostrf(num, 16, 6, buffer);
	
	// Remove trailing zeros
	len = strlen(buffer);
	
	// Find decimal point
	for(i = 0; i < len; i++) {
		if(buffer[i] == '.') {
			decimal_pos = i;
			break;
		}
	}
	
	// Remove trailing zeros after decimal
	if(decimal_pos != -1) {
		i = len - 1;
		while(i > decimal_pos && buffer[i] == '0') {
			buffer[i] = '\0';
			i--;
		}
		if(buffer[i] == '.') {
			buffer[i] = '\0';
		}
	}
	
	// Remove leading spaces from dtostrf
	int start = 0;
	while(buffer[start] == ' ') start++;
	
	// Copy to temp buffer
	strcpy(temp_buffer, &buffer[start]);
	len = strlen(temp_buffer);
	
	// Right align the number on 16-char display
	LCD_SetCursor(1, 0);
	for(i = 0; i < 16; i++) {
		if(i >= 16 - len) {
			LCD_Data(temp_buffer[i - (16 - len)]);
			} else {
			LCD_Data(' ');
		}
	}
}

// ================= KEYPAD FUNCTIONS =================
void Keypad_Init(void) {
	KEYPAD_DDR |= (1 << ROW1) | (1 << ROW2) | (1 << ROW3) | (1 << ROW4);
	KEYPAD_PORT |= (1 << ROW1) | (1 << ROW2) | (1 << ROW3) | (1 << ROW4);
	
	KEYPAD_DDR &= ~((1 << COL1) | (1 << COL2) | (1 << COL3) | (1 << COL4));
	KEYPAD_PORT |= (1 << COL1) | (1 << COL2) | (1 << COL3) | (1 << COL4);
}

char Keypad_GetKey(void) {
	uint8_t row, col;
	uint8_t key_pressed = 0;
	
	for(row = 0; row < 4; row++) {
		// Set all rows high
		KEYPAD_PORT |= (1 << ROW1) | (1 << ROW2) | (1 << ROW3) | (1 << ROW4);
		
		// Set current row low
		switch(row) {
			case 0: KEYPAD_PORT &= ~(1 << ROW1); break;
			case 1: KEYPAD_PORT &= ~(1 << ROW2); break;
			case 2: KEYPAD_PORT &= ~(1 << ROW3); break;
			case 3: KEYPAD_PORT &= ~(1 << ROW4); break;
		}
		
		_delay_us(10);
		
		// Check each column
		if(!(KEYPAD_PIN & (1 << COL1))) {
			key_pressed = 1;
			col = 0;
			} else if(!(KEYPAD_PIN & (1 << COL2))) {
			key_pressed = 1;
			col = 1;
			} else if(!(KEYPAD_PIN & (1 << COL3))) {
			key_pressed = 1;
			col = 2;
			} else if(!(KEYPAD_PIN & (1 << COL4))) {
			key_pressed = 1;
			col = 3;
		}
		
		if(key_pressed) {
			_delay_ms(20); // Debounce
			// Wait for key release with timeout
			uint16_t timeout = 0;
			while(!(KEYPAD_PIN & (1 << (COL1 + col))) && timeout < 1000) {
				_delay_ms(1);
				timeout++;
			}
			return keypad[row][col];
		}
	}
	return 0;
}

// ================= CALCULATOR ENGINE =================
void Calculator_UpdateFirstLine(void) {
	char first_str[17];
	char sec_str[17];
	char line[17] = "                "; // 16 spaces
	
	LCD_SetCursor(0, 0);
	
	switch(state) {
		case CALC_INPUT_FIRST:
		LCD_String(line);
		break;
		
		case CALC_INPUT_SECOND:
		case CALC_SHOW_RESULT:
		// Format first number
		dtostrf(first_num, 10, 3, first_str);
		
		// Remove trailing zeros
		int i = strlen(first_str) - 1;
		while(i >= 0 && first_str[i] == '0') i--;
		if(i >= 0 && first_str[i] == '.') i--;
		first_str[i+1] = '\0';
		
		// Remove leading spaces
		int start = 0;
		while(first_str[start] == ' ') start++;
		
		// Copy to line
		strcpy(line, &first_str[start]);
		int pos = strlen(line);
		
		// Add operation
		if(pos < 15) {
			line[pos++] = ' ';
			line[pos++] = operation;
			line[pos] = '\0';
		}
		
		// Add second number if in result state
		if(state == CALC_SHOW_RESULT) {
			dtostrf(second_num, 8, 3, sec_str);
			
			// Remove trailing zeros
			i = strlen(sec_str) - 1;
			while(i >= 0 && sec_str[i] == '0') i--;
			if(i >= 0 && sec_str[i] == '.') i--;
			sec_str[i+1] = '\0';
			
			// Remove leading spaces
			start = 0;
			while(sec_str[start] == ' ') start++;
			
			// Add second number if space available
			if(pos + strlen(&sec_str[start]) + 1 < 16) {
				line[pos++] = ' ';
				strcpy(&line[pos], &sec_str[start]);
			}
		}
		
		LCD_String(line);
		break;
		
		case CALC_ERROR:
		LCD_String("   Error        ");
		break;
	}
}

void Calculator_Reset(void) {
	first_num = 0.0;
	second_num = 0.0;
	result = 0.0;
	operation = ' ';
	strcpy(display_buffer, "0");
	state = CALC_INPUT_FIRST;
	decimal_entered = 0;
}

void Calculator_Display(void) {
	LCD_Clear();
	Calculator_UpdateFirstLine();
	
	LCD_SetCursor(1, 0);
	
	if(state == CALC_ERROR) {
		LCD_String("                ");
		LCD_SetCursor(1, 5);
		LCD_String("DIV/0");
		} else {
		float display_num;
		if(state == CALC_INPUT_FIRST || state == CALC_INPUT_SECOND) {
			display_num = atof(display_buffer);
			} else {
			display_num = result;
		}
		LCD_DisplayNumber(display_num);
	}
}

void Calculator_AddDigit(char digit) {
	int len = strlen(display_buffer);
	
	if(state == CALC_SHOW_RESULT || state == CALC_ERROR) {
		Calculator_Reset();
		len = 1; // "0"
	}
	
	// Handle decimal point
	if(digit == '.') {
		if(strchr(display_buffer, '.') == NULL) { // No decimal point yet
			if(len < 16) {
				display_buffer[len] = '.';
				display_buffer[len+1] = '\0';
				decimal_entered = 1;
			}
		}
		return;
	}
	
	// Handle digits
	if(len == 1 && display_buffer[0] == '0' && digit != '.') {
		display_buffer[0] = digit;
		display_buffer[1] = '\0';
		} else if(len < 16) {
		display_buffer[len] = digit;
		display_buffer[len+1] = '\0';
	}
	
	Calculator_Display();
}

void Calculator_SetOperation(char op) {
	// If showing result, use it as first number
	if(state == CALC_SHOW_RESULT) {
		first_num = result;
		operation = op;
		state = CALC_INPUT_SECOND;
		strcpy(display_buffer, "0");
		decimal_entered = 0;
		Calculator_Display();
		return;
	}
	
	// If already in second input, calculate first
	if(state == CALC_INPUT_SECOND) {
		Calculator_Calculate();
		if(state == CALC_ERROR) return;
		first_num = result;
		operation = op;
		state = CALC_INPUT_SECOND;
		strcpy(display_buffer, "0");
		decimal_entered = 0;
		Calculator_Display();
		return;
	}
	
	// Normal: from first input
	if(state == CALC_INPUT_FIRST) {
		first_num = atof(display_buffer);
		operation = op;
		state = CALC_INPUT_SECOND;
		strcpy(display_buffer, "0");
		decimal_entered = 0;
		Calculator_Display();
	}
}

void Calculator_Calculate(void) {
	if(state != CALC_INPUT_SECOND) return;
	
	second_num = atof(display_buffer);
	
	// Check for division by zero
	if(operation == '/' && fabs(second_num) < 0.000001) {
		state = CALC_ERROR;
		Calculator_Display();
		return;
	}
	
	// Perform calculation
	switch(operation) {
		case '+': result = first_num + second_num; break;
		case '-': result = first_num - second_num; break;
		case '*': result = first_num * second_num; break;
		case '/': result = first_num / second_num; break;
		default: return;
	}
	
	// Check for overflow/underflow
	if(isinf(result) || isnan(result)) {
		state = CALC_ERROR;
		strcpy(display_buffer, "Error");
		} else {
		strcpy(display_buffer, "0");
		state = CALC_SHOW_RESULT;
	}
	
	Calculator_Display();
}

void Calculator_Clear(void) {
	Calculator_Reset();
	Calculator_Display();
}

// ================= MAIN PROGRAM =================
int main(void) {
	char key;
	char last_key = 0;
	
	// Initialize hardware
	LCD_Init();
	Keypad_Init();
	
	// Display initial screen
	Calculator_Reset();
	Calculator_Display();
	
	while(1) {
		key = Keypad_GetKey();
		
		if(key != 0 && key != last_key) {
			last_key = key;
			
			if(key >= '0' && key <= '9') {
				Calculator_AddDigit(key);
			}
			else if(key == '.') {
				Calculator_AddDigit(key);
			}
			else if(key == '+' || key == '-' || key == '*' || key == '/') {
				Calculator_SetOperation(key);
			}
			else if(key == '=') {
				if(state == CALC_INPUT_SECOND) {
					Calculator_Calculate();
				}
				else if(state == CALC_SHOW_RESULT) {
					// Repeat last calculation
					Calculator_Calculate();
				}
			}
			else if(key == 'C') {
				Calculator_Clear();
			}
			
			// Wait for key release
			while(Keypad_GetKey() != 0);
			last_key = 0;
		}
		
		_delay_ms(10); // Small delay to reduce CPU usage
	}
	
	return 0;
}