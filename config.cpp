
#include <string>

#ifdef _WIN32
    #include <windows.h>
    EXTERN_C IMAGE_DOS_HEADER __ImageBase;
#else
    #include <fcntl.h>
    #include <dlfcn.h>
#endif

std::string getConfigPath() {
    std::string result = "";
#ifdef _WIN32
    WCHAR DllPath[MAX_PATH] = { 0 };

    GetModuleFileNameW((HINSTANCE)&__ImageBase, DllPath, (DWORD)MAX_PATH);

    WCHAR *tmp = wcsrchr(DllPath, L'\\');
    WCHAR wConfigPath[MAX_PATH] = { 0 };
    size_t path_len = tmp - DllPath;
    wcsncpy(wConfigPath, DllPath, path_len);
    wcscat(wConfigPath, L"\\config.ini");

    char ConfigPath[MAX_PATH] = { 0 };
    wcstombs(ConfigPath, wConfigPath, sizeof(ConfigPath));

    result.append(ConfigPath);
#else
    Dl_info PathToSharedObject;
    void * pointer = reinterpret_cast<void*> (getRobotModuleObject);
    dladdr(pointer, &PathToSharedObject);
    std::string dltemp(PathToSharedObject.dli_fname);

    int dlfound = dltemp.find_last_of("/");

    dltemp = dltemp.substr(0, dlfound);
    dltemp += "/config.ini";

    result.append(dltemp.c_str());
#endif
    return result;
}