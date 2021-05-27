#pragma once

#include <string>
#include <mutex>


class WintunEx;
class WintunPacketProcess;
class WintunService {
public:
	WintunService();
	virtual ~WintunService();

	int Init(const std::wstring& adater_name, const std::wstring& adater_pool);

	int Start(const char* addr,int net_mask, const char* dns, unsigned short forword_port);
	void Stop();
protected:
	std::unique_ptr<WintunEx> wintun_ex_;
	std::unique_ptr<WintunPacketProcess> packet_process_;
};