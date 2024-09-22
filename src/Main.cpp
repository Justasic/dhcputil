#include "vendor/CLI11.hpp"
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

#include "DHCP.h"
#include "Socket.h"

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

	// Build out the DHCP packet

	uint8_t dhcp_packet[300]{0};
	struct DHCPPacket *packet = reinterpret_cast<struct DHCPPacket*>(dhcp_packet);

	packet->op = BOOTREQUEST;    // DHCPREQUEST
	packet->htype = 0x1;         // Ethernet Hardware Type
	packet->hlen = 0x6;          // Hardware Address is our mac address, always 6 bytes long.
	packet->xid = cmdline.xid;   // Client ID
	packet->flags = htons(cmdline.flags);    // Set broadcast flag
	packet->ciaddr = sock.GetInterfaceAddress(); // Set client address (either the current known address or 0.0.0.0)
	packet->cookie = DHCP_COOKIE; // Set the cookie, note this is written in little-endian but compiled to big-endian
	memcpy(packet->chaddr, sock.GetInterfaceHWID().data(), 6); // Copy mac address in.

	// So, this is a bit fun, basically this is some pointer mafs to get the DHCP option array position
	// in the memory space, allowing us to set some basic options.
	struct DHCPOption *optionstart = reinterpret_cast<struct DHCPOption*>(dhcp_packet + sizeof(struct DHCPPacket));
	optionstart->option_id = 53;        // Option ID for DHCP request message type
	optionstart->option_len = 1;        // we're 1 byte big
	optionstart->data[0] = cmdline.mtype; // DHCP Request?
	optionstart++; // lmao next struct!
	optionstart->option_id = 0xFF; // Option End byte.
    
	// Calculate our packet length.
	size_t len = (reinterpret_cast<uint8_t*>(optionstart) + 1) - dhcp_packet;
	printf("Length of packet: %zu\n", len);

	// Send out the broadcast socket.
	ssize_t written = sock.Send(INADDR_BROADCAST, 67, dhcp_packet);

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


	return EXIT_SUCCESS;
}
