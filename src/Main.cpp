#include "vendor/CLI11.hpp"
#include <asm-generic/socket.h>
#include <cstring>
#include <random>
#include <string>
#include <format>
#include <cstdint>
#include <cstdlib>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <net/if.h>

class CommandLine
{
public:
	// Options with defaults
	std::string ip = "";
	int seconds = 0;
	std::string xid = "";
	std::string choice = "request";
	uint16_t flags = 0x8000; // broadcast bit set
	std::string yip = "0.0.0.0";
	std::string gip = "0.0.0.0";
	std::string sname = "";
	std::string fname = "";
	int verbosity = 1;
	int timeout = 5;

	// Options with choices
	std::vector<std::string> operation = {"discover", "request", "release", "decline", "inform"};

	// Options with values
	std::vector<std::pair<std::string, std::string>> sip = {{"--sip", ""}};
	std::string sip_value = "";

	// Options with multiple values
	std::vector<std::pair<std::string, std::string>> dhcp_opts = {{"-X", ""}};
	std::string dhcp_opts_values;

	// Options with existing values
	int reply_cnt = 0; // default is unlimited
					  
	// IPv4 options
	std::string src_ip = "";
	std::string dst_ip = "";
	int ttl = 0;
	uint8_t tos = 0;

	// Ethernet options
	std::string dst_ether = "";

	std::string interface{""};

	int Parse(int argc, char **argv)
	{
		CLI::App app("dhcputil");

		// Required options
		app.add_option("-i", interface, "Network interface to use.")->required();
		app.add_option("-c,--client-ip", ip, "Client IP address.")->default_val(ip);
		app.add_option("-s,--seconds", seconds, "Seconds since client began acquisition process.")->default_val(seconds);
		app.add_option("--xid", xid, "Set transaction ID to xid.")->default_val(xid);
		app.add_option("--flags", flags,"Bootp flags (uint16).")->default_val(flags);
		app.add_option("-y,--your-ip", yip, "Your (client) IP address.")->default_val(yip);
		app.add_option("-g,--gateway-ip", gip, "Gateway/relay agent IP address.")->default_val(gip);
		app.add_option("--server-name", sname, "Server name string.")->default_val(sname);
		app.add_option("--client-boot-file", fname, "Client boot file name string.")->default_val(fname);
		app.add_option("-v,--verbosity", verbosity, "How chatty we should be (default: 1).")->default_val(verbosity);
		app.add_option("-t,--timeout", timeout, "Seconds to wait for any replies before exiting (default: 5).")->default_val(timeout);
		app.add_option("--operation", choice, "DHCP message type (default: \"request\").");
		app.add_option("-S,--server-ip", sip_value, "Server IP address (gotten from OFFER, default: 0.0.0.0).")->default_val(sip_value);
		app.add_option("-X,--dhcp-opt", dhcp_opts_values, "DHCP option (e.g. -X 50=c0a80189).");
		app.add_option("--reply-cnt", reply_cnt, "Maximum number of replies to wait for before exiting.")->default_val(reply_cnt);
		app.add_option("-F,--src-ip", src_ip, "Send IP datagram from this source IP address.")->default_val(src_ip);
		app.add_option("-T,--dst-ip", dst_ip, "Send IP datagram to this destination IP address.")->default_val(dst_ip);
		app.add_option("--ttl", ttl, "Use this TTL value for outgoing datagrams.")->default_val(ttl);
		// app.add_option("--tos", tos, "Use this type-of-service value for outgoing datagrams.")->default_val(tos);
		app.add_option("-E,--dst-ether", dst_ether, "Use this destination MAC address (default: ff:ff:ff:ff:ff:ff).")->default_val(dst_ether);

		CLI11_PARSE(app, argc, argv);

		return 0;
	}
};

/**
 *op            1  Message op code / message type.
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

struct __attribute__((packed)) DHCPPacket 
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

enum DHCPMessageType
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

union sockaddrs
{
	struct sockaddr_in in;
	struct sockaddr sa;
};

void print_mac(const struct sockaddr *sa) 
{
    if (sa->sa_family == AF_UNSPEC)
        return;

    const unsigned char *mac = reinterpret_cast<const unsigned char*>(sa->sa_data);
    for (int i = 0; i < 6; ++i) 
	{
        printf("%02x", mac[i]);
        if (i != 5)
            std::printf(":");
    }
	printf("\n");
}

std::string IPv4ToString(in_addr_t address)
{
    return std::format("{}.{}.{}.{}", 
		(address      & 0xFF),
		(address >> 8 & 0xFF),
		(address >> 16 & 0xFF),
        (address >> 24 & 0xFF)
    );
}
int main(int argc, char* argv[]) 
{
	CommandLine cmdline;
	if (int ret = cmdline.Parse(argc, argv); ret != 0)
		return ret;

	std::ostream_iterator<char> out(std::cout);
	std::ostream_iterator<char> err(std::cerr);

	int sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock == -1)
	{
		std::cerr << "Failed to open UDP socket: " << strerror(errno) << std::endl;
		return EXIT_FAILURE;
	}

	int broadcast = 1;
	if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0)
	{
		std::cerr << "Failed to set socket flag for broadcast: " << strerror(errno) << std::endl;
		return EXIT_FAILURE;
	}

	struct ifreq ifr;
	strcpy(ifr.ifr_name, cmdline.interface.c_str());
	if (ioctl(sock, SIOCGIFHWADDR, &ifr) < 0)
	{
		std::cerr << "Failed to call ioctl for interface: " << strerror(errno) << std::endl;
		return EXIT_FAILURE;
	}

	// Bind to this specific ethernet device.
	if (setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE, cmdline.interface.c_str(), cmdline.interface.length()) < 0)
	{
		std::cerr << "Failed to bind to " << cmdline.interface << ": " << strerror(errno) << std::endl;
		return EXIT_FAILURE;
	}

	// Get the interface IP address if available
	struct ifreq ifaceip;
	strcpy(ifaceip.ifr_name, ifr.ifr_name);
	if (ioctl(sock, SIOCGIFADDR, &ifaceip) < 0)
	{
		// Set the IP address to 0
		memset(&ifaceip, 0, sizeof(struct ifreq));
		std::cout << "No address on " << ifr.ifr_name << ": " << strerror(errno) << std::endl;
	}

	// // Bind to 0.0.0.0:68
	sockaddrs client;
	client.in.sin_family = AF_INET;
	client.in.sin_port = htons(68);
	client.in.sin_addr.s_addr = reinterpret_cast<struct sockaddr_in*>(&ifaceip.ifr_addr)->sin_addr.s_addr;
	if (int res = bind(sock, &client.sa, sizeof(struct sockaddr)); res)
	{
		std::cerr << "Failed to bind to " << ifr.ifr_name << " as " << IPv4ToString(client.in.sin_addr.s_addr) << ":" << 
				ntohs(client.in.sin_port) << ": " << strerror(errno) << std::endl;
		return EXIT_FAILURE;
	}
	
	std::format_to(out, "Bound to {} as {}:{}", ifr.ifr_name, IPv4ToString(client.in.sin_addr.s_addr), ntohs(client.in.sin_port));

	// Create a random device for getting random numbers.
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<uint32_t> distrib(1);

	uint8_t dhcp_packet[300]{0};
	struct DHCPPacket *packet = reinterpret_cast<struct DHCPPacket*>(dhcp_packet);

	packet->op = 0x1;            // DHCPREQUEST
	packet->htype = 0x1;         // Ethernet Hardware Type
	packet->hlen = 0x6;          // Hardware Address is our mac address, always 6 bytes long.
	packet->xid = distrib(gen);  // Client ID
	packet->flags = htons(1 << 15);    // Set broadcast flag
	packet->ciaddr = client.in.sin_addr.s_addr; // Set client address (either the current known address or 0.0.0.0)
	packet->cookie = 0x63538263; // Set the cookie, note this is written in little-endian but compiled to big-endian
	memcpy(packet->chaddr, ifr.ifr_hwaddr.sa_data, 6); // Copy mac address in.

	print_mac(&ifr.ifr_hwaddr);

	std::cout << " | ";
	for (int i = 0, c = sizeof(dhcp_packet); i < c; ++i)
	{
	    std::cout << std::hex << std::setw(2) << std::setfill('0') << (unsigned)dhcp_packet[i] << " ";
        if ((i + 1) % 32 == 0) {
            std::cout << "\n" << " | ";
        }
	}
	std::cout << std::endl;

	// So, this is a bit fun, basically this is some pointer mafs to get the DHCP option array position
	// in the memory space, allowing us to set some basic options.
	struct DHCPOption *optionstart = reinterpret_cast<struct DHCPOption*>(dhcp_packet + sizeof(struct DHCPPacket));
	optionstart->option_id = 53;        // Option ID for DHCP request message type
	optionstart->option_len = 1;        // we're 1 byte big
	optionstart->data[0] = DHCPREQUEST; // DHCP Request?
	optionstart++; // lmao next struct!
	optionstart->option_id = 0xFF; // Option End byte.
    
	// Calculate our packet length.
	size_t len = (reinterpret_cast<uint8_t*>(optionstart) + 1) - dhcp_packet;
	printf("Length of packet: %zu\n", len);

	sockaddrs sa;
	sa.in.sin_family = AF_INET;
	sa.in.sin_port = htons(67);
	sa.in.sin_addr.s_addr = 0xFFFFFFFF;
	ssize_t written = sendto(sock, dhcp_packet, len, 0, &sa.sa, sizeof(struct sockaddr_in));

	if (written < 0)
	{
		std::cerr << "Failed to send datagram: " << strerror(errno) << std::endl;
		return EXIT_FAILURE;
	}

	std::cerr << "Wrote " << written << " bytes!" << std::endl;


	return EXIT_SUCCESS;
}
