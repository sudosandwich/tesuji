#include <exception>
#include <format>
#include <map>
#include <string>
#include <string_view>

#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#define NOMINMAX
#include <Windows.h>


struct FarprocFunction
{
    template<typename ReturnT = DWORD> ReturnT operator()(auto... args) {
        using Fn = ReturnT (*)(decltype(args)...);
        auto fn  = reinterpret_cast<Fn>(funcAddress);
        return fn(args...);
    }

    FARPROC funcAddress;
};


std::string GetLastWin32Error() {
    DWORD errorMessageID = ::GetLastError();
    if(errorMessageID == 0) {
        return std::string(); // No error message has been recorded
    }
    LPSTR       messageBuffer = nullptr;
    size_t      size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
                                          | FORMAT_MESSAGE_IGNORE_INSERTS,
                                      nullptr, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                      (LPSTR)&messageBuffer, 0, nullptr);
    std::string message(messageBuffer, size);
    LocalFree(messageBuffer);
    return message;
}


map<string, FarprocFunction> LoadLibraryDyn(string_view dllName) {
    map<string, FarprocFunction> exports;

    HMODULE lib = LoadLibraryA(dllName.data());
    if(!lib) {
        throw runtime_error(
            format("Failed to load library '{}'\n{}", dllName, GetLastWin32Error()));
    }
    BYTE *baseAddress = (BYTE *)lib;

    // We advance through the internal pointer structures representing the DLL.
    // This is magic.

    // dos headers
    auto *dosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(lib);
    if(dosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
        throw runtime_error(
            format("IMAGE_DOS_SIGNATURE not found in header for "
                   "'{}', it contained '{}' instead of '{}'\n{}",
                   dllName, dosHeader->e_magic, IMAGE_DOS_SIGNATURE, GetLastWin32Error()));
    }

    // nt headers
    auto *ntHeaderAddress = baseAddress + dosHeader->e_lfanew;
    auto *ntHeader        = reinterpret_cast<PIMAGE_NT_HEADERS>(ntHeaderAddress);
    if(ntHeader->Signature != IMAGE_NT_SIGNATURE) {
        throw runtime_error(
            format("IMAGE_NT_SIGNATURE not found in header for "
                   "'{}', it contained '{}' instead of '{}'\n{}",
                   dllName, ntHeader->Signature, IMAGE_NT_SIGNATURE, GetLastWin32Error()));
    }
    if(ntHeader->OptionalHeader.NumberOfRvaAndSizes == 0) {
        throw runtime_error(format("NumberOfRvaAndSizes is 0 in header for '{}'", dllName));
    }

    // read the exports
    auto exportsOffset =
        ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
    auto *exportsAddress = baseAddress + exportsOffset;
    auto *exportsHeader  = reinterpret_cast<PIMAGE_EXPORT_DIRECTORY>(exportsAddress);
    if(exportsHeader->AddressOfNames == 0) {
        throw runtime_error(format("AddressOfNames is 0 in exports for '{}'", dllName));
    }

    // read the exported names and store them in our map
    DWORD *names = reinterpret_cast<DWORD *>(baseAddress + exportsHeader->AddressOfNames);
    for(size_t i = 0; i < exportsHeader->NumberOfNames; ++i) {
        const char *funcName          = reinterpret_cast<const char *>(baseAddress + names[i]);
        exports[funcName].funcAddress = GetProcAddress(lib, funcName);
    }

    return exports;
}


#include <iostream>
using std::cout;
using std::endl;

int main(int argc, char **argv) try {
    if(argc > 1) {
    	// list exports of a DLL
        auto mydll = LoadLibraryDyn(argv[1]);
        cout << argv[1] << ":" << mydll.size() << " exports\n";
        for(auto &[name, fn]: mydll) { cout << name << "  "; }
    } else {
        // call a function from user32.dll as an example
        auto user32 = LoadLibraryDyn("user32.dll");
        auto x = user32["MessageBoxA"](nullptr, "Hello, World!", "Hello", MB_YESNO);
		cout << "MessageBoxA returned " << x << endl;
    }

    return 0;
} catch(std::exception &e) {
    cout << "Exception: " << e.what() << endl;
    return 1;
}
