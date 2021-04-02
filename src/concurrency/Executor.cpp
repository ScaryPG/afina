#include <afina/concurrency/Executor.h>
#include <algorithm>

namespace Afina {
namespace Concurrency {

Executor::Executor(size_t low_watermark, size_t high_watermark,
                   size_t max_queue_size, std::chrono::milliseconds idle_time) {
    _low_watermark = std::max(low_watermark, 1LU);
    _high_watermark = std::max(high_watermark, _low_watermark);
    _max_queue_size = std::max(max_queue_size, 1LU);
    _idle_time = idle_time;

    state = State::kRun;
    _occupied_threads = 0;

    {
        std::unique_lock<std::mutex> lock(mutex);
        for (size_t i = 0; i < _low_watermark; ++i) {
            create_thread();
        }
    }
}

void perform(Executor *executor) noexcept {
    while (executor->state == Executor::State::kRun || !executor->tasks.empty()) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(executor->mutex);
            if (executor->tasks.empty()) {
                auto ret = executor->empty_condition.wait_for(lock, executor->_idle_time);
                if (ret == std::cv_status::timeout && executor->threads.size() > executor->_low_watermark) {
                    break;
                }
                if (executor->tasks.empty()) {
                    continue;
                }
            }
            executor->_occupied_threads += 1;
            task = executor->tasks.front();
            executor->tasks.pop_front();
        }
        task();
        {
            std::unique_lock<std::mutex> lock(executor->mutex);
            executor->_occupied_threads -= 1;
        }
    }
    {
        std::unique_lock<std::mutex> lock(executor->mutex);
        executor->destroy_thread();
        if (executor->threads.empty()) {
            executor->_stop_condition.notify_all();
            executor->state = Executor::State::kStopped;
        }
    }
}

void Executor::Stop(bool await) {
    std::unique_lock<std::mutex> lock(mutex);
    if (threads.empty()) {
        state = State::kStopped;
    } else {
        state = State::kStopping;
        if (await) {
            while (!threads.empty()) {
                _stop_condition.wait(lock);
            }
        }
    }
}

Executor::~Executor() {
    std::unique_lock<std::mutex> lock(mutex);
    if (state != State::kStopped) {
        lock.unlock();
        Stop(true);
    }
}

void Executor::create_thread() {
    threads.emplace_back(perform, this);
}

void Executor::destroy_thread() {
    auto this_thread = std::this_thread::get_id();
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
