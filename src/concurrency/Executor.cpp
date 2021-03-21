#include <afina/concurrency/Executor.h>
#include <algorithm>

namespace Afina {
namespace Concurrency {

Executor::Executor(size_t low_watermark, size_t high_watermark,
                   size_t max_queue_size, std::chrono::milliseconds idle_time) {
    _low_watermark = low_watermark;
    _high_watermark = high_watermark;
    _max_queue_size = max_queue_size;
    _idle_time = idle_time;

    if (_low_watermark == 0) {
        _low_watermark = 1;
    }

    if (_high_watermark < _low_watermark) {
        _high_watermark = _low_watermark;
    }

    if (_max_queue_size == 0) {
        _max_queue_size = 1;
    }

    state = State::kRun;
    _occupied_threads = 0;

    for (size_t i = 0; i < _low_watermark; ++i) {
        create_thread();
    }
}

void perform(Executor *executor) {
    while (true) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(executor->mutex);
            if (executor->tasks.empty()) {
                if (executor->state != Executor::State::kRun) {
                    executor->destroy_thread();
                    executor->empty_condition.notify_all();
                    return;
                }
                auto ret = executor->empty_condition.wait_for(lock, executor->_idle_time);
                if (ret == std::cv_status::timeout && executor->threads.size() > executor->_low_watermark) {
                    executor->destroy_thread();
                    return;
                }
                continue;
            } else {
                executor->_occupied_threads += 1;
                task = executor->tasks.front();
                executor->tasks.pop_front();
            }
        }
        task();
        executor->_occupied_threads -= 1;
    }
}

void Executor::Stop(bool await) {
    std::unique_lock<std::mutex> lock(mutex);
    state = State::kStopping;
    if (await) {
        while (!threads.empty()) {
            empty_condition.wait(lock);
        }
    }
    state = State::kStopped;
}

Executor::~Executor() {
    std::unique_lock<std::mutex> lock(mutex);
    if (state != State::kStopped) {
        lock.unlock();
        Stop(true);
    }
}

void Executor::create_thread() {
    std::unique_lock<std::mutex> lock(_threads_lock);
    threads.emplace_back(perform, this);
}

void Executor::destroy_thread() {
    auto this_thread = std::this_thread::get_id();
    std::unique_lock<std::mutex> lock(_threads_lock);
    auto it = std::find_if(threads.begin(),
                           threads.end(),
                           [=](std::thread &t) { return t.get_id() == this_thread; });
    if (it != threads.end()) {
        it->detach();
        threads.erase(it);
    }
}

}
} // namespace Afina
