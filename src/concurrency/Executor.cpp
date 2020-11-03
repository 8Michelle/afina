#include <afina/concurrency/Executor.h>

namespace Afina {
namespace Concurrency {


void Executor::Start() {
    std::unique_lock<std::mutex> lock(mutex_);

    if (state_ != State::kStopped) {
        return;
    }

    for (size_t i = 0; i < low_watermark_; ++i) {
        std::thread new_thread(&Executor::perform, this);
        new_thread.detach();
    }

    threads_ = low_watermark_;
    free_threads_ = low_watermark_;
    state_ = State::kRun;
}


void Executor::Stop(bool await) {
    std::unique_lock<std::mutex> lock(mutex_);

    if (state_ == State::kStopped) {
        return;
    }

    state_ = State::kStopping;
    if (threads_ > 0) {
        empty_condition_.notify_all();

        if (await) {
            while (state_ != State::kStopped) {
                stop_condition_.wait(lock);
            }
        }

    } else {
        state_ = State::kStopped;
    }
}

void Executor::perform() {
    std::unique_lock<std::mutex> lock(mutex_);

    while (true) {
        if (tasks_.empty()) {
            if (state_ == Executor::State::kStopping ||
                (empty_condition_.wait_for(lock, idle_time_) == std::cv_status::timeout &&
                threads_ > low_watermark_)) {
                break;

            } else {
                continue;
            }

        } else {
            std::function<void()> task = tasks_.front();
            tasks_.pop_front();
            --free_threads_;
            lock.unlock();

            task();

            lock.lock();
            ++free_threads_;
        }
    }

    if (--threads_ == 0 && state_ == Executor::State::kStopping) {
        state_ = Executor::State::kStopped;
        stop_condition_.notify_one();
    }
    --free_threads_;
}


} // namespace Concurrency
} // namespace Afina
