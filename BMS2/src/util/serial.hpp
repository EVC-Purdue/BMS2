#ifndef SERIAL_HPP
#define SERIAL_HPP

#include "stdio.h"

namespace Serial
{
    void printHex(uint8_t data);
    char readHex();
    char getChar();
    void println(const char *str);
    void printf(const char *format, ...); // TODO: Implement printf
    void print(const char *str);
    void print(int data, int base);
}
#endif // SERIAL_HPP