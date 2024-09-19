#include "vendor/CLI11.hpp"
#include <string>
#include <cstdint>
#include <cstdlib>


int main(int argc, char* argv[]) {
    CLI::App app("dhcputil");

    // Required options
    app.add_option("-i", "Network interface to use.", true)->required()->check(CLI::ExistingFile);

    // Options with defaults
    std::string ip = "";
    int seconds = 0;
    std::string xid = "";
    uint16_t flags = 0x8000; // broadcast bit set
    std::string yip = "0.0.0.0";
    std::string gip = "0.0.0.0";
    std::string sname = "";
    std::string fname = "";
    int verbosity = 1;
    int timeout = 5;

    app.add_option("-c,--client-ip", "Client IP address.", ip)->default_val(ip);
    app.add_option("-s,--seconds", "Seconds since client began acquisition process.", seconds)->default_val(seconds);
    app.add_option("--xid", "Set transaction ID to xid.", xid)->default_val(xid);
    app.add_option("--flags", "Bootp flags (uint16).", flags)->default_val(flags);
    app.add_option("-y,--your-ip", "Your (client) IP address.", yip)->default_val(yip);
    app.add_option("-g,--gateway-ip", "Gateway/relay agent IP address.", gip)->default_val(gip);
    app.add_option("--server-name", "Server name string.", sname)->default_val(sname);
    app.add_option("--client-boot-file", "Client boot file name string.", fname)->default_val(fname);
    app.add_option("-v,--verbosity", "How chatty we should be (default: 1).", verbosity)->default_val(verbosity);
    app.add_option("-t,--timeout", "Seconds to wait for any replies before exiting (default: 5).", timeout)->default_val(timeout);

    // Options with choices
    std::vector<std::string> operation = {"discover", "request", "release", "decline", "inform"};
    int opt = 1; // default operation is request

    app.add_option("--operation", "DHCP message type (default: \"request\").", opt)->choices(operation);

    // Options with values
    std::vector<std::pair<std::string, std::string>> sip = {{"--sip", ""}};
    std::string sip_value = "";

    app.add_option("-S,--server-ip", "Server IP address (gotten from OFFER, default: 0.0.0.0).", sip_value)->default_val(sip_value);

    // Options with multiple values
    std::vector<std::pair<std::string, std::string>> dhcp_opts = {{"-X", ""}};
    std::string dhcp_opts_values;

    app.add_option("-X,--dhcp-opt", "DHCP option (e.g. -X 50=c0a80189).", dhcp_opts_values);

    // Options with existing values
    int reply_cnt = 0; // default is unlimited

    app.add_option("--reply-cnt", "Maximum number of replies to wait for before exiting.", reply_cnt)->default_val(reply_cnt);

    // IPv4 options
    std::string src_ip = "";
    std::string dst_ip = "";
    int ttl = 0;
    uint8_t tos = 0;

    app.add_option("-F,--src-ip", "Send IP datagram from this source IP address.", src_ip)->default_val(src_ip);
    app.add_option("-T,--dst-ip", "Send IP datagram to this destination IP address.", dst_ip)->default_val(dst_ip);
    app.add_option("--ttl", "Use this TTL value for outgoing datagrams.", ttl)->default_val(ttl);
    app.add_option("--tos", "Use this type-of-service value for outgoing datagrams.", tos)->default_val(tos);

    // Ethernet options
    std::string dst_ether = "";

    app.add_option("-E,--dst-ether", "Use this destination MAC address (default: ff:ff:ff:ff:ff:ff).", dst_ether)->default_val(dst_ether);

    try {
        CLI11::App app("dhcptool");
        // ... (rest of code)
    } catch (const std::exception& e) {
        return EXIT_FAILURE;
    }

    return app.run();
}
