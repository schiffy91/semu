// window-trace: records how an app's windows appear, so a launch transition can be checked
// without looking at the screen. Every 15 ms it prints each on-screen window the named owner
// has: elapsed ms, bounds, alpha, layer, window id. An unchanged window list is printed once.
// build: cc -O2 -framework CoreGraphics -framework CoreFoundation window-trace.c -o window-trace
// usage: window-trace OWNER|PID SECONDS   (a number is a process id)
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static double number(CFDictionaryRef dictionary, CFStringRef key) {
    double value = 0;
    CFNumberRef reference = CFDictionaryGetValue(dictionary, key);
    if (reference) CFNumberGetValue(reference, kCFNumberDoubleType, &value);
    return value;
}

static double milliseconds(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return now.tv_sec * 1000.0 + now.tv_nsec / 1e6;
}

int main(int argc, char **argv) {
    if (argc != 3) { fprintf(stderr, "usage: window-trace OWNER|PID SECONDS\n"); return 2; }
    CFStringRef owner = CFStringCreateWithCString(NULL, argv[1], kCFStringEncodingUTF8);
    char *end;
    long pid = strtol(argv[1], &end, 10);
    if (*end) pid = 0;
    double start = milliseconds(), limit = atof(argv[2]) * 1000.0;
    char previous[4096] = "";
    while (milliseconds() - start < limit) {
        CFArrayRef windows = CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly, kCGNullWindowID);
        char line[4096] = "";
        for (CFIndex index = 0; windows && index < CFArrayGetCount(windows); index++) {
            CFDictionaryRef window = CFArrayGetValueAtIndex(windows, index);
            CFStringRef name = CFDictionaryGetValue(window, kCGWindowOwnerName);
            if (pid ? (long)number(window, kCGWindowOwnerPID) != pid
                    : !name || CFStringCompare(name, owner, 0) != kCFCompareEqualTo) continue;
            CGRect bounds;
            CGRectMakeWithDictionaryRepresentation(CFDictionaryGetValue(window, kCGWindowBounds), &bounds);
            char entry[256];
            snprintf(entry, sizeof entry, " [%.0f,%.0f %.0fx%.0f alpha %.2f layer %.0f id %.0f]", bounds.origin.x, bounds.origin.y,
                     bounds.size.width, bounds.size.height, number(window, kCGWindowAlpha), number(window, kCGWindowLayer),
                     number(window, kCGWindowNumber));
            strncat(line, entry, sizeof line - strlen(line) - 1);
        }
        if (windows) CFRelease(windows);
        if (strcmp(line, previous) != 0) {
            printf("%7.0f ms%s\n", milliseconds() - start, line[0] ? line : " (no window)");
            fflush(stdout);
            strcpy(previous, line);
        }
        usleep(15000);
    }
    return 0;
}
