// wintunassist.cpp : 定义控制台应用程序的入口点。
//
#include <Windows.h>
#include <Shlobj.h>

#include "wintun_service.h"
#include "utils.h"

struct MainOption {
	std::string ip;
	int mask;
	std::string dns;
	int forward_port;
	std::wstring adater_name;
	std::wstring adater_pool;
};

bool ParseOption(int argc, char* argv[], MainOption& option) {
	for (int i = 0; i < argc; ++i) {
		if (strcmp(argv[i], "-ip") == 0 && i + 1 < argc) {
			option.ip = argv[i + 1];
			++i;
		}
		else if (strcmp(argv[i], "-mask") == 0 && i + 1 < argc) {
			option.mask = atoi(argv[i + 1]);
			++i;
		}
		else if (strcmp(argv[i], "-dns") == 0 && i + 1 < argc) {
			option.dns = argv[i + 1];
			++i;
		}
		else if (strcmp(argv[i], "-port") == 0 && i + 1 < argc) {
			option.forward_port = atoi(argv[i + 1]);
			++i;
		}
		else if (strcmp(argv[i], "-name") == 0 && i + 1 < argc) {
			option.adater_name = Utils::String2WString(argv[i + 1]);
			++i;
		}
		else if (strcmp(argv[i], "-pool") == 0 && i + 1 < argc) {
			option.adater_pool = Utils::String2WString(argv[i + 1]);
			++i;
		}
	}
	return true;
}

int main(int argc, char* argv[]){
	MainOption option;
	if (!ParseOption(argc, argv, option)) {
		return 1;
	}

	printf("wintun start.\n");
	auto service = std::make_unique<WintunService>();
	int ret = service->Init(option.adater_name, option.adater_pool);
	if (ret != 0) {
		fprintf(stderr, "wintun init error : %d", ret);
		return 2;
	}


	ret = service->Start(option.ip.c_str(), option.mask, option.dns.c_str(), option.forward_port);
	if (ret != 0) {
		fprintf(stderr, "wintun start error : %d", ret);
		return 3;
	}
	HANDLE mutex = OpenMutexA(MUTEX_ALL_ACCESS, FALSE, "a9e3aec4-add8-4acd-a4af-1f86b9f1ca70");
	if (mutex == NULL) {
		fprintf(stderr, "open mutex failed");
		return -1;
	}
	WaitForSingleObject(mutex, INFINITE);
	ReleaseMutex(mutex);
	CloseHandle(mutex);
	service.reset();
	printf("wintun stop.");
    return 0;
}

