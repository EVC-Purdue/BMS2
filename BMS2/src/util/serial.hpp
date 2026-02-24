#ifndef SERIAL_HPP
#define SERIAL_HPP

#include "stdio.h"

namespace Serial
{
    bool available();
    
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