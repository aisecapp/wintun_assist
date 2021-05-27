#pragma once
#include "wintun_ex.h"
#include "wintun_service.h"
#include <unordered_map>
#include <mutex>

struct ck_iphdr
{
#if BYTE_ORDER == LITTLE_ENDIAN
	uint8_t		ihl : 4,
		version : 4;
#elif BYTE_ORDER == BIG_ENDIAN
	uint8_t		version : 4,
		ihl : 4;
#endif
	uint8_t		tos;
	uint16_t	tot_len;
	uint16_t	id;
	uint16_t	frag_off;
	uint8_t		ttl;
	uint8_t		protocol;
	uint16_t	check;
	uint32_t	saddr;
	uint32_t	daddr;
	/* the options start here. */
};

struct ck_tcphdr {
	uint16_t source;
	uint16_t dest;
	uint32_t seq;
	uint32_t ack_seq;
	//#  if __BYTE_ORDER == __LITTLE_ENDIAN
	uint16_t res1 : 4;
	uint16_t doff : 4;
	uint16_t fin : 1;
	uint16_t syn : 1;
	uint16_t rst : 1;
	uint16_t psh : 1;
	uint16_t ack : 1;
	uint16_t urg : 1;
	uint16_t res2 : 2;
	/*#  elif __BYTE_ORDER == __BIG_ENDIAN
	uint16_t doff:4;
	uint16_t res1:4;
	uint16_t res2:2;
	uint16_t urg:1;
	uint16_t ack:1;
	uint16_t psh:1;
	uint16_t rst:1;
	uint16_t syn:1;
	uint16_t fin:1;
	#  else
	#   error "Adjust your <bits/endian.h> defines"
	#  endif*/
	uint16_t window;
	uint16_t check;
	uint16_t urg_ptr;
};

struct ck_udphdr {
	uint16_t source; //16位源端口号
	uint16_t dest;  //16位目的端口号
	uint16_t len;   //指udp首部长度和udp数据的长度总和长度  
	uint16_t check; //udp校验和，校验的是udp首部和upd数据的总的校验和
};


class WintunPacketProcess: public IWintunPakcetProcess {
public:
	explicit WintunPacketProcess(IWintunSender* wintun_sender, uint16_t fowrd_port);
	virtual ~WintunPacketProcess();

public:
	bool ProcessPacket(unsigned char* packet_data, int packet_size) override;

protected:
	bool ProcessTcpPacket(unsigned char* packet_data, int packet_size);
	bool ProcessIpPacket(unsigned char* packet_data, int packet_size);

	void SendIpPacket(unsigned char* packet_data, int packet_size);
	void SendTcpPort(uint32_t dst_addr, uint16_t source, uint16_t dest);

	bool ProcessAppPacket(struct ck_iphdr *iphdr, struct ck_tcphdr *tcphdr, int tcplen, uint16_t transfer_port);
	bool ProcessMiniServerPacket(struct ck_iphdr *iphdr, struct ck_tcphdr *tcphdr, int tcplen);

	uint16_t TcpChecksum(unsigned char *tcphead, int tcplen, unsigned int *srcaddr, unsigned int *dstaddr);
	uint16_t IpChecksum(unsigned char *hdr, int len);
	uint16_t PartChecksum(uint16_t init_sum, unsigned char *buffer, int len);

protected:
	uint16_t forword_port_;
	IWintunSender* wintun_sender_;
	std::unordered_map<uint16_t, uint16_t> tcp_port_map_;

	SOCKET udp_socket_;

};