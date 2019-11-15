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
        m_do_logon = config.ParamExists("do_logon");

        if (config.ParamExists("integrity"))
            m_run_integrity = IntegrityStringToEnum(config.GetParam("integrity"));
        if (IntegrityLevel::Other == m_run_integrity)
            m_run_integrity = GetCurrentIntegrity();

        m_exe = config.GetParam("exe");
        m_exe_args = config.ParamExists("exe_args") ? config.GetParam("exe_args") : "";
    }

    bool m_run_as_admin = false;
    bool m_do_logon = false;
    IntegrityLevel m_run_integrity = IntegrityLevel::Other;
    std::string m_exe;
    std::string m_exe_args;
};

void LogParams(const RunParams& params) {
    std::cout << "Run parameters:\n";
    std::cout << "  run_as_admin=" << params.m_run_as_admin << std::endl;
    if (params.m_run_as_admin)
        std::cout << "  do_logon=" << params.m_do_logon << std::endl;
    std::cout << "  integrity=" << IntegrityEnumToString(params.m_run_integrity) << std::endl;
    std::cout << "  exe='" << params.m_exe << "'" << std::endl;
    std::cout << "  exe args='" << params.m_exe_args << "'" << std::endl;
}

void RestartSelfAsAdmin(const std::string& cmd_args, bool logon_in_console) {
    fs::path exe_path = GetCurrentExePath();
    const std::wstring dir = exe_path.parent_path();
    const std::wstring app = exe_path.filename();
    const std::wstring arg = ToUnicode(cmd_args);

    if (logon_in_console) {
        auto logon = LogonInteractively();
        if (!logon)
            throw std::runtime_error("failed to logon");

        ShellExecAsUser(static_cast<HANDLE>(logon->m_logon_token),
                        logon->m_user.data(),
                        dir.c_str(),
                        app.c_str(),
                        arg.c_str());
    }
    else {
        ShellExecAsCurUser(dir.c_str(),
                           app.c_str(),
                           arg.c_str());
    }
}

int main(int argc, char* argv[]) {
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    std::cout << "===============Launcher================\n";
    LogIntegrity();

    const CmdParams cmd(argc, argv);
    const RunParams params(cmd);
    LogParams(params);

    std::cout << "Trying to create process..\n" << std::flush;
    if (params.m_run_as_admin && !IsAdministrator()) {
        std::cout << "Restarting launcher as admin...\n";
        auto args = cmd.ToMap();
        args.erase("run_as_admin");
        RestartSelfAsAdmin(CmdParams(args).ToString(), params.m_do_logon);
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
