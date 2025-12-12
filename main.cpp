#include "header.h"

int main(int argc, char* argv[]) {
    std::string configFile = "known_devices.txt";
    int interval = 30;

    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-f" || arg == "--file") {
            if (i + 1 < argc) {
                configFile = argv[++i];
            }
        } else if (arg == "-i" || arg == "--interval") {
            if (i + 1 < argc) {
                interval = std::atoi(argv[++i]);
                if (interval < 1) interval = 30;
            }
        } else if (arg == "-h" || arg == "--help") {
            std::cout << "Usage: " << argv[0] << " [options]" << std::endl;
            std::cout << "Options:" << std::endl;
            std::cout << "  -f, --file <filename>    Specify known devices file (default: known_devices.txt)" << std::endl;
            std::cout << "  -i, --interval <seconds> Set scan interval (default: 30)" << std::endl;
            std::cout << "  -h, --help              Show this help message" << std::endl;
            return 0;
        }
    }

    ARPMonitor monitor(interval);

    if (!monitor.loadKnownDevices(configFile)) {
        std::cout << "Creating sample configuration file: " << configFile << std::endl;

        // Create a sample configuration file
        std::ofstream sampleFile(configFile);
        sampleFile << "# Known devices configuration file" << std::endl;
        sampleFile << "# Format: IP,MAC,Name (Name is optional)" << std::endl;
        sampleFile << "# Example entries:" << std::endl;
        sampleFile << "# 192.168.1.1,00-11-22-33-44-55,Router" << std::endl;
        sampleFile << "# 192.168.1.100,aa-bb-cc-dd-ee-ff,My Laptop" << std::endl;
        sampleFile << "# 192.168.1.50,12-34-56-78-9a-bc" << std::endl;
        sampleFile.close();

        std::cout << "Please edit " << configFile << " with your known devices and run the program again." << std::endl;
        return 1;
    }

    monitor.printKnownDevices();
    monitor.run();

    return 0;
}
