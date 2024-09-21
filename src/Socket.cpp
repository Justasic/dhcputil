#include "vendor/expected.hpp"
#include <asm-generic/socket.h>
#include <cstdint>
#include <ranges>
#include <string_view>
#include <sys/types.h>
#include <utility>
#include <string>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <iostream>
#include "Socket.h"


bool DHCPSessionSocket::SetSocketOption(int option, bool state)
{
	if (setsockopt(this->sock_, SOL_SOCKET, option, &state, sizeof(state)) < 0)
	{
		perror("setsockopt");
		return false;
	}
	return true;
}

int DHCPSessionSocket::OpenInterface(std::string iface)
{
	this->sock_ = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock_ == -1)
		return errno;

	// Set that we're a broadcast socket capable of sending to 
	// 255.255.255.255 and such
	if (this->SetSocketOption(SO_BROADCAST, true))
		return errno;

	// Move our interface name copy into ourselves
	this->interface_ = std::move(iface);

	// Get the interface mac address
	struct ifreq ifr;
	strcpy(ifr.ifr_name, this->interface_.c_str());
	if (ioctl(sock_, SIOCGIFHWADDR, &ifr) < 0)
	{
		std::cerr << "Failed to call ioctl for interface: " << strerror(errno) << std::endl;
		return errno;
	}

	// Copy said mac address.
	memcpy(this->hardware_id_.data(), ifr.ifr_hwaddr.sa_data, 6);

	// Bind to this specific ethernet device.
	if (this->SetSocketOption(SO_BINDTODEVICE, this->interface_))
		return errno;

	// Get the interface IP address if available
	struct ifreq ifaceip;
	strcpy(ifaceip.ifr_name, ifr.ifr_name);
	if (ioctl(sock_, SIOCGIFADDR, &ifaceip) < 0)
	{
		// Set the IP address to 0 if unavailable
		memset(&ifaceip, 0, sizeof(struct ifreq));
		std::cout << "No address on " << ifr.ifr_name << ": " << strerror(errno) << std::endl;
	}

	// Copy the ip address
	this->interface_ip_ = reinterpret_cast<struct sockaddr_in*>(&ifaceip.ifr_addr)->sin_addr.s_addr;

	return 0;
}

bool DHCPSessionSocket::BindSocket(in_addr_t ipaddr, in_port_t port)
{
	sockaddrs bindable;
	bindable.in.sin_family = AF_INET;
	bindable.in.sin_port = htons(port);
	bindable.in.sin_addr.s_addr = ipaddr;

	if (int res = bind(this->sock_, &bindable.sa, sizeof(struct sockaddr)); res)
	{
		perror("bind");
		return false;
	}

	return true;
}

bool DHCPSessionSocket::BindSocket(std::string_view bindaddr, in_port_t port)
{
	auto address = ToIPv4(bindaddr);
	if (!address)
		return false;

	return this->BindSocket(*address, port);
}
