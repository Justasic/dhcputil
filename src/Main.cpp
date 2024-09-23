#include "vendor/CLI11.hpp"
#include <cstring>
#include <random>
#include <string>
#include <format>
#include <cstdint>
#include <cstdlib>
#include <cassert>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <net/if.h>

#include "DHCP.h"
#include "Socket.h"

// Reference information
// https://networkencyclopedia.com/dhcp-options/
// https://datatracker.ietf.org/doc/html/rfc2131
// https://datatracker.ietf.org/doc/html/rfc2132
// https://www.man7.org/linux/man-pages/man7/netdevice.7.html

class CommandLine
{
public:
	// Options with defaults
	std::string ip = "";
	int seconds = 0;
	uint32_t xid = 0;
	DHCPMessageType mtype{DHCPREQUEST};
	uint16_t flags = 0x8000; // broadcast bit set
	std::string yip = "0.0.0.0";
	std::string gip = "0.0.0.0";
	std::string sname = "";
	std::string fname = "";
	int verbosity = 1;
	int timeout = 5;

	// Options with choices
	std::map<std::string, DHCPMessageType> choices{
		{"discover", DHCPDISCOVER}, 
		{"request", DHCPREQUEST},
		{"decline", DHCPDECLINE},
		{"inform", DHCPINFORM}
	};
	// std::vector<std::string> operation = {"discover", "request", "release", "decline", "inform"};

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

		// Create a random device for getting random numbers.
		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_int_distribution<uint32_t> distrib(1);
		xid = distrib(gen);

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
		// app.add_option("-v,--verbosity", verbosity, "How chatty we should be (default: 1).")->default_val(verbosity);
		app.add_option("-t,--timeout", timeout, "Seconds to wait for any replies before exiting (default: 5).")->default_val(timeout);
		app.add_option("--operation", mtype, "DHCP message type (default: \"request\").")->transform(CLI::CheckedTransformer(choices, CLI::ignore_case));
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

void print_xxd(const std::vector<uint8_t>& vec) {
    for (size_t i = 0; i < vec.size(); i++) {
        // Print offset in hexadecimal and as a separator between bytes and ASCII representation
        if (i % 16 == 0) {
            std::cout << "\n" << std::setfill('0') << std::hex << std::setw(8) << i;
        } else if (i % 2 == 0) {
            std::cout << " ";
        }
        
        // Print byte in hexadecimal and ASCII representation
        uint8_t c = vec[i];
        std::cout << ' ' << std::setfill('0') << std::hex << std::setw(2) 
                  << static_cast<int>(c);
                  
    }
    
    // Print ASCII representation
    for (size_t i = vec.size(); i % 16 != 0; i++) {
        if (i % 8 == 0) {
            std::cout << " ";
        }
        
        std::cout << "  ";
    }
    
    for (const uint8_t& c : vec) {
        char ch = static_cast<char>(c);
        if (std::isprint(ch)) {
            std::cout << ch;
        } else {
            std::cout << '.'; // non-printable characters are replaced with '.'
        }
    }
    
    std::cout << "\n";
}


int main(int argc, char* argv[]) 
{
	CommandLine cmdline;
	if (int ret = cmdline.Parse(argc, argv); ret != 0)
		return ret;

	DHCPSessionSocket sock;
	if (int res = sock.OpenInterface(cmdline.interface); res)
	{
		std::cerr << "Failed to open a new socket: " << strerror(errno) << std::endl;
		return EXIT_FAILURE;
	}

	// Bind to the interface address on port 68 (as client)
	if (!sock.BindSocket(INADDR_ANY, 68))
	{
		perror("bind");
		return EXIT_FAILURE;
	}

	DHCPPayload payload;

	struct DHCPPacket *packet_ = payload.GetDHCPPakcetStructure();

	packet_->hlen   = 6;
	packet_->xid    = cmdline.xid;
	packet_->flags  = htons(cmdline.flags);
	packet_->ciaddr = sock.GetInterfaceAddress();
	memcpy(packet_->chaddr, sock.GetInterfaceHWID().data(), packet_->hlen);

	payload.AddOption(53, cmdline.mtype);
	
	// Send out the broadcast socket.
	ssize_t written = sock.Send(INADDR_BROADCAST, 67, payload.GetStructureData());
	if (written < 0)
	{
		std::cerr << "Failed to send datagram: " << strerror(errno) << std::endl;
		return EXIT_FAILURE;
	}

	std::cerr << "Wrote " << written << " bytes!" << std::endl;

	std::vector<uint8_t> buffer;
	// According to AI this is the average DHCP packet size.
	buffer.reserve(579); 

	ssize_t readdata = sock.Recieve(INADDR_BROADCAST, 68, buffer);
	if (readdata < 0)
	{
		perror("recvfrom");
		return EXIT_FAILURE;
	}

	printf("Received %d bytes of data!\n", buffer.size());

	print_xxd(buffer);

	struct DHCPPacket *packet = reinterpret_cast<struct DHCPPacket*>(buffer.data());

	printf("op: 0x%X\n", packet->op);
	printf("htype: 0x%X\n", packet->htype);
	printf("hlen: %d\n", packet->hlen);
	printf("hops: %d\n", packet->hops);
	printf("xid: 0x%X\n", packet->xid);
	printf("secs: %d\n", packet->secs);
	printf("flags: 0x%X\n", packet->flags);
	printf("ciaddr: %s\n", IPv4ToString(packet->ciaddr).c_str());
	printf("yiaddr: %s\n", IPv4ToString(packet->yiaddr).c_str());
	printf("siaddr: %s\n", IPv4ToString(packet->siaddr).c_str());
	printf("giaddr: %s\n", IPv4ToString(packet->giaddr).c_str());
	printf("chaddr: ");
	for (int i = 0; i < packet->hlen; ++i)
		printf("%0X%c", packet->chaddr[i], (i + 1 == packet->hlen ? '\0' : ':'));
	printf("\nsname: %s\n", packet->sname);
	printf("file: %s\n", packet->file);
	printf("cookie: 0x%X\n\nDHCP Options\n", packet->cookie);

	// Process all the DHCP options.
	std::vector<struct DHCPOption*> options;

	// The DHCP option structures come after the initial DHCP packet header processed
	// above. In this case we have to iterate rather carefully to find all the structures.
	struct DHCPOption *optstart = reinterpret_cast<struct DHCPOption*>(packet + 1);

	// Make sure this is not DHCP options end.
	while (optstart->option_id != 0xFF)
	{
		// Start by getting the size of a structure (3 bytes)
		// we subtract 1 because the compiler will assume that
		// `uint8_t data[1]` is 1 byte but that is incorrect.
		size_t len = sizeof(struct DHCPOption) - 1 + optstart->option_len;
		// Sanity check
		assert(len < buffer.size() && "length calculated to be longer than the buffer???");
		// Next, we read the structure and figure out what additional
		// length needs to be read/parsed. At this stage, we can also
		// print the info to the console.
		printf("Option %d: ", optstart->option_id);
		for (int i = 0; i < optstart->option_len; ++i)
			printf("%X", optstart->data[i]);
		printf("\n");

		// Now move to the next one.
		options.emplace_back(optstart);
		optstart = reinterpret_cast<struct DHCPOption*>(reinterpret_cast<uint8_t*>(optstart) + len);
	}

	return EXIT_SUCCESS;
}
