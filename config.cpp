
#include <string>
#include <windows.h>
EXTERN_C IMAGE_DOS_HEADER __ImageBase;

std::string getConfigPath() {
  std::string result = "";
  WCHAR DllPath[MAX_PATH] = {0};

  GetModuleFileNameW((HINSTANCE)&__ImageBase, DllPath, (DWORD)MAX_PATH);

  WCHAR *tmp = wcsrchr(DllPath, L'\\');
  WCHAR wConfigPath[MAX_PATH] = {0};
  size_t path_len = tmp - DllPath;
  wcsncpy(wConfigPath, DllPath, path_len);
  wcscat(wConfigPath, L"\\config.ini");

  char ConfigPath[MAX_PATH] = {0};
  wcstombs(ConfigPath, wConfigPath, sizeof(ConfigPath));

  result.append(ConfigPath);
  return result;
}