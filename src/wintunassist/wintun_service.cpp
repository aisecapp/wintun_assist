#include "wintun_service.h"
#include "wintun_ex.h"
#include "wintun_packet_process.h"


WintunService::WintunService() {

}

WintunService::~WintunService() {
	wintun_ex_.reset();
	packet_process_.reset();
}

int WintunService::Init(const std::wstring& adater_name, const std::wstring& adater_pool) {
	wintun_ex_ = std::make_unique<WintunEx>();
	if (!wintun_ex_->Init(adater_name, adater_pool)) {
		return -1;
	}

	return 0;
}

int WintunService::Start(const char* addr,int net_mask, const char* dns, unsigned short forword_port) {
	packet_process_ = std::make_unique<WintunPacketProcess>(wintun_ex_.get(), forword_port);
	if ( !wintun_ex_->Start(addr, net_mask,dns, packet_process_.get())){
		return -1;
	}
	return 0;
}

void WintunService::Stop() {
	wintun_ex_->Stop();
}