#pragma once
#include <type_traits>
#include <string.h>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <volk/volk.h>
#include "buffer/buffer.h"

// 1MSample buffer
#define STREAM_BUFFER_SIZE 1000000

namespace dsp {
    class untyped_stream {
    public:
        virtual ~untyped_stream() {}
        virtual bool swap(int size) { return false; }
        virtual int read() { return -1; }
        virtual void flush() {}
        virtual void stopWriter() {}
        virtual void clearWriteStop() {}
        virtual void stopReader() {}
        virtual void clearReadStop() {}
    };

    template <class T>
    class stream : public untyped_stream {
    public:
        stream() {
            writeBuf = buffer::alloc<T>(STREAM_BUFFER_SIZE);
            readBuf = buffer::alloc<T>(STREAM_BUFFER_SIZE);
        }

        virtual ~stream() {
            free();
        }        

        virtual void setBufferSize(int samples) {
            buffer::free(writeBuf);
            buffer::free(readBuf);
            writeBuf = buffer::alloc<T>(samples);
            readBuf = buffer::alloc<T>(samples);
        }

        virtual inline bool swap(int size) {
            {
                // Wait to either swap or stop
                std::unique_lock<std::mutex> lck(swapMtx);
                swapCV.wait(lck, [this] { return (canSwap || writerStop); });

                // If writer was stopped, abandon operation
                if (writerStop) { return false; }

                // Swap buffers
                dataSize = size;
                T* temp = writeBuf;
                writeBuf = readBuf;
                readBuf = temp;
                canSwap = false;
            }

            // Notify reader that some data is ready
            {
                std::lock_guard<std::mutex> lck(rdyMtx);
                dataReady = true;
            }
            rdyCV.notify_all();

            return true;
        }

        virtual inline int read() {
            // Wait for data to be ready or to be stopped
            std::unique_lock<std::mutex> lck(rdyMtx);
            rdyCV.wait(lck, [this] { return (dataReady || readerStop); });

            return (readerStop ? -1 : dataSize);
        }

        template <typename U>
        float convertToFloat(const U& value) {
            if constexpr (std::is_same_v<U, float>) {
                return value;
            } else if constexpr (std::is_arithmetic_v<U>) {
                return static_cast<float>(value);
            } else if constexpr (std::is_same_v<U, dsp::complex_t>) {
                // Assuming dsp::complex_t has real and imag parts accessible
                return std::sqrt(value.re * value.re + value.im * value.im);
            } else {
                // For any other types, you might need to add more specific handling
                return 0.0f;  // Default case, adjust as needed
            }
        }

        // Your readBufferToVector method remains the same
        std::vector<float> readBufferToVector() {
            std::vector<float> result;
            std::unique_lock<std::mutex> lck(rdyMtx);
            rdyCV.wait(lck, [this] { return (dataReady || readerStop); });

            if (readerStop) {
                return result;
            }

            result.resize(dataSize);

                   // Convert each element to float
            for (int i = 0; i < dataSize; ++i) {
                result[i] = convertToFloat(readBuf[i]);
            }

            dataReady = false;
            {
                std::lock_guard<std::mutex> lck(swapMtx);
                canSwap = true;
            }
            swapCV.notify_all();
            return result;
        }

        virtual inline void flush() {
            // Clear data ready
            {
                std::lock_guard<std::mutex> lck(rdyMtx);
                dataReady = false;
            }

            // Notify writer that buffers can be swapped
            {
                std::lock_guard<std::mutex> lck(swapMtx);
                canSwap = true;
            }

            swapCV.notify_all();
        }

        virtual void stopWriter() {
            {
                std::lock_guard<std::mutex> lck(swapMtx);
                writerStop = true;
            }
            swapCV.notify_all();
        }

        virtual void clearWriteStop() {
            writerStop = false;
        }

        virtual void stopReader() {
            {
                std::lock_guard<std::mutex> lck(rdyMtx);
                readerStop = true;
            }
            rdyCV.notify_all();
        }

        virtual void clearReadStop() {
            readerStop = false;
        }

        void free() {
            if (writeBuf) { buffer::free(writeBuf); }
            if (readBuf) { buffer::free(readBuf); }
            writeBuf = NULL;
            readBuf = NULL;
        }

        T* writeBuf;
        T* readBuf;

    private:
        std::mutex swapMtx;
        std::condition_variable swapCV;
        bool canSwap = true;

        std::mutex rdyMtx;
        std::condition_variable rdyCV;
        bool dataReady = false;

        bool readerStop = false;
        bool writerStop = false;

        int dataSize = 0;
    };
}
