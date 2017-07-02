/* XMRig
 * Copyright 2010      Jeff Garzik <jgarzik@pobox.com>
 * Copyright 2012-2014 pooler      <pooler@litecoinpool.org>
 * Copyright 2014      Lucas Jones <https://github.com/lucasjones>
 * Copyright 2014-2016 Wolf9466    <https://github.com/OhGodAPet>
 * Copyright 2016      Jay D Dee   <jayddee246@gmail.com>
 * Copyright 2016-2017 XMRig       <support@xmrig.com>
 *
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


#include <thread>


#include "crypto/CryptoNight.h"
#include "workers/SingleWorker.h"
#include "workers/Workers.h"


SingleWorker::SingleWorker(Handle *handle)
    : Worker(handle)
{
}


void SingleWorker::start()
{
    while (true) {
        if (Workers::isPaused()) {
            do {
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }
            while (Workers::isPaused());

            consumeJob();
        }

        while (!Workers::isOutdated(m_sequence)) {
            if ((m_count & 0xF) == 0) {
                storeStats();
            }

            m_count++;
            *m_job.nonce() = ++m_result.nonce;

            if (CryptoNight::hash(m_job, m_result, m_ctx)) {
                Workers::submit(m_result);
            }

            std::this_thread::yield();
        }

        consumeJob();
    }
}



void SingleWorker::consumeJob()
{
    m_job = Workers::job();
    m_sequence = Workers::sequence();

    memcpy(m_result.jobId, m_job.id(), sizeof(m_result.jobId));
    m_result.poolId = m_job.poolId();
    m_result.diff   = m_job.diff();

    if (m_job.isNicehash()) {
        m_result.nonce = (*m_job.nonce() & 0xff000000U) + (0xffffffU / m_threads * m_id);
    }
    else {
        m_result.nonce = 0xffffffffU / m_threads * m_id;
    }
}