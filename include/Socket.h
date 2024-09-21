#pragma once
#include <array>
#include <ranges>
#include <cstdint>
#include <format>
#include <netinet/in.h>
#include <sys/types.h>

union sockaddrs
{
	struct sockaddr_in in;
	struct sockaddr sa;
};

constexpr std::string MACAddressToString(const struct sockaddr *sa) 
{
    if (sa->sa_family == AF_UNSPEC)
        return "";

    const unsigned char *mac = reinterpret_cast<const unsigned char*>(sa->sa_data);
	return std::format("{:2X}:{:2X}:{:2X}:{:2X}:{:2X}:{:2X}", 
			mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

constexpr std::string IPv4ToString(in_addr_t address)
{
    return std::format("{}.{}.{}.{}", 
		(address      & 0xFF),
		(address >> 8 & 0xFF),
		(address >> 16 & 0xFF),
        (address >> 24 & 0xFF)
    );
}

template <std::ranges::range Range> 
	requires std::convertible_to<std::ranges::range_value_t<std::remove_cvref_t<Range>>, uint8_t>
constexpr std::optional<uint32_t> ToIPv4(Range &&range)
{
    std::ranges::subrange rest{ range };
    uint32_t sin_addr{0}, num{0}, byte{0};

    for (; !rest.empty() && byte <= 3; rest.advance(1))
    {
        const uint8_t ch = static_cast<uint8_t>(*rest.begin());

        // Verify the character is one of the valid ones
        if ((ch < '0' || ch > '9') && ch != '\0' && ch != '.')
            return std::nullopt;

        if (ch == '.')
        {
            sin_addr |= num << (byte++ * 8);
            num = 0;
        }
        else if (ch == '\0')
            continue;
        else
            num = num * 10 + ch - '0';
    }

    return sin_addr | num << (byte * 8);
}


/**
 * Because we're so simple and don't do anything asynchronously
 * and use blocking sockets, we can wrap the session management
 * and socket operations into one class.
 */
class DHCPSessionSocket
{
	// The socket itself.
	int sock_{-1};

	// The interface we're bound to.
	std::string interface_;

	// The current IP address of the interface.
	in_addr_t interface_ip_;

	// The hardware ID (Mac address) of the interface.
	std::array<uint8_t, 6> hardware_id_;

public:
	DHCPSessionSocket() = default;

	// Not copyable
	DHCPSessionSocket(const DHCPSessionSocket &) = delete;
	DHCPSessionSocket &operator=(const DHCPSessionSocket &) = delete;

	// Can be moved
	constexpr DHCPSessionSocket(DHCPSessionSocket &&rhs) noexcept : 
		sock_(rhs.sock_), interface_(std::move(rhs.interface_))
	{ 
		rhs.sock_ = -1; 
	}

	constexpr DHCPSessionSocket &operator=(DHCPSessionSocket &&rhs) noexcept
	{
		if (this == &rhs) [[unlikely]]
			return *this;

		// Move/copy data
		this->sock_ = rhs.sock_;

		// Reset stuff that cannot be moved back to sane values.
		rhs.sock_ = -1;

		return *this;
	}

	// Getters
	constexpr in_addr_t GetInterfaceAddress() const noexcept { return this->interface_ip_; }
	constexpr std::array<uint8_t, 6> GetInterfaceHWID() const noexcept { return this->hardware_id_; }
	constexpr std::string_view GetInterface() const noexcept { return this->interface_; }

	bool SetSocketOption(int option, bool state);

	template <std::ranges::range Range> 
		requires std::convertible_to<std::ranges::range_value_t<std::remove_cvref_t<Range>>, uint8_t>
	bool SetSocketOption(int option, Range &&range)
	{
		const uint8_t *data = reinterpret_cast<const uint8_t*>(std::ranges::cdata(range));
		size_t length = std::ranges::size(range);
		if (setsockopt(this->sock_, SOL_SOCKET, option, data, static_cast<socklen_t>(length)) < 0)
		{
			perror("setsock_opt");
			return false;
		}
		return true;
	}

	// Open a sock_et.
	int OpenInterface(std::string iface);
	bool BindSocket(in_addr_t ipaddr, in_port_t port);
	bool BindSocket(std::string_view bindaddr, in_port_t port);

	template <std::ranges::range Range> 
		requires std::convertible_to<std::ranges::range_value_t<std::remove_cvref_t<Range>>, uint8_t>
	ssize_t Send(in_addr_t ipaddr, in_addr_t port, Range &&data)
	{
		sockaddrs sa;
		sa.in.sin_family = AF_INET;
		sa.in.sin_port = htons(port);
		sa.in.sin_addr.s_addr = ipaddr;

		const uint8_t *pdata = reinterpret_cast<const uint8_t*>(std::ranges::cdata(data));
		size_t length = std::ranges::size(data);

		return ::sendto(this->sock_, pdata, length, 0, &sa.sa, sizeof(struct sockaddr_in));
	}

	template <std::ranges::range Range> 
		requires std::convertible_to<std::ranges::range_value_t<std::remove_cvref_t<Range>>, uint8_t>
	ssize_t Send(std::string_view ipaddr, in_addr_t port, Range &&data)
	{
		auto address = ToIPv4(ipaddr);
		if (!address)
		{
			errno = EINVAL;
			return -1;
		}

		return this->Send(*address, port, std::move(data));
	}
};
