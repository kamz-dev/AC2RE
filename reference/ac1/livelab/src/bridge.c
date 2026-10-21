// Bridge: a local-only TCP command line (127.0.0.1:BridgePort, default 27015) for tools/lab.py.
// One command per line; each reply ends with a line holding a single '.'.
#include "lab.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string.h>

#pragma comment(lib, "ws2_32.lib")

static int g_port;

static void serve(SOCKET s)
{
    char buf[4096], reply[16384];
    int have = 0;
    for (;;) {
        char *nl;
        int r = recv(s, buf + have, (int)sizeof(buf) - 1 - have, 0);
        if (r <= 0) break;
        have += r;
        buf[have] = 0;
        while ((nl = strchr(buf, '\n'))) {
            size_t len;
            *nl = 0;
            if (nl > buf && nl[-1] == '\r') nl[-1] = 0;
            reply[0] = 0;
            if (buf[0]) control_queue_wait(buf, reply, sizeof(reply) - 4);
            len = strlen(reply);
            if (len && reply[len - 1] != '\n') reply[len++] = '\n';
            reply[len++] = '.', reply[len++] = '\n', reply[len] = 0;
            send(s, reply, (int)len, 0);
            have -= (int)(nl + 1 - buf);
            memmove(buf, nl + 1, have + 1);
        }
        if (have >= (int)sizeof(buf) - 1) have = 0;
    }
    closesocket(s);
}

static DWORD WINAPI bridge_thread(LPVOID arg)
{
    WSADATA w;
    SOCKET ls;
    struct sockaddr_in a;
    (void)arg;
    if (WSAStartup(MAKEWORD(2, 2), &w)) return 1;
    ls = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons((u_short)g_port);
    inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);
    if (ls == INVALID_SOCKET || bind(ls, (struct sockaddr *)&a, sizeof(a)) || listen(ls, 4)) {
        lab_log("bridge: cannot listen on 127.0.0.1:%d (%d)", g_port, WSAGetLastError());
        return 1;
    }
    lab_log("bridge listening on 127.0.0.1:%d", g_port);
    for (;;) {
        SOCKET c = accept(ls, NULL, NULL);
        if (c == INVALID_SOCKET) continue;
        serve(c);
    }
}

void bridge_start(int port)
{
    g_port = port;
    if (port > 0) CloseHandle(CreateThread(NULL, 0, bridge_thread, NULL, 0, NULL));
}
