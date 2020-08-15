#include "timeout.h"
#include "main.h"
#include "http_proxy.h"

int timeout_minute;

void *tcp_timeout_check(void *nullPtr)
{
    int i;

    while (1) {
        sleep(60);
        for (i = 0; i < MAX_CONNECTION; i += 2)
            if (cts[i].fd > -1) {
                if (cts[i].timer >= timeout_minute) {
                    close_connection(cts + i);
                }
                else
                    cts[i].timer++;
            }
    }
}
