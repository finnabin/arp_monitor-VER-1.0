#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <sstream>
#include <chrono>
#include <thread>
#include <regex>
#include <cstdlib>
#include <windows.h>

struct NetworkDevice {
    std::string ip;
    std::string mac;
    std::string name;

    NetworkDevice(const std::string& ip_addr, const std::string& mac_addr, const std::string& device_name = "")
        : ip(ip_addr), mac(mac_addr), name(device_name) {}
};

class ARPMonitor {
private:
    std::vector<NetworkDevice> knownDevices;
    std::map<std::string, std::string> currentDevices;
    // IP -> MAC mapping
    int intervalSeconds;

    std::string executeCommand(const std::string& command) {
        std::string result;
        char buffer[128];

        // _popen opens a pipe
        FILE* pipe = _popen(command.c_str(), "r"); // converts a string from c++ to c, and sets to read mode
        if (!pipe) {
            std::cerr << "Error executing command: " << command << std::endl;
            return "";
        }

        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            result += buffer; // error checking - returns a null pointer when there is no more output
        }

        _pclose(pipe);
        return result; //outputs the FILE* pointer as a string
    }

    //function to parse the arp-a data which is then contained in strings
    void parseArpOutput(const std::string& arpOutput) {
        currentDevices.clear(); //clears the current container of data

        // Regex to match IP and MAC address from arp -a output
        // Format: "  192.168.1.1           00-11-22-33-44-55     dynamic"
        std::regex arpRegex(R"(\s*(\d+\.\d+\.\d+\.\d+)\s+([0-9a-fA-F]{2}-[0-9a-fA-F]{2}-[0-9a-fA-F]{2}-[0-9a-fA-F]{2}-[0-9a-fA-F]{2}-[0-9a-fA-F]{2}))"); //raw string to avoid formatting errors

        //creates a string from the arp output
        std::istringstream stream(arpOutput);
        std::string line;

        //reads the string line by line
        while (std::getline(stream, line)) {
            std::smatch matches;
            if (std::regex_search(line, matches, arpRegex)) {
                std::string ip = matches[1].str();
                std::string mac = matches[2].str();

                // Normalize MAC address to lowercase
                std::transform(mac.begin(), mac.end(), mac.begin(), ::tolower);

                currentDevices[ip] = mac;
            }
        }
    }

    std::string normalizeMac(const std::string& mac) {
        std::string normalized = mac;
        std::transform(normalized.begin(), normalized.end(), normalized.begin(), ::tolower);

        // Convert colons to dashes for consistency
        std::replace(normalized.begin(), normalized.end(), ':', '-');

        return normalized;
    }

public:
    ARPMonitor(int interval = 3) : intervalSeconds(interval) {}

    //function to load known_devices.txt
    bool loadKnownDevices(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Could not open file: " << filename << std::endl;
            return false;
        }

        std::string line;
        while (std::getline(file, line)) {
            if (line.empty() || line[0] == '#') continue; // Skip empty lines and comments

            std::istringstream iss(line);
            std::string ip, mac, name;

            // Expected format: IP,MAC,Name (name is optional)
            if (std::getline(iss, ip, ',') && std::getline(iss, mac, ',')) {
                std::getline(iss, name); // makes name optional

                // Trim whitespace
                ip.erase(0, ip.find_first_not_of(" \t"));
                ip.erase(ip.find_last_not_of(" \t") + 1);
                mac.erase(0, mac.find_first_not_of(" \t"));
                mac.erase(mac.find_last_not_of(" \t") + 1);

                mac = normalizeMac(mac);

                knownDevices.emplace_back(ip, mac, name);
            }
        }

        file.close();
        std::cout << "Loaded " << knownDevices.size() << " known devices from " << filename << std::endl;
        return true;
    }

    //function which performs the arp-a request
    void performArpScan() {
        std::cout << "\n--- Performing ARP scan at " << getCurrentTimeString() << " ---" << std::endl;

        std::string arpOutput = executeCommand("arp -a");
        if (arpOutput.empty()) {
            std::cerr << "Failed to execute ARP command" << std::endl;
            return;
        }

        parseArpOutput(arpOutput);
        std::cout << "Found " << currentDevices.size() << " devices on network" << std::endl;
    }

    //function which compares the known devices string with the arp-a output string
    void compareWithKnownDevices() {
        std::vector<NetworkDevice> newDevices;
        std::vector<NetworkDevice> missingDevices;
        std::vector<NetworkDevice> changedDevices;

        // Check for new or changed devices
        for (const auto& current : currentDevices) {
            bool found = false;
            bool changed = false;

            for (const auto& known : knownDevices) {
                if (known.ip == current.first) {
                    found = true;
                    if (normalizeMac(known.mac) != current.second) {
                        changedDevices.emplace_back(current.first, current.second, "MAC changed from " + known.mac);
                        changed = true;
                    }
                    break;
                }
            }

            if (!found) {
                newDevices.emplace_back(current.first, current.second, "Unknown device");
            }
        }

        // Check for missing devices
        for (const auto& known : knownDevices) {
            if (currentDevices.find(known.ip) == currentDevices.end()) {
                missingDevices.push_back(known);
            }
        }

        // Report findings
        if (!newDevices.empty()) {
            std::cout << "\n*** NEW DEVICES DETECTED ***" << std::endl;
            for (const auto& device : newDevices) {
                std::cout << "  IP: " << device.ip << " | MAC: " << device.mac << std::endl;
            }
        }

        if (!changedDevices.empty()) {
            std::cout << "\n*** DEVICE CHANGES DETECTED ***" << std::endl;
            for (const auto& device : changedDevices) {
                std::cout << "  IP: " << device.ip << " | New MAC: " << device.mac << " | " << device.name << std::endl;
            }
        }

        if (!missingDevices.empty()) {
            std::cout << "\n*** MISSING DEVICES ***" << std::endl;
            for (const auto& device : missingDevices) {
                std::cout << "  IP: " << device.ip << " | MAC: " << device.mac;
                if (!device.name.empty()) {
                    std::cout << " | Name: " << device.name;
                }
                std::cout << std::endl;
            }
        }

        if (newDevices.empty() && changedDevices.empty() && missingDevices.empty()) {
            std::cout << "All known devices present and unchanged." << std::endl;
        }
    }

    //clock used to measure invervals between arp-a requests
    std::string getCurrentTimeString() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);

        char buffer[100];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", std::localtime(&time_t));
        return std::string(buffer);
    }

    //function that begins the arp-a request loop
    void run() {
        std::cout << "Starting ARP Monitor (interval: " << intervalSeconds << " seconds)" << std::endl;
        std::cout << "Monitoring " << knownDevices.size() << " known devices" << std::endl;
        std::cout << "Press Ctrl+C to stop..." << std::endl;

        while (true) {
            performArpScan();
            compareWithKnownDevices();

            std::cout << "\nNext scan in " << intervalSeconds << " seconds..." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(intervalSeconds));
        }
    }

    //function that lets you adjust the time between arp-a requests (BUG - chrono not working properly)
    void setInterval(int seconds) {
        intervalSeconds = seconds;
    }

    //function that outputs the string of known devices
    void printKnownDevices() {
        std::cout << "\nKnown devices:" << std::endl;
        for (const auto& device : knownDevices) {
            std::cout << "  IP: " << device.ip << " | MAC: " << device.mac;
            if (!device.name.empty()) {
                std::cout << " | Name: " << device.name;
            }
            std::cout << std::endl;
        }
    }
};