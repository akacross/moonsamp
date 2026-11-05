/* A stand-in MoonLoader.asi for overlay_test.c: it writes
 * moonloader\moonloader.log through its own import of WriteFile, as
 * MoonLoader does. */
#include <windows.h>

static HANDLE log_file = INVALID_HANDLE_VALUE;

__declspec(dllexport) void ml_write(const char *line)
{
    DWORD n;
    if (log_file == INVALID_HANDLE_VALUE) {
        CreateDirectoryA("moonloader", NULL);
        log_file = CreateFileA("moonloader\\moonloader.log", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                               NULL, CREATE_ALWAYS, 0, NULL);
    }
    WriteFile(log_file, line, lstrlenA(line), &n, NULL);
    FlushFileBuffers(log_file);
}
