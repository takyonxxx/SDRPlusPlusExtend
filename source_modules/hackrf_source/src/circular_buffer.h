#ifndef CIRCULAR_BUFFER_H
#define CIRCULAR_BUFFER_H
#include <vector>
#include <mutex>
#include <queue>
#include <condition_variable>

class CircularBuffer {
public:
    CircularBuffer(size_t size) : buffer_(size), head_(0), tail_(0), full_(false) {}

    void write(const std::vector<uint8_t>& data) {
        std::unique_lock<std::mutex> lock(mutex_);
        for (auto byte : data) {
            buffer_[head_] = byte;
            head_ = (head_ + 1) % buffer_.size();
            if (full_) {
                tail_ = (tail_ + 1) % buffer_.size();
            }
            full_ = head_ == tail_;
        }
        data_available_.notify_all();
    }

    void read(std::vector<uint8_t>& data, size_t length) {
        std::unique_lock<std::mutex> lock(mutex_);
        data_available_.wait(lock, [&] {
            return size() >= length;
        });

        for (size_t i = 0; i < length; ++i) {
            data[i] = buffer_[(tail_ + i) % buffer_.size()];
        }

        tail_ = (tail_ + length) % buffer_.size();
        full_ = false;
    }

    size_t size() const {
        std::unique_lock<std::mutex> lock(mutex_);
        if (full_) {
            return buffer_.size();
        }
        if (head_ >= tail_) {
            return head_ - tail_;
        } else {
            return buffer_.size() - tail_ + head_;
        }
    }

    void addData(double data) {
        std::unique_lock<std::mutex> lock(mutex_);
        buffer_[tail_] = data;
        tail_ = (tail_ + 1) % buffer_.size();
        data_available_.notify_one();
    }

    size_t capacity() const {
        return buffer_.size();
    }

    mutable std::mutex mutex_;
    std::condition_variable data_available_;

public:
    std::vector<uint8_t> buffer_;
    size_t head_;
    size_t tail_;
    bool full_;
};
#endif // CIRCULAR_BUFFER_H
