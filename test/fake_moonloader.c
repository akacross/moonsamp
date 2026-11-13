/* A stand-in MoonLoader.asi for overlay_test.c: it writes
 * moonloader\moonloader.log through its own import of WriteFile, as
 * MoonLoader v.026 does, or of fwrite, as v.027 does. */
#include <windows.h>
#include <stdio.h>

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

__declspec(dllexport) void ml_fwrite(const char *line)
{
    static FILE *f;
    if (!f)
        f = fopen("moonloader\\moonloader.log", "ab");
    if (!f)
        return;
    fwrite(line, 1, lstrlenA(line), f);
    fflush(f);
}
