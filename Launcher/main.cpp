#include "CmdParams.h"
#include "Logon.h"
#include "RunProcess.h"
#include "ProcessUtils.h"

#include <conio.h>
#include <iostream>
#include <experimental/filesystem>

namespace fs = std::experimental::filesystem;

struct RunParams {
    RunParams(const CmdParams& config) {
        if (!config.ParamExists("exe"))
            throw std::runtime_error("missing parameter 'exe'");

        m_run_as_admin = config.ParamExists("run_as_admin");

        if (config.ParamExists("integrity"))
            m_run_integrity = IntegrityStringToEnum(config.GetParam("integrity"));
        if (IntegrityLevel::Other == m_run_integrity)
            m_run_integrity = GetCurrentIntegrity();

        m_exe = config.GetParam("exe");
        m_exe_args = config.ParamExists("exe_args") ? config.GetParam("exe_args") : "";
    }

    bool m_run_as_admin = false;
    IntegrityLevel m_run_integrity = IntegrityLevel::Other;
    std::string m_exe;
    std::string m_exe_args;
};

void RestartSelfAsAdmin(const std::string& cmd_args) {
    auto logon = LogonInteractively();
    if (!logon)
        throw std::runtime_error("failed to logon");

    fs::path exe_path = GetCurrentExePath();
    ShellExecAsUser(static_cast<HANDLE>(logon->m_logon_token),
                    logon->m_user.data(),
                    exe_path.parent_path().c_str(),
                    exe_path.filename().c_str(),
                    ToUnicode(cmd_args).c_str());
}

int main(int argc, char* argv[]) {
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    std::cout << "===============Launcher================\n";
    LogIntegrity();

    const CmdParams cmd(argc, argv);
    const RunParams params(cmd);
    std::cout << "Run parameters:\n";
    std::cout << "  run_as_admin=" << params.m_run_as_admin << std::endl;
    std::cout << "  integrity=" << IntegrityEnumToString(params.m_run_integrity) << std::endl;
    std::cout << "  exe='" << params.m_exe << "'" << std::endl;
    std::cout << "  exe args='" << params.m_exe_args << "'" << std::endl;

    std::cout << "Trying to create process..\n" << std::flush;
    if (params.m_run_as_admin && !IsAdministrator()) {
        std::cout << "Restarting launcher as admin...\n";
        auto args = cmd.ToMap();
        args.erase("run_as_admin");
        RestartSelfAsAdmin(CmdParams(args).ToString());
    }
    else {
        fs::path exe_path = GetCurrentExePath();
        // assuming that launched exe is in the same directory as launcher
        std::wstring cmd_line = (exe_path.parent_path() / params.m_exe).wstring() + L" " + ToUnicode(params.m_exe_args);
        IntegrityScope i(params.m_run_integrity);
        CreateProcAs(i.GetToken(), cmd_line.c_str());
    }

    std::cout << "Press any key to exit Launcher\n";
    _getch();
    std::cout << "Exiting Launcher\n";

    return 0;
}
