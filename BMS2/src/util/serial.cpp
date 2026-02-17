// TODO
#include "serial.hpp"

namespace Serial {

void printHex(uint8_t data) {
	if (data < 16) {
		Serial.print("0");
		Serial.print((byte)data, HEX);
	} else {
		Serial.print((byte)data, HEX);
	}
}

char readHex() {
	byte data;
	hexToByteBuffer[2] = getChar();
	hexToByteBuffer[3] = getChar();
	getChar();
	getChar();
	data = strtol(hexToByteBuffer, NULL, 0);
	return (data);
}

char getChar() {
	while (Serial.available() <= 0);
	return (Serial.read());
}

void println(const char* str) {
    Serial.println(str);
}

void printf(const char* format, ...) {
	char buffer[128]; // Adjust size as needed
	va_list args;
	va_start(args, format);
	vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);
	Serial.println(buffer);
}

void print(const char* str) {
    Serial.print(str);
}

void print(int data, int base) {
	Serial.print(data, base);
}

}