#include "wintun_ex.h"
#include <ws2tcpip.h>
#include "utils.h"

static WINTUN_CREATE_ADAPTER_FUNC WintunCreateAdapter;
static WINTUN_DELETE_ADAPTER_FUNC WintunDeleteAdapter;
static WINTUN_DELETE_POOL_DRIVER_FUNC WintunDeletePoolDriver;
static WINTUN_ENUM_ADAPTERS_FUNC WintunEnumAdapters;
static WINTUN_FREE_ADAPTER_FUNC WintunFreeAdapter;
static WINTUN_OPEN_ADAPTER_FUNC WintunOpenAdapter;
static WINTUN_GET_ADAPTER_LUID_FUNC WintunGetAdapterLUID;
static WINTUN_GET_ADAPTER_NAME_FUNC WintunGetAdapterName;
static WINTUN_SET_ADAPTER_NAME_FUNC WintunSetAdapterName;
static WINTUN_GET_RUNNING_DRIVER_VERSION_FUNC WintunGetRunningDriverVersion;
static WINTUN_SET_LOGGER_FUNC WintunSetLogger;
static WINTUN_START_SESSION_FUNC WintunStartSession;
static WINTUN_END_SESSION_FUNC WintunEndSession;
static WINTUN_GET_READ_WAIT_EVENT_FUNC WintunGetReadWaitEvent;
static WINTUN_RECEIVE_PACKET_FUNC WintunReceivePacket;
static WINTUN_RELEASE_RECEIVE_PACKET_FUNC WintunReleaseReceivePacket;
static WINTUN_ALLOCATE_SEND_PACKET_FUNC WintunAllocateSendPacket;
static WINTUN_SEND_PACKET_FUNC WintunSendPacket;

//a9e3aec4 - add8 - 4acd - a4af - 1f86b9f1ca70
static const GUID WINTUN_GUID = { 0xa9e3aec4, 0xadd8, 0x4acd,{ 0xa4, 0xaf, 0x1f, 0x86, 0xb9, 0xf1, 0xca, 0x70 } };


static void CALLBACK 
ConsoleLogger(_In_ WINTUN_LOGGER_LEVEL Level, _In_z_ const WCHAR *LogLine) {
	char log[1024] = { 0 };
	snprintf(log, 1024, "%ws", LogLine);
	if (Level == WINTUN_LOG_ERR) {
		fprintf(stderr, "%s\n", log);
	}
	else {
		printf("%s\n", log);
	}
}

static void
Log(_In_ WINTUN_LOGGER_LEVEL Level, _In_z_ const WCHAR *Format, ...)
{
	WCHAR LogLine[0x200];
	va_list args;
	va_start(args, Format);
	_vsnwprintf_s(LogLine, _countof(LogLine), _TRUNCATE, Format, args);
	va_end(args);
	ConsoleLogger(Level, LogLine);
}


static DWORD
LogError(_In_z_ const WCHAR *Prefix, _In_ DWORD Error)
{
	WCHAR *SystemMessage = NULL, *FormattedMessage = NULL;
	DWORD_PTR args[] = {(DWORD_PTR)Prefix, (DWORD_PTR)Error, (DWORD_PTR)SystemMessage};

	FormatMessageW(
		FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_MAX_WIDTH_MASK,
		NULL,
		HRESULT_FROM_SETUPAPI(Error),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPWSTR)&SystemMessage,
		0,
		NULL);
	FormatMessageW(
		FORMAT_MESSAGE_FROM_STRING | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_ARGUMENT_ARRAY |
		FORMAT_MESSAGE_MAX_WIDTH_MASK,
		SystemMessage ? L"%1: %3(Code 0x%2!08X!)" : L"%1: Code 0x%2!08X!",
		0,
		0,
		(LPWSTR)&FormattedMessage,
		0,
		(va_list *)args);
	if (FormattedMessage)
		ConsoleLogger(WINTUN_LOG_ERR, FormattedMessage);
	LocalFree(FormattedMessage);
	LocalFree(SystemMessage);
	return Error;
}

WintunEx::WintunEx(){

	packet_prcoess_ = NULL;
	dll_moudule_ = NULL;
	adapter_ = NULL;
	session_ = NULL;
	receive_event_ = NULL;
	adapter_index_ = -1;

	WORD ver = MAKEWORD(2, 2);//版本
	WSADATA dat;
	WSAStartup(ver, &dat);
}

WintunEx::~WintunEx() {
	Stop();

	if (adapter_) {
		WintunFreeAdapter(adapter_);
		adapter_ = NULL;
	}

	if (dll_moudule_) {
		FreeLibrary(dll_moudule_);
		dll_moudule_ = NULL;
	}
	WSACleanup();
}

bool WintunEx::Init(const std::wstring& adater_name, const std::wstring& adater_pool) {
	if (!InitializeWintun()) {
		return false;
	}
	WintunSetLogger(ConsoleLogger);
	adapter_ = WintunOpenAdapter(adater_pool.c_str(), adater_name.c_str());
	if (!adapter_){
		adapter_ = WintunCreateAdapter(adater_pool.c_str(), adater_name.c_str(), &WINTUN_GUID, NULL);
		if (!adapter_){
			DWORD error = GetLastError();
			LogError(L"Failed to create adapter", error);
			return false;
		}
	}
	DWORD Version = WintunGetRunningDriverVersion();
	Log(WINTUN_LOG_INFO, L"Wintun v%u.%u loaded", (Version >> 16) & 0xff, (Version >> 0) & 0xff);

	return true;
}

void WintunEx::ResetNetConfig() {

	PMIB_UNICASTIPADDRESS_TABLE pIpTable = NULL;
	DWORD LastError = GetUnicastIpAddressTable(AF_INET, &pIpTable);
	if (LastError != NO_ERROR) {
		return;
	}
	MIB_UNICASTIPADDRESS_ROW AddressRow;
	InitializeUnicastIpAddressEntry(&AddressRow);
	WintunGetAdapterLUID(adapter_, &AddressRow.InterfaceLuid);
	for (ULONG i = 0; i < pIpTable->NumEntries; ++i) {
		if (pIpTable->Table[i].InterfaceLuid.Value != AddressRow.InterfaceLuid.Value) {
			continue;
		}
		DeleteUnicastIpAddressEntry(&pIpTable->Table[i]);
	}
}

int WintunEx::SetMtu(int mtu) {
	MIB_IPINTERFACE_ROW ipiface;
	InitializeIpInterfaceEntry(&ipiface);
	ipiface.Family = AF_INET;
	ipiface.InterfaceIndex = adapter_index_;

	DWORD err = GetIpInterfaceEntry(&ipiface);
	if (err == NO_ERROR) {
		ipiface.SitePrefixLength = 0;
		ipiface.NlMtu = mtu;
		err = SetIpInterfaceEntry(&ipiface);
	}
	if (err != NO_ERROR) {
		return -1;
	}
	return 0;
}

void WintunEx::SetDns(const std::string& dns) {
	auto dnses = Utils::split(dns, ",");
	for(size_t i = 0; i < dnses.size(); ++i){
		ConfigDns(dnses[i], i == 0);
	}
}


int WintunEx::ConfigDns(const std::string& dns_ip, bool primary) {
	char command[256] = { 0 };
	if (primary){
		sprintf_s(command, sizeof(command), "interface ipv4 set dnsservers %lu static %s primary validate=no", adapter_index_, dns_ip.c_str());
	}
	else {
		sprintf_s(command, sizeof(command), "interface ipv4 add dnsservers %lu %s validate=no", adapter_index_, dns_ip.c_str());
	}
	return Utils::Netsh(command);
}

int WintunEx::DelDns() {
	char command[256];
	sprintf_s(command, sizeof(command), "interface ipv4 set dnsservers %lu static none", adapter_index_);
	return Utils::Netsh(command);
}

bool WintunEx::Start(const std::string& ip, int mask, const std::string& dns, IWintunPakcetProcess* packet_process) {
	packet_prcoess_ = packet_process;
	if (!adapter_) {
		return false;
	}
	ResetNetConfig();

	MIB_UNICASTIPADDRESS_ROW AddressRow;
	InitializeUnicastIpAddressEntry(&AddressRow);
	WintunGetAdapterLUID(adapter_, &AddressRow.InterfaceLuid);
	AddressRow.Address.Ipv4.sin_family = AF_INET;
	inet_pton(AF_INET, ip.c_str(), (void*)&AddressRow.Address.Ipv4.sin_addr.S_un.S_addr);
	//AddressRow.Address.Ipv4.sin_addr.S_un.S_addr = htonl((10 << 24) | (6 << 16) | (7 << 8) | (7 << 0)); /* 10.6.7.7 */
	AddressRow.OnLinkPrefixLength = mask; /* This is a /24 network */
	DWORD LastError = CreateUnicastIpAddressEntry(&AddressRow);
	if (LastError != ERROR_SUCCESS && LastError != ERROR_OBJECT_ALREADY_EXISTS)
	{
		LogError(L"Failed to set IP address", LastError);
		return false;
	}

	PMIB_IPINTERFACE_TABLE pipTable = NULL;
	LastError = GetIpInterfaceTable(AF_INET, &pipTable);
	if (LastError != NO_ERROR) {
		LogError(L"Failed to GetIpInterfaceTable", LastError);
		return false;
	}
	for (ULONG i = 0; i < pipTable->NumEntries; ++i) {
		if (pipTable->Table[i].InterfaceLuid.Value == AddressRow.InterfaceLuid.Value) {
			adapter_index_ = pipTable->Table[i].InterfaceIndex;
		}
	}

	SetMtu(1500);
	DelDns();
	SetDns(dns);

	session_ = WintunStartSession(adapter_, WINTUN_MAX_RING_CAPACITY/2);
	if (!session_)
	{
		LogError(L"Failed to start wintun session", GetLastError());
		return false;
	}

	receive_event_ = CreateEventW(NULL, TRUE, FALSE, NULL);
	if (!receive_event_) {
		LogError(L"Failed to create receive event", GetLastError());
		return false;
	}
	if (packet_prcoess_ == nullptr) {
		return false;
	}

	StartReceive();
	return true;
}
void WintunEx::Stop() {
	StopReceive();
	if (session_) {
		WintunEndSession(session_);
		session_ = NULL;
	}
	if (receive_event_) {
		CloseHandle(receive_event_);
		receive_event_ = NULL;
	}
}

void WintunEx::ReceivePacket() {
	HANDLE WaitHandles[] = { WintunGetReadWaitEvent(session_), receive_event_ };
	while (receive_){
		DWORD PacketSize;
		BYTE *Packet = WintunReceivePacket(session_, &PacketSize);
		if (Packet){
			if (packet_prcoess_->ProcessPacket(Packet, PacketSize)) {
				SendPacket(Packet, PacketSize);
			}
			WintunReleaseReceivePacket(session_, Packet);
			continue;
		}
		DWORD LastError = GetLastError();
		switch (LastError)
		{
		case ERROR_NO_MORE_ITEMS: 
			if (WaitForMultipleObjects(_countof(WaitHandles), WaitHandles, FALSE, INFINITE) == WAIT_OBJECT_0) {
				continue;
			}
			break;
		default:
			LogError(L"Packet read failed: %d", LastError);
		}
	}
}

void  WintunEx::SendPacket(const unsigned char* data, int length) {
	BYTE *Packet = WintunAllocateSendPacket(session_, length);
	if (Packet){
		memcpy(Packet, data, length);
		WintunSendPacket(session_, Packet);
	}
}

void WintunEx::StartReceive() {
	receive_ = true;
	ResetEvent(receive_event_);
	receive_thread_ = std::make_unique<std::thread>(std::bind(&WintunEx::ReceivePacket, this));
}


void WintunEx::StopReceive(){
	receive_ = false;
	SetEvent(receive_event_);
	if (receive_thread_ && receive_thread_->joinable()) {
		receive_thread_->join();
	}
}


bool WintunEx::InitializeWintun() {
	if (dll_moudule_) {
		return true;
	}
	HMODULE Wintun =
		LoadLibraryExW(L"wintun.dll", NULL, LOAD_LIBRARY_SEARCH_APPLICATION_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
	if (!Wintun)
		return NULL;
#define X(Name, Type) ((Name = (Type)GetProcAddress(Wintun, #Name)) == NULL)
	if (X(WintunCreateAdapter, WINTUN_CREATE_ADAPTER_FUNC) || X(WintunDeleteAdapter, WINTUN_DELETE_ADAPTER_FUNC) ||
		X(WintunDeletePoolDriver, WINTUN_DELETE_POOL_DRIVER_FUNC) || X(WintunEnumAdapters, WINTUN_ENUM_ADAPTERS_FUNC) ||
		X(WintunFreeAdapter, WINTUN_FREE_ADAPTER_FUNC) || X(WintunOpenAdapter, WINTUN_OPEN_ADAPTER_FUNC) ||
		X(WintunGetAdapterLUID, WINTUN_GET_ADAPTER_LUID_FUNC) ||
		X(WintunGetAdapterName, WINTUN_GET_ADAPTER_NAME_FUNC) ||
		X(WintunSetAdapterName, WINTUN_SET_ADAPTER_NAME_FUNC) ||
		X(WintunGetRunningDriverVersion, WINTUN_GET_RUNNING_DRIVER_VERSION_FUNC) ||
		X(WintunSetLogger, WINTUN_SET_LOGGER_FUNC) || X(WintunStartSession, WINTUN_START_SESSION_FUNC) ||
		X(WintunEndSession, WINTUN_END_SESSION_FUNC) || X(WintunGetReadWaitEvent, WINTUN_GET_READ_WAIT_EVENT_FUNC) ||
		X(WintunReceivePacket, WINTUN_RECEIVE_PACKET_FUNC) ||
		X(WintunReleaseReceivePacket, WINTUN_RELEASE_RECEIVE_PACKET_FUNC) ||
		X(WintunAllocateSendPacket, WINTUN_ALLOCATE_SEND_PACKET_FUNC) || X(WintunSendPacket, WINTUN_SEND_PACKET_FUNC))
#undef X
	{
		DWORD LastError = GetLastError();
		FreeLibrary(Wintun);
		SetLastError(LastError);
		return NULL;
	}
	dll_moudule_ = Wintun;
	return true;
}



