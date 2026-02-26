// TODO
#include "serial.hpp"
#include "freertos/FreeRTOS.h"
#include "driver/uart.h"
#include "string.h"
#include "hardware/pins.hpp"

namespace Serial
{

	void setup(int baud_rate)
	{
		const int BUF_SIZE = 1024;
		const int TX_PIN = pins::ESP::CAN_TX;
		const int RX_PIN = pins::ESP::CAN_RX;
		uart_config_t uart_config = {
			.baud_rate = baud_rate,
			.data_bits = UART_DATA_8_BITS,
			.parity = UART_PARITY_DISABLE,
			.stop_bits = UART_STOP_BITS_1,
			.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
			.source_clk = UART_SCLK_DEFAULT,
		};

		uart_driver_install(pins::ESP::UART_NUM, BUF_SIZE * 2, 0, 0, NULL, 0);
		uart_param_config(pins::ESP::UART_NUM, &uart_config);
		uart_set_pin(pins::ESP::UART_NUM, TX_PIN, RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
	}

	// TODO: Impliment master functions
	void print(const char *str)
	{
		uart_write_bytes(pins::ESP::UART_NUM, str, strlen(str));
	}

	//! Blocking read, recommended to use with Serial.available() to prevent blocking when no data is present
	uint8_t read()
	{
		while (available() <= 0)
			;
		uint8_t data;
		uart_read_bytes(pins::ESP::UART_NUM, &data, 1, portMAX_DELAY);
		return data;
	}

	bool available()
	{
		size_t bytes_available = 0;
		uart_get_buffered_data_len(pins::ESP::UART_NUM, &bytes_available);
		return bytes_available > 0;
	}
	// end todo section

	void printHex(uint8_t data)
	{
		print(static_cast<char>(data), 16);
	}

	uint8_t readHex()
	{
		char hex_char = getChar();
		if ((hex_char >= '0') && (hex_char <= '9'))
		{
			return hex_char - '0';
		}
		else if ((hex_char >= 'A') && (hex_char <= 'F'))
		{
			return hex_char - 'A' + 10;
		}
		else if ((hex_char >= 'a') && (hex_char <= 'f'))
		{
			return hex_char - 'a' + 10;
		}
		else
		{
			return 0; // Invalid hex character, return 0 as default
		}
	}

	char getChar()
	{ // Why wrapper for read?
		return read();
	}

	void println(const char *str)
	{
		print(str);
		print("\n");
	}

	void println()
	{
		print("\n");
	}

	// Warning: 256 char limit for formatted output due to fixed buffer size in printf implementation
	void printf(const char *format, ...)
	{
		char buffer[256];
		va_list args;
		va_start(args, format);
		vsnprintf(buffer, sizeof(buffer), format, args);
		va_end(args);
		println(buffer);
	}

	void print(int data, int base)
	{
		char buffer[33]; // Enough to hold binary representation of 32-bit integer
		if (base == 16)
		{
			snprintf(buffer, sizeof(buffer), "%X", data);
		}
		else if (base == 10)
		{
			snprintf(buffer, sizeof(buffer), "%d", data);
		}
		else if (base == 8)
		{
			snprintf(buffer, sizeof(buffer), "%o", data);
		}
		else if (base == 2)
		{
			snprintf(buffer, sizeof(buffer), "%b", data);
		}
		else
		{
			snprintf(buffer, sizeof(buffer), "%d", data); // Default to decimal
		}
		print(buffer);
	}

	//! Default to decimal base
	void print(int data)
	{
		print(data, 10);
	}

	// From UserInterface.cpp =================================== //

	// Read data from the serial interface into the ui_buffer
	uint8_t read_data(char *buffer, size_t buffer_size)
	{
		uint8_t index = 0; // index to hold current location in ui_buffer
		int c = 0;			   // single character used to store incoming keystrokes
		while (index < buffer_size - 1)
		{
			c = read(); // read one character
			if (((char)c == '\r') || ((char)c == '\n'))
				break;										// if carriage return or linefeed, stop and return data
			if (((char)c == '\x7F') || ((char)c == '\x08')) // remove previous character (decrement index) if Backspace/Delete key pressed      index--;
			{
				if (index > 0)
					index--;
			}
			else if (c >= 0)
			{
				buffer[index++] = (char)c; // put character into ui_buffer
			}
		}
		buffer[index] = '\0'; // terminate string with NULL

		if ((char)c == '\r') // if the last character was a carriage return, also clear linefeed if it is next character
		{
			vTaskDelay(pdMS_TO_TICKS(10)); // allow 10ms for linefeed to appear on serial pins
			int next_char = read();
			if ((char)next_char != '\n') // if linefeed appears, read it and throw it away
			{							 // if not, save it
				buffer[index++] = (char)next_char;
			}
		}

		return index; // return number of characters, not including null terminator
	}

	// Read a float value from the serial interface
	float read_float(char *buffer, size_t buffer_size)
	{
		float data;
		read_data(buffer, buffer_size);
		data = atof(buffer);
		return (data);
	}

	// Read an integer from the serial interface.
	// The routine can recognize Hex, Decimal, Octal, or Binary
	// Example:
	// Hex:     0x11 (0x prefix)
	// Decimal: 17
	// Octal:   021 (leading zero prefix)
	// Binary:  B10001 (leading B prefix)
	int32_t read_int(char *buffer, size_t buffer_size)
	{
		int32_t data;
		read_data(buffer, buffer_size);
		if (buffer[0] == 'm')
			return ('m');
		if ((buffer[0] == 'B') || (buffer[0] == 'b'))
		{
			data = strtol(buffer + 1, NULL, 2);
		}
		else
			data = strtol(buffer, NULL, 0);
		return (data);
	}

	// Read a string from the serial interface.  Returns a pointer to the ui_buffer.
	char *read_string(char *buffer, size_t buffer_size)
	{
		read_data(buffer, buffer_size);
		return (buffer);
	}

	// Read a character from the serial interface
	int8_t read_char(char *buffer, size_t buffer_size)
	{
		read_data(buffer, buffer_size);
		return (buffer[0]);
	}

	int read_int()
	{
		char buffer[16];
		read_data(buffer, sizeof(buffer));
		return strtol(buffer, NULL, 0);
	}

}