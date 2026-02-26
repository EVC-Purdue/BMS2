#ifndef SERIAL_HPP
#define SERIAL_HPP

#include "stdio.h"
#include "driver/uart.h"

namespace Serial
{
    const uart_port_t UART_NUM = UART_NUM_0; // TODO

    bool available();
    
    void setup(int baud_rate);
    uint8_t read();
    int read_int();
    uint8_t readHex();
    char getChar();
    
    void print(const char *str);
    void print(int data, int base = 10);
    void print(int data);
    void println(const char *str);
    void println();
    void printf(const char *format, ...);
    void printHex(uint8_t data);

}
#endif // SERIAL_HPP