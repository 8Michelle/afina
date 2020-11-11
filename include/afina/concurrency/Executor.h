#ifndef AFINA_CONCURRENCY_EXECUTOR_H
#define AFINA_CONCURRENCY_EXECUTOR_H

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <algorithm>
#include <chrono>

namespace Afina {
namespace Concurrency {

/**
 * # Thread pool
 */
class Executor {
    enum class State {
        // Threadpool is fully operational, tasks could be added and get executed
        kRun,

        // Threadpool is on the way to be shutdown, no ned task could be added, but existing will be
        // completed as requested
        kStopping,

        // Threadppol is stopped
        kStopped
    };

public:
    Executor(size_t max_queue_size = 4,
             size_t low_watermark = 4,
             size_t high_watermark = 16,
             std::chrono::milliseconds idle_time = std::chrono::milliseconds(10000)) :
    max_queue_size_(max_queue_size),
    state_(State::kStopped),
    idle_time_(idle_time),
    low_watermark_(low_watermark),
    high_watermark_(high_watermark),
    threads_(0),
    free_threads_(0) {}

    ~Executor() {
        Stop(true);
    }

    void Start() {
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

    /**
     * Signal thread pool to stop, it will stop accepting new jobs and close threads just after each become
     * free. All enqueued jobs will be complete.
     *
     * In case if await flag is true, call won't return until all background jobs are done and all threads are stopped
     */
    void Stop(bool await = false) {
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

    /**
     * Add function to be executed on the threadpool. Method returns true in case if task has been placed
     * onto execution queue, i.e scheduled for execution and false otherwise.
     *
     * That function doesn't wait for function result. Function could always be written in a way to notify caller about
     * execution finished by itself
     */
    template <typename F, typename... Types> bool Execute(F &&func, Types... args) {
        // Prepare "task"
        auto exec = std::bind(std::forward<F>(func), std::forward<Types>(args)...);

        std::unique_lock<std::mutex> lock(this->mutex_);
        if (state_ != State::kRun || tasks_.size() == max_queue_size_) {
            return false;
        }

        // Enqueue new task
        tasks_.push_back(exec);

        if (free_threads_ == 0 && threads_ < high_watermark_) {
            ++threads_;
            ++free_threads_;
            std::thread new_thread(&Executor::perform, this);
            new_thread.detach();
            return true;
        }

        if (tasks_.size() - 1 == 0) {
            empty_condition_.notify_all();
        }

        return true;
    }

private:
    // No copy/move/assign allowed
    Executor(const Executor &);            // = delete;
    Executor(Executor &&);                 // = delete;
    Executor &operator=(const Executor &); // = delete;
    Executor &operator=(Executor &&);      // = delete;

    /**
     * Main function that all pool threads are running. It polls internal task queue and execute tasks
     */
    void perform() {
        std::unique_lock<std::mutex> lock(mutex_);
        std::function<void()> task;
        auto time_stamp = std::chrono::system_clock::now();
        while (true) {
            if (tasks_.empty()) {
                if (state_ == Executor::State::kStopping ||
                    (empty_condition_.wait_until(lock, time_stamp + idle_time_) == std::cv_status::timeout &&
                     threads_ > low_watermark_)) {
                    break;

                } else {
                    continue;
                }

            } else {
                task = tasks_.front();
                tasks_.pop_front();
                --free_threads_;
                lock.unlock();

                task();

                lock.lock();
                ++free_threads_;
                time_stamp = std::chrono::system_clock::now();
            }
        }

        if (--threads_ == 0 && state_ == Executor::State::kStopping) {
            state_ = Executor::State::kStopped;
            stop_condition_.notify_all();
        }
        --free_threads_;
    }

    /**
     * Mutex to protect state below from concurrent modification
     */
    std::mutex mutex_;

    /**
     * Conditional variable to await new data in case of empty queue
     */
    std::condition_variable empty_condition_;

    /**
     * Conditional variable to await in case of full queue.
     */

    std::condition_variable stop_condition_;

    /**
     * Vector of actual threads that perorm execution
     */

    /**
     * Task queue
     */
    std::deque<std::function<void()>> tasks_;

    /**
     * Flag to stop bg threads
     */
    State state_;

    size_t threads_;
    size_t free_threads_;

    size_t max_queue_size_;

    std::chrono::milliseconds idle_time_;

    size_t low_watermark_;
    size_t high_watermark_;
};

} // namespace Concurrency
} // namespace Afina

#endif // AFINA_CONCURRENCY_EXECUTOR_H
