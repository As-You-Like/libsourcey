///
//
// LibSourcey
// Copyright (c) 2005, Sourcey <http://sourcey.com>
//
// SPDX-License-Identifier:	LGPL-2.1+
//
/// @addtogroup sked
/// @{


#include "scy/sked/scheduler.h"
#include "scy/datetime.h"
#include "scy/logger.h"
#include "scy/platform.h"
#include "scy/singleton.h"

#include "assert.h"
#include <algorithm>


using namespace std;


namespace scy {
namespace sked {


Scheduler::Scheduler()
{
}


Scheduler::~Scheduler()
{
}


void Scheduler::schedule(sked::Task* task)
{
    TaskRunner::start(task);
    //_wakeUp.set();
}


void Scheduler::cancel(sked::Task* task)
{
    TaskRunner::cancel(task);
    //_wakeUp.set();
}


void Scheduler::clear()
{
    TaskRunner::clear();
    //_wakeUp.set();
}


void Scheduler::run()
{
    // TraceS(this) << "Running" << endl;
    // while (!_stopped)
    //{

    // Update and sort the task list bringing the
    // next scheduled task to the front of the list.
    // TODO: Call only after task run, and when the
    // initial task is nullptr.
    update();

    // TODO: Create a nextTask member so we don't
    // need to call next() on each iteration
    auto task = reinterpret_cast<sked::Task*>(next());

    // Run the task
    if (task && task->trigger().timeout()) {
#if _DEBUG
        {
            DateTime now;
            Timespan remaining = task->trigger().scheduleAt - now;
            TraceS(this) << "Waiting: "
                         << "\n\tPID: " << task
                         << "\n\tDays: " << remaining.days()
                         << "\n\tHours: " << remaining.totalHours()
                         << "\n\tMinutes: " << remaining.totalMinutes()
                         << "\n\tSeconds: " << remaining.totalSeconds()
                         << "\n\tMilliseconds: "
                         << remaining.totalMilliseconds() << "\n\tCurrentTime: "
                         << DateTimeFormatter::format(
                                now, DateTimeFormat::ISO8601_FORMAT)
                         << "\n\tScheduledAt: "
                         << DateTimeFormatter::format(
                                task->trigger().scheduleAt,
                                DateTimeFormat::ISO8601_FORMAT)
                         << endl;
        }
#endif

        // Wait for the scheduled timeout
        // if (!task->trigger().timeout())
        //    _wakeUp.tryWait(static_cast<long>(task->trigger().remaining()));

        // The task list may have changed during the timeout
        // duration, or the current task deleted, so we need
        // to ensure that the next pending task matches the
        // current pending task.
        if ( // task == next() &&
            // task->trigger().timeout() &&
            task->beforeRun()) {
#if _DEBUG
            {
                DateTime now;
                TraceS(this)
                    << "Running: "
                    << "\n\tPID: " << task << "\n\tCurrentTime: "
                    << DateTimeFormatter::format(now,
                                                 DateTimeFormat::ISO8601_FORMAT)
                    << "\n\tScheduledTime: "
                    << DateTimeFormatter::format(task->trigger().scheduleAt,
                                                 DateTimeFormat::ISO8601_FORMAT)
                    << endl;
            }
#else
            TraceS(this) << "Running: " << task << endl;
#endif
            task->run();
            if (task->afterRun())
                onRun(task);
            else {
                TraceS(this) << "Destroy After Run: " << task << endl;
                task->_destroyed = true; // destroy();
            }
        } else
            TraceS(this) << "Skipping Task: " << task << endl;

        // Destroy the task if needed
        if (task->destroyed()) {
            TraceS(this) << "Destroy Task: " << task << endl;
            assert(remove(task));
            delete task;
        }

        // TraceS(this) << "Running: OK: " << task << endl;
    }

    // Go to sleep if we have no tasks
    // else {
    //    TraceS(this) << "Sleeping" << endl;
    //    _wakeUp.wait();
    //    TraceS(this) << "Waking up" << endl;
    //}

    // Prevent 100% CPU
    scy::sleep(3);

    // Dispatch the Idle signal
    // TODO: Send Idle complete iteration of all tasks,
    // rather than after each task.
    // Idle.emit(/*this*/);
    //}

    // TraceS(this) << "Shutdown" << endl;
    // Shutdown.emit(/*this*/);
    // TraceS(this) << "Exiting" << endl;
}


void Scheduler::update()
{
    Mutex::ScopedLock lock(_mutex);

    // TraceS(this) << "Updating: " << _tasks.size() << endl;

    // Update and clean the task list
    auto it = _tasks.begin();
    while (it != _tasks.end()) {
        sked::Task* task = reinterpret_cast<sked::Task*>(*it);
        if (task->destroyed()) {
            it = _tasks.erase(it);
            onRemove(task);
            TraceS(this) << "Destroy: " << task << endl;
            delete task;
        } else
            ++it;
    }

    // Sort the task list to the next task to
    // trigger is at the front of the queue.
    sort(_tasks.begin(), _tasks.end(), sked::Task::CompareTimeout);
}


void Scheduler::serialize(json::Value& root)
{
    TraceS(this) << "Serializing" << endl;

    Mutex::ScopedLock lock(_mutex);
    for (auto it = _tasks.begin(); it != _tasks.end(); ++it) {
        sked::Task* task = reinterpret_cast<sked::Task*>(*it);
        TraceS(this) << "Serializing: " << task << endl;
        json::Value& entry = root[root.size()];
        task->serialize(entry);
        task->trigger().serialize(entry["trigger"]);
    }
}


void Scheduler::deserialize(json::Value& root)
{
    TraceS(this) << "Deserializing" << endl;

    for (auto it = root.begin(); it != root.end(); it++) {
        sked::Task* task = nullptr;
        sked::Trigger* trigger = nullptr;
        try {
            json::assertMember(*it, "trigger");
            task = factory().createTask((*it)["type"].asString());
            task->deserialize((*it));
            trigger =
                factory().createTrigger((*it)["trigger"]["type"].asString());
            trigger->deserialize((*it)["trigger"]);
            task->setTrigger(trigger);
            schedule(task);
        } catch (std::exception& exc) {
            if (task)
                delete task;
            if (trigger)
                delete trigger;
            ErrorS(this) << "Deserialization Error: " << exc.what() << endl;
        }
    }
}


void Scheduler::print(std::ostream& ost)
{
    json::StyledWriter writer;
    json::Value data;
    serialize(data);
    ost << writer.write(data);
}


Scheduler& Scheduler::getDefault()
{
    static Singleton<Scheduler> sh;
    return *sh.get();
}


TaskFactory& Scheduler::factory()
{
    return TaskFactory::getDefault();
}


} // namespace sked
} // namespace scy


/// @\}
