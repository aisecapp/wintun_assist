#pragma once
#include <winsock2.h>
#include <Windows.h>
#include <ws2ipdef.h>
#include <iphlpapi.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <thread>
#include "wintun.h"


class IWintunSender {
public:
	virtual void SendPacket(const unsigned char* data, int length) = 0;
};


class IWintunPakcetProcess {
public:
	virtual bool ProcessPacket(unsigned char* packet_data, int packet_size) = 0;
};

class WintunEx : public IWintunSender {
public:
	WintunEx();
	virtual ~WintunEx();

	bool Init(const std::wstring& adater_name, const std::wstring& adater_pool);

	bool Start(const std::string& ip, int mask, const std::string& dns, IWintunPakcetProcess* packet_process);
	void Stop();

	void StartReceive();
	void StopReceive();

	void  SendPacket(const unsigned char* data, int length) override;
protected:
	bool InitializeWintun();
	void ReceivePacket();
	void ResetNetConfig();
	int SetMtu(int mtu);
	void SetDns(const std::string& dns);
	int ConfigDns(const std::string& dns_ip, bool primary);
	int DelDns();

private:
	HMODULE dll_moudule_;
	WINTUN_ADAPTER_HANDLE adapter_;
	ULONG adapter_index_;
	WINTUN_SESSION_HANDLE session_;
	volatile bool receive_;
	HANDLE receive_event_;
	std::unique_ptr<std::thread> receive_thread_;
	IWintunPakcetProcess* packet_prcoess_;
};
