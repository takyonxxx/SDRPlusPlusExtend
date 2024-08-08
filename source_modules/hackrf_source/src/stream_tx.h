#ifndef STREAM_TX_H
#define STREAM_TX_H
#pragma once
#include <vector>
#include <mutex>
#include <condition_variable>
#include <volk/volk.h>

namespace dsp::buffer {
    template<class T>
    inline T* alloc_tx(int count) {
        return (T*)volk_malloc(count * sizeof(T), volk_get_alignment());
    }

    template<class T>
    inline void clear_tx(T* buffer, int count, int offset = 0) {
        memset(&buffer[offset], 0, count * sizeof(T));
    }

    inline void free_tx(void* buffer) {
        volk_free(buffer);
    }
}

// 1MSample buffer
#define STREAM_BUFFER_SIZE 1000000

namespace dsp {
    class untyped_stream_tx {
    public:
        virtual ~untyped_stream_tx() {}
        virtual bool swap(int size) { return false; }
        virtual void flush() {}       
    };

    template <class T>
    class stream_tx : public untyped_stream_tx {
    public:
        stream_tx() {
            writeBuf = buffer::alloc_tx<T>(STREAM_BUFFER_SIZE);
            readBuf = buffer::alloc_tx<T>(STREAM_BUFFER_SIZE);
        }

        virtual ~stream_tx() {
            free();
        }

        virtual void setBufferSize(int samples) {
            buffer::free_tx(writeBuf);
            buffer::free_tx(readBuf);
            writeBuf = buffer::alloc_tx<T>(samples);
            readBuf = buffer::alloc_tx<T>(samples);
        }

        virtual inline bool swap(int size) {

            std::unique_lock<std::mutex> lck(rdyMtx);
            dataSize = size;
            T* temp = writeBuf;
            writeBuf = readBuf;
            readBuf = temp;
            canSwap = false;
            return true;
        }

        std::vector<float> readBufferToVector() {
            std::vector<float> result;
            std::unique_lock<std::mutex> lck(rdyMtx);

            if (dataSize <= 0 || readBuf == nullptr) {
                return result;
            }

            result.reserve(dataSize * 2);
            for (int i = 0; i < dataSize; ++i) {
                result.push_back(readBuf[i].re);
                result.push_back(readBuf[i].im);
            }
            return result;
        }

        void free() {
            if (writeBuf) { buffer::free_tx(writeBuf); }
            if (readBuf) { buffer::free_tx(readBuf); }
            writeBuf = nullptr;
            readBuf = nullptr;
        }

        T* writeBuf = nullptr;
        T* readBuf = nullptr;

    public:
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

#endif // STREAM_TX_H
