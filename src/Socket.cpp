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
	int s = state;
	if (setsockopt(this->sock_, SOL_SOCKET, option, &s, sizeof(s)) < 0)
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
	if (!this->SetSocketOption(SO_BROADCAST, true))
	{
		std::cerr << "Failed to set the socket to a broadcast IP capable socket" << std::endl;
		return errno;
	}

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
	if (!this->SetSocketOption(SO_BINDTODEVICE, this->interface_))
	{
		std::cerr << "Failed to bind to device " << this->interface_ <<std::endl;
		return errno;
	}

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


ssize_t DHCPSessionSocket::Recieve(in_addr_t ipaddr, in_port_t port, std::vector<uint8_t> &buf)
{
	// Sockaddr to know who we received data from
	sockaddrs sa;
	socklen_t slen = sizeof(struct sockaddr_in);
	buf.resize(1024);

	ssize_t datasz = recvfrom(this->sock_, buf.data(), buf.capacity(), 0, &sa.sa, &slen);

	if (datasz < 0)
		return datasz;
	else if (datasz == 0)
	{
		// Connection closed by peer??
	}
	else if (datasz == buf.capacity())
	{
		// Expand the buffer and re-read data.
		buf.resize(buf.capacity()*2);
		ssize_t moredatasz = recvfrom(this->sock_, buf.data()+datasz, buf.capacity(), 0, &sa.sa, &slen);
		if (moredatasz < 0)
			return moredatasz;

		datasz += moredatasz;
	}

	// Inform the vector of it's element size now.
	buf.resize(datasz);

	return datasz;
}
