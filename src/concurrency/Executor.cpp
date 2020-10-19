#include <afina/concurrency/Executor.h>

namespace Afina {
namespace Concurrency {

void perform(Executor *executor) {
    std::function<void()> task;
    while (true) {

        {
            std::unique_lock<std::mutex> lock(executor->mutex_);
            bool stopping = (executor->state_ == Executor::State::kStopping);
            bool stopped = (executor->state_ == Executor::State::kStopped);
            if (executor->empty_condition_.wait_for(lock,
                                                   executor->idle_time_,
                                                   [executor]() { return !executor->tasks_.empty(); }) && !stopped) {

                task = executor->tasks_.front();
                executor->tasks_.pop_front();

                if (stopping && executor->tasks_.empty()) {
                    executor->stop_condition_.notify_one();
                }

            } else if (executor->threads_.size() > executor->low_watermark_ || stopped) {

                auto self = std::this_thread::get_id();
                auto thread = std::find_if(executor->threads_.begin(), executor->threads_.end(),
                                        [self](std::thread& thread) { return thread.get_id() == self; }); // try std::move
                executor->threads_.erase(thread);

                if (executor->threads_.empty()) {
                    executor->stop_condition_.notify_one();
                }

                break;

            }
        }

        task();
    }
}

//void Executor::Stop(bool await) {
//    std::unique_lock<std::mutex> lock(mutex_);
//    state_ = State::kStopping;
//    empty_condition_.notify_all();
//    stop_condition_.wait(lock, [this]() {return tasks_.empty(); });
//    state_ = State::kStopped;
//    if (await) {
//        stop_condition_.wait(lock, [this]() {return threads_.empty(); });
//    }
//}

} // namespace Concurrency
} // namespace Afina
