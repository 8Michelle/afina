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
    Executor(std::string name,
             size_t size = 100,
             size_t low_watermark = 4,
             size_t high_watermark = 16,
             size_t timeout = 5000) :
    name_(std::move(name)),
    max_queue_size_(size),
    state_(State::kRun),
    idle_time_(timeout),
    low_watermark_(low_watermark),
    high_watermark_(high_watermark) {

        std::unique_lock<std::mutex> lock(mutex_);
        queue_size_ = 0;
        for (size_t i = 0; i < low_watermark_; ++i) {
//            std::thread new_thread([this](){ perform(this); });
            threads_.emplace_back(std::thread([this](){ perform(this); }));
//            new_thread.detach();
        }
    }

    ~Executor() {
        Stop(true);
    }

    /**
     * Signal thread pool to stop, it will stop accepting new jobs and close threads just after each become
     * free. All enqueued jobs will be complete.
     *
     * In case if await flag is true, call won't return until all background jobs are done and all threads are stopped
     */
    void Stop(bool await = false) {
        std::unique_lock<std::mutex> lock(mutex_);
        state_ = State::kStopping;
        empty_condition_.notify_all();
        stop_condition_.wait(lock, [this]() {return tasks_.empty(); });
        state_ = State::kStopped;
        if (await) {
            stop_condition_.wait(lock, [this]() {return threads_.empty(); });
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
        if (state_ != State::kRun || queue_size_ == max_queue_size_) {
            return false;
        }

        // Enqueue new task
        tasks_.push_back(exec);
        ++queue_size_;
        empty_condition_.notify_all();
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
    friend void perform(Executor *executor);

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
    std::vector<std::thread> threads_;

    /**
     * Task queue
     */
    std::deque<std::function<void()>> tasks_;

    /**
     * Flag to stop bg threads
     */
    State state_;

    std::string name_;

    size_t max_queue_size_;
    size_t queue_size_;

    std::chrono::milliseconds idle_time_;

    size_t low_watermark_;
    size_t high_watermark_;
};

} // namespace Concurrency
} // namespace Afina

#endif // AFINA_CONCURRENCY_EXECUTOR_H
