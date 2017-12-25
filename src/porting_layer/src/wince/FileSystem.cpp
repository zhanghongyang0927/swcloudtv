#include <windows.h>

#include <porting_layer/FileSystem.h>

namespace ctvc {

const char FILE_SEPARATOR = '\\';   // DOS and Windows specific (back slash)

} // namespace

static void char2wchar(const char *s, WCHAR *wide, int len)
{
    int i;
    for (i = 0; i <= len; i++) {
        wide[i] = (WCHAR) s[i];
    }
}

int remove(const char *pathname)
{
    int result;

    // Assume we use file names without fancy UTF characters that require real conversion...
    int len = strlen(pathname) + 1; // include terminating zero
    WCHAR *widestr = new WCHAR[len];
    char2wchar(pathname, widestr, len);
    if (!::DeleteFile(widestr)) {
        result = 1; // Error
    } else {
        result = 0; // Success
    }

    delete[] widestr;
    return result;
}
