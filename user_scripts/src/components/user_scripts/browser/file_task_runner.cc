/*
    This file is part of Bromite.

    Bromite is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Bromite is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Bromite. If not, see <https://www.gnu.org/licenses/>.
*/

#include "file_task_runner.h"

#include "base/sequenced_task_runner.h"
#include "base/task/lazy_thread_pool_task_runner.h"
#include "base/task/task_traits.h"

namespace user_scripts {

namespace {

base::LazyThreadPoolSequencedTaskRunner g_us_task_runner =
    LAZY_THREAD_POOL_SEQUENCED_TASK_RUNNER_INITIALIZER(
        base::TaskTraits(base::MayBlock(),
                         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN,
                         base::TaskPriority::USER_VISIBLE));

}  // namespace

scoped_refptr<base::SequencedTaskRunner> GetUserScriptsFileTaskRunner() {
  return g_us_task_runner.Get();
}

}  // namespace user_scripts
