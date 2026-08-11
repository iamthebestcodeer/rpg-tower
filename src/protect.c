#include "protect.h"
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <stdlib.h>
#endif

// ---------------------------------------------------------------------------
// XOR decrypt (symmetric). Separated into its own TU so the compiler cannot
// inline-propagate plaintext across translation units before LTO. Marked
// noinline and compiled without auto-vectorization hints.
// ---------------------------------------------------------------------------
#if defined(__GNUC__)
__attribute__((noinline, optimize("no-tree-vectorize")))
#endif
void AV_Decrypt(char *data, size_t len, unsigned char key)
{
    // Volatile pointer prevents the compiler from optimizing the loop into a
    // memset/memcpy that would be easier to pattern-match.
    volatile char *v = (volatile char *)data;
    for (size_t i = 0; i < len; i++) v[i] ^= (char)key;
    // Null-terminate
    if (len > 0) ((char *)data)[len] = '\0';
}

// ---------------------------------------------------------------------------
// Anti-debug / anti-tamper (Windows only).
// No cleartext import names are left in the IAT: DLL and function names are
// stored XOR-encrypted and resolved at runtime via GetModuleHandleA +
// GetProcAddress. A linear scan of several detection methods is used – if any
// one fires, the process is terminated via ExitProcess (not exit()) so the
// CRT atexit handlers never run and no visible error appears.
// ---------------------------------------------------------------------------
#ifdef _WIN32

// Encrypted strings (XOR 0xA7), decrypted on demand on the stack.
static void decrypt_tmp(char *buf, const unsigned char *enc, size_t n, unsigned char k)
{
    for (size_t i = 0; i < n; i++) buf[i] = (char)(enc[i] ^ k);
    buf[n] = '\0';
}

__attribute__((noinline))
void AV_AntiDebug(void)
{
    // Encrypted literals – must NOT appear as cleartext in .rdata.
    static const unsigned char enc_kernel32[] = {0xCC,0xC2,0xD5,0xC9,0xC2,0xCB,0x94,0x95,0x89,0xC3,0xCB,0xCB};
    static const unsigned char enc_ntdll[]    = {0xC9,0xD3,0xC3,0xCB,0xCB,0x89,0xC3,0xCB,0xCB};
    static const unsigned char enc_IsDebuggerPresent[] = {0xEE,0xD4,0xE3,0xC2,0xC5,0xD2,0xC0,0xC0,0xC2,0xD5,0xF7,0xD5,0xC2,0xD4,0xC2,0xC9,0xD3};
    static const unsigned char enc_CheckRemote[] = {0xE4,0xCF,0xC2,0xC4,0xCC,0xF5,0xC2,0xCA,0xC8,0xD3,0xC2,0xE3,0xC2,0xC5,0xD2,0xC0,0xC0,0xC2,0xD5,0xF7,0xD5,0xC2,0xD4,0xC2,0xC9,0xD3};
    static const unsigned char enc_NtQIP[] = {0xE9,0xD3,0xF6,0xD2,0xC2,0xD5,0xDE,0xEE,0xC9,0xC1,0xC8,0xD5,0xCA,0xC6,0xD3,0xCE,0xC8,0xC9,0xF7,0xD5,0xC8,0xC4,0xC2,0xD4,0xD4};
    static const unsigned char enc_IsDebuggedPEB[] = {0xC9,0xD3,0xD1,0xD2,0xF1,0xC2,0xC3,0xD2,0xC0,0xC0,0xC2,0xC3}; // "NtCurrentTeb" trick uses PEB

    const unsigned char KEY = 0xA7;
    char buf_kernel32[16], buf_ntdll[12], buf_Idp[20], buf_Crdp[30], buf_NtQIP[28];

    decrypt_tmp(buf_kernel32, enc_kernel32, sizeof(enc_kernel32), KEY);
    decrypt_tmp(buf_ntdll,    enc_ntdll,    sizeof(enc_ntdll), KEY);
    decrypt_tmp(buf_Idp,      enc_IsDebuggerPresent, sizeof(enc_IsDebuggerPresent), KEY);
    decrypt_tmp(buf_Crdp,     enc_CheckRemote, sizeof(enc_CheckRemote), KEY);
    decrypt_tmp(buf_NtQIP,    enc_NtQIP, sizeof(enc_NtQIP), KEY);

    // Mark enc_* as used to prevent optimizer from discarding
    (void)enc_IsDebuggedPEB;

    HMODULE hK32 = GetModuleHandleA(buf_kernel32);
    HMODULE hNtdll = GetModuleHandleA(buf_ntdll);
    if (!hK32 || !hNtdll) return;

    typedef BOOL (WINAPI *PFN_IsDebuggerPresent)(void);
    typedef BOOL (WINAPI *PFN_CheckRemoteDebuggerPresent)(HANDLE, PBOOL);
    typedef LONG (WINAPI *PFN_NtQueryInformationProcess)(HANDLE, ULONG, PVOID, ULONG, PULONG);

    PFN_IsDebuggerPresent pIsDebuggerPresent = (PFN_IsDebuggerPresent)(void*)GetProcAddress(hK32, buf_Idp);
    PFN_CheckRemoteDebuggerPresent pCheckRemote = (PFN_CheckRemoteDebuggerPresent)(void*)GetProcAddress(hK32, buf_Crdp);
    PFN_NtQueryInformationProcess pNtQIP = (PFN_NtQueryInformationProcess)(void*)GetProcAddress(hNtdll, buf_NtQIP);

    // Scrub decrypted names from stack immediately
    SecureZeroMemory(buf_Idp, sizeof(buf_Idp));
    SecureZeroMemory(buf_Crdp, sizeof(buf_Crdp));
    SecureZeroMemory(buf_NtQIP, sizeof(buf_NtQIP));
    SecureZeroMemory(buf_kernel32, sizeof(buf_kernel32));
    SecureZeroMemory(buf_ntdll, sizeof(buf_ntdll));

    int detected = 0;

    // 1) IsDebuggerPresent
    if (pIsDebuggerPresent && pIsDebuggerPresent()) detected = 1;

    // 2) CheckRemoteDebuggerPresent
    if (!detected && pCheckRemote) {
        BOOL isDbg = FALSE;
        if (pCheckRemote(GetCurrentProcess(), &isDbg) && isDbg) detected = 1;
    }

    // 3) PEB BeingDebugged flag (x64: GS:[0x60] -> PEB, offset 0x02)
    // Use intrinsic-free inline check compatible with MinGW.
    if (!detected) {
#if defined(__x86_64__) || defined(_M_X64) || defined(__amd64__)
        unsigned char beingDebugged = 0;
#ifdef __GNUC__
        __asm__ __volatile__(
            "mov %%gs:0x60, %%rax\n\t"
            "movb 0x02(%%rax), %0\n\t"
            : "=r"(beingDebugged) : : "rax");
#else
        beingDebugged = 0;
#endif
        if (beingDebugged) detected = 1;
#elif defined(__i386__) || defined(_M_IX86)
        unsigned char beingDebugged = 0;
#ifdef __GNUC__
        __asm__ __volatile__(
            "mov %%fs:0x30, %%eax\n\t"
            "movb 0x02(%%eax), %0\n\t"
            : "=r"(beingDebugged) : : "eax");
#endif
        if (beingDebugged) detected = 1;
#endif
    }

    // 4) NtQueryInformationProcess(ProcessDebugPort = 7)
    if (!detected && pNtQIP) {
        ULONG_PTR debugPort = 0;
        if (pNtQIP(GetCurrentProcess(), 7, &debugPort, sizeof(debugPort), NULL) == 0 /*STATUS_SUCCESS*/)
            if (debugPort != 0) detected = 1;
        // ProcessDebugObjectHandle = 30
        HANDLE hDebugObj = NULL;
        if (!detected && pNtQIP(GetCurrentProcess(), 30, &hDebugObj, sizeof(hDebugObj), NULL) == 0)
            if (hDebugObj != NULL) detected = 1;
    }

    // 5) Timing probe intentionally removed.
    // A wall-time probe of a tight loop (80 ms threshold) is prone to
    // false positives: normal Windows scheduling stalls, VM contention,
    // or a busy GitHub Actions runner can easily deschedule the process
    // for >80 ms and would have been misclassified as "debugged", causing
    // a silent ExitProcess(0) that kills gameplay or swallows --bench
    // results with a success exit code. The four structural checks above
    // (IsDebuggerPresent, CheckRemoteDebuggerPresent, PEB BeingDebugged,
    // NtQueryInformationProcess DebugPort/Object) are sufficient and have
    // no benign false-positive mode. If a timing heuristic is re-added
    // it must not singularly trigger termination — require corroboration
    // from another check or multiple consecutive outliers, and exit
    // non-zero so failures are visible.
    (void)detected; // keep variable used in builds without the probe

    if (detected) {
        // Silent termination – no MessageBox, no stdout/stderr, no exit code that screams "anti-debug".
        // ExitProcess avoids CRT cleanup that could be hooked.
        ExitProcess(0);
    }
}

#else // not Windows

void AV_AntiDebug(void) { /* no-op elsewhere */ }

#endif
