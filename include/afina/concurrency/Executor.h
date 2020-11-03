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
    Executor(size_t max_queue_size = 4,
             size_t low_watermark = 4,
             size_t high_watermark = 16,
             std::chrono::milliseconds idle_time = std::chrono::milliseconds(10000)) :
    max_queue_size_(max_queue_size),
    state_(State::kStopped),
    idle_time_(idle_time),
    low_watermark_(low_watermark),
    high_watermark_(high_watermark),
    queue_size_(0),
    threads_(0),
    free_threads_(0) {}

    ~Executor() {
        Stop(true);
    }

    void Start();

    /**
     * Signal thread pool to stop, it will stop accepting new jobs and close threads just after each become
     * free. All enqueued jobs will be complete.
     *
     * In case if await flag is true, call won't return until all background jobs are done and all threads are stopped
     */
    void Stop(bool await = false);

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

        if (free_threads_ > 0 && queue_size_ - 1 == 0) {
            empty_condition_.notify_one();

        } else if (threads_ < high_watermark_) {
            ++threads_;
            ++free_threads_;
            std::thread new_thread(&Executor::perform, this);
            new_thread.detach();
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
    void perform();

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
    size_t queue_size_;

    std::chrono::milliseconds idle_time_;

    size_t low_watermark_;
    size_t high_watermark_;
};

} // namespace Concurrency
} // namespace Afina

#endif // AFINA_CONCURRENCY_EXECUTOR_H
