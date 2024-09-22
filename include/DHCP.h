#pragma once
#include <cstdint>
#include <vector>
#include <ranges>
#include "DHCP.h"

// Always set to 99.130.83.99
#define DHCP_COOKIE 0x63538263

/**
   Message         Use
   -------         ---

   DHCPDISCOVER -  Client broadcast to locate available servers.

   DHCPOFFER    -  Server to client in response to DHCPDISCOVER with
                   offer of configuration parameters.

   DHCPREQUEST  -  Client message to servers either (a) requesting
                   offered parameters from one server and implicitly
                   declining offers from all others, (b) confirming
                   correctness of previously allocated address after,
                   e.g., system reboot, or (c) extending the lease on a
                   particular network address.

   DHCPACK      -  Server to client with configuration parameters,
                   including committed network address.

   DHCPNAK      -  Server to client indicating client's notion of network
                   address is incorrect (e.g., client has moved to new
                   subnet) or client's lease as expired

   DHCPDECLINE  -  Client to server indicating network address is already
                   in use.

   DHCPRELEASE  -  Client to server relinquishing network address and
                   cancelling remaining lease.

   DHCPINFORM   -  Client to server, asking only for local configuration
                   parameters; client already has externally configured
                   network address.
 */

enum DHCPMessageType : uint8_t
{
	DHCPDISCOVER = 1,
	DHCPOFFER, 
	DHCPREQUEST,
	DHCPACK,
	DHCPNAK,
	DHCPDECLINE,
	DHCPRELEASE,
	DHCPINFORM
};

/**
 * op            1  Message op code / message type.
                    1 = BOOTREQUEST, 2 = BOOTREPLY
   htype         1  Hardware address type, see ARP section in "Assigned
                    Numbers" RFC; e.g., '1' = 10mb ethernet.
   hlen          1  Hardware address length (e.g.  '6' for 10mb
                    ethernet).
   hops          1  Client sets to zero, optionally used by relay agents
                    when booting via a relay agent.
   xid           4  Transaction ID, a random number chosen by the
                    client, used by the client and server to associate
                    messages and responses between a client and a
                    server.
   secs          2  Filled in by client, seconds elapsed since client
                    began address acquisition or renewal process.
   flags         2  Flags (see figure 2).
   ciaddr        4  Client IP address; only filled in if client is in
                    BOUND, RENEW or REBINDING state and can respond
                    to ARP requests.
   yiaddr        4  'your' (client) IP address.
   siaddr        4  IP address of next server to use in bootstrap;
                    returned in DHCPOFFER, DHCPACK by server.
   giaddr        4  Relay agent IP address, used in booting via a
                    relay agent.
   chaddr       16  Client hardware address.
   sname        64  Optional server host name, null terminated string.
   file        128  Boot file name, null terminated string; "generic"
                    name or null in DHCPDISCOVER, fully qualified
                    directory-path name in DHCPOFFER.
   options     var  Optional parameters field.  See the options
                    documents for a list of defined options.

*/

struct DHCPPacket 
{
	uint8_t op;
	uint8_t htype;
	uint8_t hlen;
	uint8_t hops;
	uint32_t xid;
	uint16_t secs;
	uint16_t flags;
	uint32_t ciaddr;
	uint32_t yiaddr;
	uint32_t siaddr;
	uint32_t giaddr;
	char chaddr[16];
	char sname[64];
	char file[128];
	// Magic cookie, always set to "99.130.83.99"
	uint32_t cookie;
	// After this comes the options.
};

struct __attribute__((packed)) DHCPOption
{
	uint8_t option_id;
	uint8_t option_len;
	// data[1] is just to make warnings shush.
	uint8_t data[1];
};

enum DHCPMessageOperation
{
	BOOTREQUEST = 1,
	BOOTREPLY
};

class DHCPPayload
{
	// The actual data structure.
	std::vector<uint8_t> structuredata_;

	// The DHCP packet structure at the front of the above array.
	struct DHCPPacket *packet_;

	// a list of options instantiated inside this structure
	std::vector<struct DHCPOption*> options_;

public:
	DHCPPayload()
	{
		// Allocate the packet structure.
		structuredata_.resize(sizeof(struct DHCPPacket));
		this->packet_ = reinterpret_cast<struct DHCPPacket*>(this->structuredata_.data());

		// Set some basic data we're almost always going to have.
		this->packet_->op = BOOTREQUEST;
		this->packet_->htype = 0x1; // Ethernet hardware type.
		this->packet_->cookie = DHCP_COOKIE;
	}

	// Not copyable
	DHCPPayload(const DHCPPayload &) = delete;
	DHCPPayload &operator=(const DHCPPayload &) = delete;

	struct DHCPPacket *GetDHCPPakcetStructure() { return this->packet_; }

	// This returns the structured data and resets the structure.
	std::vector<uint8_t> &&GetStructureData()
	{
		// Insert the "End of Options" option byte.
		this->structuredata_.emplace_back(0xFF);
		// Move the data out of the function and this class entirely.
		return std::move(this->structuredata_);
	}

	// Add options to an array for later.
	template <std::ranges::range Range> 
		requires std::convertible_to<std::ranges::range_value_t<std::remove_cvref_t<Range>>, uint8_t>
	void AddOption(uint8_t id, Range &&data)
	{
		size_t length = std::ranges::size(data);
		const auto *ptr = std::ranges::cdata(data);

		// Because we got a pointer to the end of the data, we now need to 
		// allocate space inside the vector for the structure we're about to add.
		this->structuredata_.resize(this->structuredata_.size() + sizeof(struct DHCPOption) + length);

		// Get a pointer to the structure.
struct DHCPOption *opt = reinterpret_cast<struct DHCPOption*>(this->structuredata_.data() + this->structuredata_.size() - (length + sizeof(struct DHCPOption) - 1));

		// Now we can set the values.
		opt->option_id = id;
		opt->option_len = static_cast<decltype(opt->option_len)>(length);
		memcpy(opt->data, ptr, opt->option_len);

		// Add the option to the option array for later.
		this->options_.emplace_back(opt);
	}

	void AddOption(uint8_t id, uint8_t type)
	{
		// Because we got a pointer to the end of the data, we now need to 
		// allocate space inside the vector for the structure we're about to add.
		this->structuredata_.resize(this->structuredata_.size() + sizeof(struct DHCPOption));

		// Get a pointer to the structure.
		struct DHCPOption *opt = reinterpret_cast<struct DHCPOption*>(this->structuredata_.data() + this->structuredata_.size() - sizeof(struct DHCPOption));

		printf("New structure size %d\n", this->structuredata_.size());

		// Now we can set the values.
		opt->option_id = id;
		opt->option_len = 1;
		opt->data[0] = type;

		// Add the option to the option array for later.
		this->options_.emplace_back(opt);
	}
};
