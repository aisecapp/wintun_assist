#include "utils.h"
#include <Windows.h>
#include <direct.h>
#include <io.h>

int Utils::Netsh(const std::string& param) {
	char app_name[256] = { 0 };
	GetSystemDirectoryA(app_name, sizeof(app_name));
	strcat_s(app_name, sizeof(app_name), "\\NETSH.EXE");
	return ExecuteCommand(app_name, param);
}

int Utils::ExecuteCommand(const std::string& app_name, const std::string& param) {
	STARTUPINFOA si;
	memset(&si, 0, sizeof(si));
	PROCESS_INFORMATION pi;
	memset(&pi, 0, sizeof(pi));

	si.cb = sizeof(si);
	char cmd_line[1024] = { 0 };
	snprintf(cmd_line, sizeof(cmd_line), "%s %s", app_name.c_str(), param.c_str());
	if (!CreateProcessA(app_name.c_str(), cmd_line, NULL, NULL, si.hStdOutput != NULL,
		CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
		DWORD err = GetLastError();
		return -1;
	}
	WaitForSingleObject(pi.hThread, INFINITE);
	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);

	return 0;
}

std::vector<std::string> Utils::split(const std::string& content, const std::string& delim) {
	std::vector<std::string> s;
	if (delim.empty() || content.empty()) {
		return s;
	}

	size_t pos = 0;
	while (pos < content.length()) {
		size_t begin = pos;
		std::string tmp;
		pos = content.find(delim, pos);
		if (pos == std::string::npos) {
			tmp = content.substr(begin, pos - begin);
		}
		else {
			tmp = content.substr(begin, pos - begin);
			pos += delim.length();
		}
		if (!tmp.empty()) {
			s.emplace_back(tmp);
		}
	}
	return s;
}

std::wstring Utils::String2WString(const std::string& str){
	std::wstring result;
	int len = MultiByteToWideChar(CP_ACP, 0, str.c_str(), str.size(), NULL, 0);
	WCHAR* buffer = new WCHAR[len + 1];
	//¶à×Ö½Ú±àÂë×ª»»³É¿í×Ö½Ú±àÂë  
	MultiByteToWideChar(CP_ACP, 0, str.c_str(), str.size(), buffer, len);
	buffer[len] = '\0';
	result.append(buffer);
	delete[] buffer;
	return result;
}
