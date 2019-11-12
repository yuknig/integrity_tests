#pragma once
#include "HandleWrap.h"
#include <string>

void ShellExecAsUser(const HANDLE& token,
                     const wchar_t* user,
                     const wchar_t* dir,
                     const wchar_t* app_name,
                     const wchar_t* app_params);

void CreateProcAs(const HANDLE& token,
                  const wchar_t* cmd);
