#pragma once
#include <string>
#include <vector>

class Utils {
public:
	static int Netsh(const std::string& param);
	static int ExecuteCommand(const std::string& app_name, const std::string& param);

	static std::vector<std::string> split(const std::string& content, const std::string& delim);
	static std::wstring String2WString(const std::string& str);
	
};