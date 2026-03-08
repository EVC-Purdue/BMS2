#ifndef SERIAL_HPP
#define SERIAL_HPP

#include "stdint.h"
#include "stdio.h"
#include "driver/usb_serial_jtag.h"
#include "string.h"

namespace serial
{
    static char serial_buffer[32] = {0};
    static uint8_t serial_buffer_index = 0;

    static bool available()
    {
        char c;

        int len = usb_serial_jtag_read_bytes(&c, 1, 0);

        if (len > 0)
        {
            // push back into a small buffer if you need it later
            // or store it globally
            serial_buffer[serial_buffer_index++] = c;
            return true;
        }

        return false;
    }

    static char getChar()
    {
        if (serial_buffer_index > 0)
        {
            char c = serial_buffer[0];
            // Shift the buffer left
            memmove(serial_buffer, serial_buffer + 1, serial_buffer_index - 1);
            serial_buffer_index--;
            return c;
        }
        else
        {
            // No data in buffer, try reading directly
            char c;
            int len = usb_serial_jtag_read_bytes(&c, 1, 0);
            if (len > 0)
            {
                return c;
            }
        }
        return 0; // No data available
    }

    static int read_int()
    {
        char buf[32];
        int idx = 0;
        uint8_t c;

        while (idx < sizeof(buf) - 1)
        {
            int len = 0;
            if (serial_buffer_index > 0)
            {
                c = serial_buffer[0];
                // Shift the buffer left
                memmove(serial_buffer, serial_buffer + 1, serial_buffer_index - 1);
                serial_buffer_index--;
                len = 1;
            }
            else
            {
                len = usb_serial_jtag_read_bytes(&c, 1, pdMS_TO_TICKS(10));
            }

            if (len == 0)
                // continue;
                break;

            if (c == '\n' || c == '\r')
            {
                if (idx == 0 && c == '\r')
                    continue;

                if (idx > 0)
                    break;

                break;
            }

            buf[idx++] = (char)c;

            // usb_serial_jtag_write_bytes((const char *)&c, 1, 0);
        }
        printf("Read from console: %s\n", buf);
        buf[idx] = 0;
        return atoi(buf);
    }
}

#endif // SERIAL_HPP