// ac1lab.exe - starts AssassinsCreed_Dx9.exe suspended and loads DLLs at its entry point, before the game's own code
// runs (the loader has bound the imports by then). Nothing in the game folder is changed.
// Usage: ac1lab.exe [game dir] [-- game args]
//   Game dir: argument, else AC_GAME_DIR, else an "Assassin's Creed" folder next to any parent of this exe.
//   lab.ini [Launch] Windowed=1 also loads ..\tools\ac1_windowed\ac1_windowed.dll first (the window mode tool).
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>

static int file_exists(const char *p)
{
    DWORD a = GetFileAttributesA(p);
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

static int find_game_dir(const char *start, char *out)
{
    char dir[MAX_PATH], probe[MAX_PATH], *slash;
    strcpy_s(dir, MAX_PATH, start);
    for (;;) {
        sprintf_s(probe, MAX_PATH, "%s\\Assassin's Creed\\AssassinsCreed_Dx9.exe", dir);
        if (file_exists(probe)) {
            sprintf_s(out, MAX_PATH, "%s\\Assassin's Creed", dir);
            return 1;
        }
        if (!(slash = strrchr(dir, '\\'))) return 0;
        *slash = 0;
    }
}

static int fail(const char *what)
{
    fprintf(stderr, "ac1lab: %s (error %lu)\n", what, GetLastError());
    return 1;
}

int main(int argc, char **argv)
{
    char self_dir[MAX_PATH], ini[MAX_PATH], dlls[2][MAX_PATH], game_dir[MAX_PATH], exe[MAX_PATH], cmd[4096], *slash;
    STARTUPINFOA si = {sizeof(si)};
    PROCESS_INFORMATION pi;
    CONTEXT ctx;
    DWORD base, entry, nt_off, entry_rva, prot;
    BYTE saved[5], stub[128], jmp[5], *remote, *p;
    SIZE_T n;
    FARPROC load_library = GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA");
    int i, argi = 1, len, ndll = 0;

    GetModuleFileNameA(NULL, self_dir, MAX_PATH);
    if ((slash = strrchr(self_dir, '\\'))) *slash = 0;
    sprintf_s(ini, MAX_PATH, "%s\\lab.ini", self_dir);
    if (GetPrivateProfileIntA("Launch", "Windowed", 1, ini)) {
        sprintf_s(dlls[ndll], MAX_PATH, "%s\\..\\tools\\ac1_windowed\\ac1_windowed.dll", self_dir);
        if (file_exists(dlls[ndll])) ndll++;
        else printf("ac1lab: windowed mode DLL not found (%s), starting without it\n", dlls[ndll]);
    }
    sprintf_s(dlls[ndll], MAX_PATH, "%s\\ac1lab.dll", self_dir);
    if (!file_exists(dlls[ndll])) return fail("ac1lab.dll not found next to the launcher");
    ndll++;

    if (argc > 1 && strcmp(argv[1], "--")) {
        strcpy_s(game_dir, MAX_PATH, argv[1]);
        argi = 2;
    } else if (!GetEnvironmentVariableA("AC_GAME_DIR", game_dir, MAX_PATH) && !find_game_dir(self_dir, game_dir)) {
        return fail("game folder not found; pass it as the first argument or set AC_GAME_DIR");
    }
    sprintf_s(exe, MAX_PATH, "%s\\AssassinsCreed_Dx9.exe", game_dir);
    if (!file_exists(exe)) return fail("AssassinsCreed_Dx9.exe not found in the game folder");
    len = sprintf_s(cmd, sizeof(cmd), "\"%s\"", exe);
    if (argi < argc && !strcmp(argv[argi], "--")) argi++;
    for (i = argi; i < argc; i++) len += sprintf_s(cmd + len, sizeof(cmd) - len, " %s", argv[i]);

    if (!CreateProcessA(exe, cmd, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, game_dir, &si, &pi))
        return fail("CreateProcess failed");

    // suspended x86 thread: EBX = PEB, PEB+8 = image base
    ctx.ContextFlags = CONTEXT_INTEGER;
    if (!GetThreadContext(pi.hThread, &ctx)) goto kill;
    if (!ReadProcessMemory(pi.hProcess, (void *)(ctx.Ebx + 8), &base, 4, &n)) goto kill;
    if (!ReadProcessMemory(pi.hProcess, (void *)(base + 0x3C), &nt_off, 4, &n)) goto kill;
    if (!ReadProcessMemory(pi.hProcess, (void *)(base + nt_off + 0x28), &entry_rva, 4, &n)) goto kill;
    entry = base + entry_rva;
    if (!ReadProcessMemory(pi.hProcess, (void *)entry, saved, 5, &n)) goto kill;

    // remote+0 stub, +256 saved entry bytes, +512 / +512+MAX_PATH the DLL paths
    remote = (BYTE *)VirtualAllocEx(pi.hProcess, NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!remote) goto kill;
    p = stub;
    *p++ = 0x60;                                                                   // pushad
    for (i = 0; i < ndll; i++) {
        DWORD path = (DWORD)(remote + 512 + i * MAX_PATH);
        if (!WriteProcessMemory(pi.hProcess, remote + 512 + i * MAX_PATH, dlls[i], strlen(dlls[i]) + 1, &n)) goto kill;
        *p++ = 0x68; memcpy(p, &path, 4); p += 4;                                   // push path
        *p++ = 0xB8; memcpy(p, &load_library, 4); p += 4;                           // mov eax, LoadLibraryA
        *p++ = 0xFF; *p++ = 0xD0;                                                   // call eax
    }
    {
        DWORD saved_addr = (DWORD)(remote + 256), rel;
        *p++ = 0xBE; memcpy(p, &saved_addr, 4); p += 4;                             // mov esi, saved
        *p++ = 0xBF; memcpy(p, &entry, 4); p += 4;                                  // mov edi, entry
        *p++ = 0xB9; *p++ = 5; *p++ = 0; *p++ = 0; *p++ = 0;                        // mov ecx, 5
        *p++ = 0xF3; *p++ = 0xA4;                                                   // rep movsb
        *p++ = 0x61;                                                                // popad
        *p++ = 0xE9;                                                                // jmp entry
        rel = entry - ((DWORD)(remote + (p - stub)) + 4);
        memcpy(p, &rel, 4); p += 4;
    }
    if (!WriteProcessMemory(pi.hProcess, remote, stub, p - stub, &n)) goto kill;
    if (!WriteProcessMemory(pi.hProcess, remote + 256, saved, 5, &n)) goto kill;
    jmp[0] = 0xE9;
    {
        DWORD rel = (DWORD)remote - (entry + 5);
        memcpy(jmp + 1, &rel, 4);
    }
    // the entry point is in the in-memory image only; the exe on disk is untouched
    if (!VirtualProtectEx(pi.hProcess, (void *)entry, 5, PAGE_EXECUTE_READWRITE, &prot)) goto kill;
    if (!WriteProcessMemory(pi.hProcess, (void *)entry, jmp, 5, &n)) goto kill;
    FlushInstructionCache(pi.hProcess, (void *)entry, 5);
    ResumeThread(pi.hThread);
    printf("ac1lab: game started, pid %lu; %d DLL(s): %s%s%s\n", pi.dwProcessId, ndll, dlls[0], ndll > 1 ? ", " : "",
           ndll > 1 ? dlls[1] : "");
    printf("ac1lab: logs in %s\\sessions\\<date-time>\\lab.log\n", self_dir);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return 0;

kill:
    fail("injection failed, game terminated");
    TerminateProcess(pi.hProcess, 1);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return 1;
}
