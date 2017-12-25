#include <sys/time.h>

struct tm *localtime(const time_t *timep)
{
    SYSTEMTIME system_time;
    GetLocalTime(&system_time);

    static struct tm tm_info;

    tm_info.tm_hour = system_time.wHour;
    tm_info.tm_mday = system_time.wDay;
    tm_info.tm_min = system_time.wMinute;
    tm_info.tm_mon = system_time.wMonth - 1;
    tm_info.tm_sec = system_time.wSecond;
    tm_info.tm_wday = system_time.wDayOfWeek;
    tm_info.tm_year = system_time.wYear - 1900;
    tm_info.tm_yday = 0;
    tm_info.tm_isdst = 0;   // Tough, not supported

    return &tm_info;
}

static char *fmt(const char *format, const struct tm *const t, char *pt, const char *const ptlim);

size_t strftime(char *s, size_t max, const char *format, const struct tm *tm)
{
    char *p = fmt(format, tm, s, s + max);
    if (p == s + max) {
        if (max > 0) {
            s[max - 1] = '\0';
        }
        return 0;
    }
    *p = '\0';
    return p - s;
}

static char *convert(const int n, const char *const format, char *pt, const char *const ptlim)
{
    char buf[64];

    _snprintf(buf, sizeof(buf), format, n);
    char *str = buf;
    while (pt < ptlim && *str != '\0') {
        *pt = *str;
        str++;
        pt++;
    }

    return pt;
}

static char *fmt(const char *format, const struct tm *const t, char *pt, const char *const ptlim)
{
    for (; *format; ++format) {
        if (*format == '%') {
            format++;
            switch (*format) {
            case '\0':
                break;   // Null string terminator: stop.
            case 'H':
                pt = convert(t->tm_hour, "%02d", pt, ptlim);
                continue;
            case 'M':
                pt = convert (t->tm_min, "%02d", pt, ptlim);
                continue;
            case 'S':
                pt = convert (t->tm_sec, "%02d", pt, ptlim);
                continue;
            default:
                printf("WARNING: Unsupported strftime format character '%c'", *format);
                break;
            }
        }
        if (pt == ptlim)
            break;
        *pt++ = *format;
    }
    return pt;
}
