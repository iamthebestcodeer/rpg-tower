#ifndef PROTECT_H
#define PROTECT_H

#include <stddef.h>

// In-place XOR decrypt. key is per-string. Length includes no null – caller
// provides buffer size. Decrypt is symmetric: encrypt == decrypt.
void AV_Decrypt(char *data, size_t len, unsigned char key);

// Run anti-debug / anti-tamper checks. Safe to call on any platform – on
// non-Windows it is a no-op. On Windows it performs multiple checks via
// dynamically resolved imports (no cleartext import names in the IAT) and
// silently terminates if a debugger / emulator is detected. No message is
// printed so patching is non-obvious.
void AV_AntiDebug(void);

// Helper: decrypt a stack buffer that was initialized with XOR-encrypted bytes.
// Example:
//   char title[32] = {0xE6,0xC2,...}; // encrypted "Aetherium Vanguard - OPTIMIZED"
//   AV_Decrypt(title, sizeof(title)-1, 0xA7);
#endif
