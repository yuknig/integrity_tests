#include <conio.h>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <experimental/filesystem>

#include "../Launcher/ProcessUtils.h"

namespace fs = std::experimental::filesystem;

void ModifyHostsFile() {
    // get path to hosts file
    fs::path hosts_path;
    {
        const auto* win_dir = getenv("windir");
        if (!win_dir)
            win_dir = "c:\\Windows\\";
        hosts_path = fs::path(win_dir) / "System32" / "drivers" / "etc" / "hosts";
    }

    // read content of 'hosts' file
    std::vector<std::string> lines;
    {
        std::ifstream ifs(hosts_path.string().c_str(), std::ios_base::in);
        std::string line;
        while (std::getline(ifs, line))
            lines.emplace_back(std::move(line));
    }

    lines.emplace_back(std::string("# dummy line #") + std::to_string(lines.size()));

    // write lines to tmp file and replace 'host' with temporary
    {
        auto tmp_path = fs::temp_directory_path() / "hosts.tmp";
        if (fs::is_regular_file(tmp_path))
            fs::remove(tmp_path);

        {
            std::ofstream ofs(tmp_path.string().c_str(), std::ios_base::out);
            for (const auto& line : lines)
                ofs << line << std::endl;
            ofs.flush();
            ofs.close();
        }

        fs::copy(tmp_path, hosts_path, fs::copy_options::overwrite_existing);
        fs::remove(tmp_path);
    }
}

int main() {
    int return_code = 0;
    std::cout << "===============EditHosts===============\n";
    LogIntegrity();

    try {
        std::cout << "trying modify 'hosts' file...\n";
        ModifyHostsFile();
        std::cout << "'hosts' file updated successfully\n";
    }
    catch (const fs::filesystem_error & e) {
        std::cout << "failed: " << e.what() << std::endl;
        return_code = -1;
    }
    std::cout << "Press a key to exit EditHosts\n";
    _getch();
    std::cout << "Exiting EditHosts\n" << std::flush;

    return return_code;
}
