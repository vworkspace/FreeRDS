#ifndef FREERDS_X11RDP_INPUT_UNICODE_H
#define FREERDS_X11RDP_INPUT_UNICODE_H

#include <winpr/crt.h>

void start_asynchronous_unicode_input_processing_thread();
void stop_asynchronous_unicode_input_processing_thread();
void postUnicodeInputEvent(DWORD unicode, DWORD flags, void (*enqueueKey)(int, int));

#endif /* FREERDS_X11RDP_INPUT_UNICODE_H */
