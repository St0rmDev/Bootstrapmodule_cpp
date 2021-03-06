/*
    Initial author: Convery (tcn@ayria.se)
    Started: 03-08-2017
    License: MIT
    Notes:
        Inserts a hook on the games entrypoint.
        PE hooks the entrypoint to support .NET.
*/

#include "../Stdinclude.h"

#if defined (_WIN32)

// Global scope to be accessible from ASM.
extern "C" size_t Entrypoint{};
uint8_t Originaltext[20]{};
void Callback();

// Access to external modules.
extern "C" void Resumeprogram();
extern void Loadallplugins();
extern void RestoreTLS();

// Get the games entrypoint.
size_t Getentrypoint()
{
    // Module(NULL) gets the host application.
    HMODULE Modulehandle = GetModuleHandleA(NULL);
    if(!Modulehandle) return 0;

    // Traverse the PE header.
    PIMAGE_DOS_HEADER DOSHeader = (PIMAGE_DOS_HEADER)Modulehandle;
    PIMAGE_NT_HEADERS NTHeader = (PIMAGE_NT_HEADERS)((DWORD_PTR)Modulehandle + DOSHeader->e_lfanew);

    return (size_t)((DWORD_PTR)Modulehandle + NTHeader->OptionalHeader.AddressOfEntryPoint);
}

// Overwrite and restore the entrypoint.
void Removeentrypoint()
{
    Printfunction();

    Entrypoint = Getentrypoint();
    if(!Entrypoint) return;

    auto Protection = Memprotect::Unprotectrange((void *)Entrypoint, 20);
    {
        // Take a backup of the text segment.
        std::memcpy(Originaltext, (void *)Entrypoint, 20);

        // Per architecture patching.
        #if defined (ENVIRONMENT64)
            *(uint8_t *)(Entrypoint + 0) = 0x48;                   // mov
            *(uint8_t *)(Entrypoint + 1) = 0xB8;                   // rax
            *(uint64_t *)(Entrypoint + 2) = (uint64_t)Callback;    // Address
            *(uint8_t *)(Entrypoint + 10) = 0xFF;                  // jmp reg
            *(uint8_t *)(Entrypoint + 11) = 0xE0;                  // rax
        #else
            *(uint8_t *)(Entrypoint + 0) = 0xE9;                   // jmp
            *(uint32_t *)(Entrypoint + 1) = ((uint32_t)Callback - (Entrypoint + 5));
        #endif
    }
    Memprotect::Protectrange((void *)Entrypoint, 20, Protection);
}
void Restoreentrypoint()
{
    Printfunction();
    if(!Entrypoint) return;

    auto Protection = Memprotect::Unprotectrange((void *)Entrypoint, 20);
    {
        // Restore the text segment.
        std::memcpy((void *)Entrypoint, Originaltext, 20);
    }
    Memprotect::Protectrange((void *)Entrypoint, 20, Protection);
}

// Windows x64 defines this in a .asm file.
#if !defined (ENVIRONMENT64)
extern "C" void Resumeprogram()
{
    Printfunction();

    // Clang and CL does not want to cooperate here.
    #if defined (__clang__)
        *((size_t*)__builtin_frame_address(0) + 1) = Entrypoint;
    #else
        *(size_t *)_AddressOfReturnAddress() = Entrypoint;
    #endif
}
#endif

// The callback from the games entrypoint.
void Callback()
{
    Printfunction();

    // Remove our hackery.
    Restoreentrypoint();
    RestoreTLS();

    // Do what we came here for.
    Loadallplugins();

    // Resume execution at the entrypoint.
    Resumeprogram();
}

// Install the hook on startup.
namespace { struct Overwriteentrypoint { Overwriteentrypoint() { Removeentrypoint(); }; }; static Overwriteentrypoint Loader{}; }

#endif
