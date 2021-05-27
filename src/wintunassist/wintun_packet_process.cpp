#include "wintun_packet_process.h"

WintunPacketProcess::WintunPacketProcess(IWintunSender* wintun_sender, uint16_t forward_port)
	:forword_port_(forward_port)
	,wintun_sender_(wintun_sender){
	udp_socket_ = 0;
}

WintunPacketProcess::~WintunPacketProcess() {
	if (udp_socket_ > 0) {
		closesocket(udp_socket_);
	}
}


bool WintunPacketProcess::ProcessPacket(unsigned char* packet_data, int packet_size) {
	struct ck_iphdr* iphdr = (struct ck_iphdr*)(packet_data);
	if (iphdr->version != 4) {
		//ignore
		return false;
	}

	if (iphdr->protocol == IPPROTO_TCP){
		return ProcessTcpPacket(packet_data, packet_size);
	}
	else{
		return ProcessIpPacket(packet_data, packet_size);
	}
}

bool WintunPacketProcess::ProcessTcpPacket(unsigned char* packet_data, int packet_size) {
	uint16_t port = htons(forword_port_);
	struct ck_iphdr* iphdr = (struct ck_iphdr*)(packet_data);
	struct ck_tcphdr* tcphdr = (struct ck_tcphdr *)(packet_data + iphdr->ihl * 4);
	int tcplen = ntohs(iphdr->tot_len) - iphdr->ihl * 4;
	if (tcphdr->source != port){
		// App 发出的包
		return ProcessAppPacket(iphdr, tcphdr, tcplen, port);
	}
	else{
		// miniserver服务发出的包,端口地址转换
		return ProcessMiniServerPacket(iphdr, tcphdr, tcplen);
	}
}

bool WintunPacketProcess::ProcessAppPacket(struct ck_iphdr *iphdr, struct ck_tcphdr *tcphdr, int tcplen, uint16_t transfer_port) {
	// App 发出的包
	if (tcphdr->syn){
		tcp_port_map_[tcphdr->source] = tcphdr->dest;
		SendTcpPort(iphdr->saddr, tcphdr->source, tcphdr->dest);
	}

	uint32_t tmp_saddr = iphdr->saddr;
	iphdr->saddr = iphdr->daddr;
	iphdr->daddr = tmp_saddr;
	tcphdr->dest = transfer_port;

	iphdr->check = 0;
	iphdr->check = IpChecksum((uint8_t *)iphdr, iphdr->ihl * 4);

	tcphdr->check = 0;
	tcphdr->check = TcpChecksum((uint8_t *)tcphdr, tcplen, &iphdr->saddr, &iphdr->daddr);
	return true;
}

bool WintunPacketProcess::ProcessMiniServerPacket(struct ck_iphdr *iphdr, struct ck_tcphdr *tcphdr, int tcplen) {
	auto it = tcp_port_map_.find(tcphdr->dest);
	if (it == tcp_port_map_.end()) {
		return false;
	}
	// miniserver服务发出的包,端口地址转换
	uint32_t tmp_addr = iphdr->saddr;
	iphdr->saddr = iphdr->daddr;
	iphdr->daddr = tmp_addr;

	
	tcphdr->source = it->second;


	iphdr->check = 0;
	iphdr->check = IpChecksum((uint8_t *)iphdr, iphdr->ihl * 4);

	tcphdr->check = 0;
	tcphdr->check = TcpChecksum((uint8_t *)tcphdr, tcplen, &iphdr->saddr, &iphdr->daddr);

	return true;
}

bool WintunPacketProcess::ProcessIpPacket(unsigned char* packet_data, int packet_size) {
	struct ck_iphdr* iphdr = (struct ck_iphdr*)(packet_data);
	if (iphdr->protocol == IPPROTO_UDP) {
		struct ck_udphdr* udphdr = (struct ck_udphdr *)(packet_data + iphdr->ihl * 4);
		if (udphdr->source == htons(forword_port_)) {
			int head_length = iphdr->ihl * 4 + sizeof(ck_udphdr);
			wintun_sender_->SendPacket(packet_data + head_length, packet_size - head_length);
			return false;
		}
	}
	SendIpPacket(packet_data, packet_size);
	return false;
}

void WintunPacketProcess::SendIpPacket(unsigned char* packet_data, int packet_size) {
	if (udp_socket_ <= 0) {
		udp_socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (udp_socket_ <= 0) {
			return;
		}
	}

	struct ck_iphdr* iphdr = (struct ck_iphdr*)(packet_data);

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(forword_port_);
	addr.sin_addr.s_addr = iphdr->saddr;
	sendto(udp_socket_, (const char*)packet_data, packet_size, 0, (sockaddr*)&addr, sizeof(addr));
}

void WintunPacketProcess::SendTcpPort(uint32_t dst_addr, uint16_t source, uint16_t dest) {
	if (udp_socket_ <= 0) {
		udp_socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (udp_socket_ <= 0) {
			return;
		}
	}

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(forword_port_+1);
	addr.sin_addr.s_addr = dst_addr;

	uint16_t data[2] = { source, dest };
	sendto(udp_socket_, (const char*)data, sizeof(data), 0, (sockaddr*)&addr, sizeof(addr));
}


uint16_t WintunPacketProcess::TcpChecksum(unsigned char *tcphead, int tcplen, unsigned int *srcaddr, unsigned int *dstaddr) {
	unsigned char pseudoheader[12];
	unsigned short calccksum;

	memcpy(&pseudoheader[0], srcaddr, 4);
	memcpy(&pseudoheader[4], dstaddr, 4);
	pseudoheader[8] = 0; /* 填充零 */
	pseudoheader[9] = 6;
	pseudoheader[10] = (tcplen >> 8) & 0xFF;
	pseudoheader[11] = (tcplen & 0xFF);

	calccksum = PartChecksum(0, pseudoheader, sizeof(pseudoheader));
	calccksum = PartChecksum(calccksum, tcphead, tcplen);
	return htons(~calccksum);
}

uint16_t WintunPacketProcess::IpChecksum(unsigned char *hdr, int len) {
	return htons(~PartChecksum(0, hdr, len));
}

uint16_t WintunPacketProcess::PartChecksum(uint16_t init_sum, unsigned char *buffer, int len) {
	ULONG checksum = init_sum;
	for (; len >1; len -= 2, buffer += 2) {
		checksum += ((unsigned long)buffer[0] << 8) + ((unsigned long)buffer[1]);
	}
	if (len) {
		checksum += ((unsigned long)buffer[0] << 8);
	}
	while (checksum >> 16) {
		checksum = (checksum & 0xFFFF) + (checksum >> 16);
	}
	return checksum;
}