#include <stdio.h>
#include <stdint.h>
#include <windows.h>

void print_error(const char *context)
{
    DWORD error_code = GetLastError();
    char buffer[256];
    DWORD size = FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_MAX_WIDTH_MASK,
        NULL, error_code, MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
        buffer, sizeof(buffer), NULL);
    if (size == 0)
    {
        buffer[0] = 0;
    }
    fprintf(stderr, "%s: %s\n", context, buffer);
}

// Opens the specified serial port, configures its timeouts, and sets its
// baud rate.  Returns a handle on success, or INVALID_HANDLE_VALUE on failure.
HANDLE open_serial_port(const char *device, uint32_t baud_rate)
{
    HANDLE port = CreateFileA(device, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (port == INVALID_HANDLE_VALUE)
    {
        print_error(device);
        return INVALID_HANDLE_VALUE;
    }

    // Flush away any bytes previously read or written.
    BOOL success = FlushFileBuffers(port);
    if (!success)
    {
        print_error("Failed to flush serial port");
        CloseHandle(port);
        return INVALID_HANDLE_VALUE;
    }

    // Configure read and write operations to time out after 100 ms.
    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout = 0;
    timeouts.ReadTotalTimeoutConstant = 100;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant = 100;
    timeouts.WriteTotalTimeoutMultiplier = 0;

    success = SetCommTimeouts(port, &timeouts);
    if (!success)
    {
        print_error("Failed to set serial timeouts");
        CloseHandle(port);
        return INVALID_HANDLE_VALUE;
    }

    // Set the baud rate and other options.
    DCB state = {0};
    state.DCBlength = sizeof(DCB);
    state.BaudRate = baud_rate;
    state.ByteSize = 8;
    state.Parity = NOPARITY;
    state.StopBits = ONESTOPBIT;
    success = SetCommState(port, &state);
    if (!success)
    {
        print_error("Failed to set serial settings");
        CloseHandle(port);
        return INVALID_HANDLE_VALUE;
    }

    return port;
}

// Writes bytes to the serial port, returning 0 on success and -1 on failure.
int write_port(HANDLE port, uint8_t *buffer, size_t size)
{
    DWORD written;
    BOOL success = WriteFile(port, buffer, size, &written, NULL);
    if (!success)
    {
        print_error("Failed to write to port");
        return -1;
    }
    if (written != size)
    {
        print_error("Failed to write all bytes to port");
        return -1;
    }
    return 0;
}

// Reads bytes from the serial port.
// Returns after all the desired bytes have been read, or if there is a
// timeout or other error.
// Returns the number of bytes successfully read into the buffer, or -1 if
// there was an error reading.
SSIZE_T read_port(HANDLE port, uint8_t *buffer, size_t size)
{
    DWORD received;
    BOOL success = ReadFile(port, buffer, size, &received, NULL);
    if (!success)
    {
        print_error("Failed to read from port");
        return -1;
    }
    return received;
}

uint32_t get_distance(HANDLE port)
{
    write_port(port, "\xA0", 1);
    uint8_t bytes[4] = {0};
    read_port(port, bytes, 3);
    uint32_t h = (uint32_t)bytes[0] << 16;
    uint32_t m = (uint32_t)bytes[1] << 8;
    uint32_t l = bytes[2];
    uint32_t um = h + m + l;
    Sleep(100); // every 100ms
    return um;
}

uint32_t get_devinfo(HANDLE port)
{
    write_port(port, "\xF1", 1);
    uint8_t bytes[16] = {0};
    read_port(port, bytes, 13);
    return 0;
}

int main()
{
    // Choose the serial port name.
    const char *device = "\\\\.\\COM10";

    // Choose the baud rate (bits per second).
    uint32_t baud_rate = 9600;

    HANDLE port = open_serial_port(device, baud_rate);
    if (port == INVALID_HANDLE_VALUE)
    {
        return 1;
    }
    for (;;)
    {
        uint32_t um = get_distance(port);
        printf("%fcm\n", (double)um / 10000);
    }

    CloseHandle(port);
    return 0;
}