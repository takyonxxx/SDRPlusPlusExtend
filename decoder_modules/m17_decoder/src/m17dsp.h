#pragma once
#include <dsp/block.h>
#include <dsp/hier_block.h>
#include <dsp/sink/null_sink.h>
#include <dsp/demod/gfsk.h>
#include <dsp/routing/doubler.h>
#include <volk/volk.h>
#include <codec2.h>
#include <golay24.h>
#include <lsf_decode.h>

extern "C" {
#include <correct.h>
}

#define M17_DEVIATION     2400.0f
#define M17_BAUDRATE      4800.0f
#define M17_RRC_ALPHA     0.5f
#define M17_4FSK_HIGH_CUT ((1.0f + (1.0f/3.0f)) / 2.0f)

#define M17_SYNC_SIZE            16
#define M17_LICH_SIZE            96
#define M17_PAYLOAD_SIZE         144
#define M17_ENCODED_PAYLOAD_SIZE 296
#define M17_LSF_SIZE             240
#define M17_ENCODED_LSF_SIZE     488
#define M17_RAW_FRAME_SIZE       384
#define M17_CUT_FRAME_SIZE       368

#define M17_MAX_FN          0x7FFF
#define M17_END_FN          0x8000
#define M17_STREAM_TIMEOUT  500

const uint8_t M17_LSF_SYNC[16] = { 0, 1, 0, 1, 0, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1 };
const uint8_t M17_STF_SYNC[16] = { 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 1, 1, 0, 1 };
const uint8_t M17_PKF_SYNC[16] = { 0, 1, 1, 1, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1 };

const uint8_t M17_SCRAMBLER[368] = { 1, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1,
                                     1, 1, 1, 0, 0, 0, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0,
                                     1, 0, 0, 0, 0, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1,
                                     1, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 0,
                                     1, 0, 1, 1, 1, 0, 1, 0, 0, 1, 0, 0, 1, 1, 1, 0,
                                     1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0,
                                     1, 1, 0, 1, 1, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 0,
                                     1, 1, 0, 1, 1, 1, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1,
                                     0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 0, 0, 0,
                                     0, 1, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 1,
                                     1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1, 0, 1,
                                     1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 0, 1, 1, 1, 0,
                                     0, 1, 1, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1, 1, 1, 1,
                                     0, 0, 1, 1, 0, 1, 0, 1, 1, 1, 0, 1, 1, 0, 1, 0,
                                     0, 0, 0, 1, 0, 1, 0, 0, 1, 1, 1, 0, 1, 0, 1, 0,
                                     1, 1, 0, 0, 1, 1, 0, 1, 0, 1, 1, 1, 0, 1, 1, 0,
                                     0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 1,
                                     1, 1, 0, 1, 0, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0,
                                     1, 1, 0, 1, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 1,
                                     1, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 1, 0, 0, 1, 1,
                                     0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0,
                                     0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1,
                                     0, 1, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 1 };

const uint16_t M17_INTERLEAVER[368] = { 0, 137, 90, 227, 180, 317, 270, 39, 360, 129, 82, 219, 172, 309, 262, 31,
                                        352, 121, 74, 211, 164, 301, 254, 23, 344, 113, 66, 203, 156, 293, 246, 15,
                                        336, 105, 58, 195, 148, 285, 238, 7, 328, 97, 50, 187, 140, 277, 230, 367,
                                        320, 89, 42, 179, 132, 269, 222, 359, 312, 81, 34, 171, 124, 261, 214, 351,
                                        304, 73, 26, 163, 116, 253, 206, 343, 296, 65, 18, 155, 108, 245, 198, 335,
                                        288, 57, 10, 147, 100, 237, 190, 327, 280, 49, 2, 139, 92, 229, 182, 319,
                                        272, 41, 362, 131, 84, 221, 174, 311, 264, 33, 354, 123, 76, 213, 166, 303,
                                        256, 25, 346, 115, 68, 205, 158, 295, 248, 17, 338, 107, 60, 197, 150, 287,
                                        240, 9, 330, 99, 52, 189, 142, 279, 232, 1, 322, 91, 44, 181, 134, 271,
                                        224, 361, 314, 83, 36, 173, 126, 263, 216, 353, 306, 75, 28, 165, 118, 255,
                                        208, 345, 298, 67, 20, 157, 110, 247, 200, 337, 290, 59, 12, 149, 102, 239,
                                        192, 329, 282, 51, 4, 141, 94, 231, 184, 321, 274, 43, 364, 133, 86, 223,
                                        176, 313, 266, 35, 356, 125, 78, 215, 168, 305, 258, 27, 348, 117, 70, 207,
                                        160, 297, 250, 19, 340, 109, 62, 199, 152, 289, 242, 11, 332, 101, 54, 191,
                                        144, 281, 234, 3, 324, 93, 46, 183, 136, 273, 226, 363, 316, 85, 38, 175,
                                        128, 265, 218, 355, 308, 77, 30, 167, 120, 257, 210, 347, 300, 69, 22, 159,
                                        112, 249, 202, 339, 292, 61, 14, 151, 104, 241, 194, 331, 284, 53, 6, 143,
                                        96, 233, 186, 323, 276, 45, 366, 135, 88, 225, 178, 315, 268, 37, 358, 127,
                                        80, 217, 170, 307, 260, 29, 350, 119, 72, 209, 162, 299, 252, 21, 342, 111,
                                        64, 201, 154, 291, 244, 13, 334, 103, 56, 193, 146, 283, 236, 5, 326, 95,
                                        48, 185, 138, 275, 228, 365, 318, 87, 40, 177, 130, 267, 220, 357, 310, 79,
                                        32, 169, 122, 259, 212, 349, 302, 71, 24, 161, 114, 251, 204, 341, 294, 63,
                                        16, 153, 106, 243, 196, 333, 286, 55, 8, 145, 98, 235, 188, 325, 278, 47 };

const uint8_t M17_PUNCTURING_P1[61] = { 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1,
                                        1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1,
                                        1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1,
                                        1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1 };

const uint8_t M17_PUNCTURING_P2[12] = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0 };

static const correct_convolutional_polynomial_t correct_conv_m17_polynomial[] = { 0b11001, 0b10111 };

namespace dsp {
    class M17Slice4FSK : public block {
    public:
        M17Slice4FSK() {}

        M17Slice4FSK(stream<float>* in) { init(in); }

        void init(stream<float>* in) {
            _in = in;
            block::registerInput(_in);
            block::registerOutput(&out);
            block::_block_init = true;
        }

        void setInput(stream<float>* in) {
            assert(block::_block_init);
            std::lock_guard<std::recursive_mutex> lck(block::ctrlMtx);
            block::tempStop();
            block::unregisterInput(_in);
            _in = in;
            block::registerInput(_in);
            block::tempStart();
        }

        int run() {
            int count = _in->read();
            if (count < 0) { return -1; }

            float val;
            for (int i = 0; i < count; i++) {
                val = _in->readBuf[i];
                out.writeBuf[i * 2] = (val < 0.0f);
                out.writeBuf[(i * 2) + 1] = (fabsf(val) > M17_4FSK_HIGH_CUT);
            }

            _in->flush();

            if (!out.swap(count * 2)) { return -1; }
            return count;
        }

        stream<uint8_t> out;

    private:
        stream<float>* _in;
    };

    class M17FrameDemux : public block {
    public:
        M17FrameDemux() {}

        M17FrameDemux(stream<uint8_t>* in) { init(in); }

        ~M17FrameDemux() {
            if (!block::_block_init) { return; }
            block::stop();
            delete[] delay;
        }

        void init(stream<uint8_t>* in) {
            _in = in;

            delay = new uint8_t[STREAM_BUFFER_SIZE];

            block::registerInput(_in);
            block::registerOutput(&linkSetupOut);
            block::registerOutput(&lichOut);
            block::registerOutput(&streamOut);
            block::registerOutput(&packetOut);
            block::_block_init = true;
        }

        void setInput(stream<uint8_t>* in) {
            assert(block::_block_init);
            std::lock_guard<std::recursive_mutex> lck(block::ctrlMtx);
            block::tempStop();
            block::unregisterInput(_in);
            _in = in;
            block::registerInput(_in);
            block::tempStart();
        }

        int run() {
            int count = _in->read();
            if (count < 0) { return -1; }

            memcpy(&delay[M17_SYNC_SIZE], _in->readBuf, count);

            for (int i = 0; i < count;) {
                if (detect) {
                    if (outCount < M17_SYNC_SIZE) {
                        outCount++;
                        i++;
                    }
                    else {
                        int id = M17_INTERLEAVER[outCount - M17_SYNC_SIZE];

                        if (type == 0) {
                            linkSetupOut.writeBuf[id] = delay[i++] ^ M17_SCRAMBLER[outCount - M17_SYNC_SIZE];
                        }
                        else if ((type == 1 || type == 2) && id < M17_LICH_SIZE) {
                            lichOut.writeBuf[id] = delay[i++] ^ M17_SCRAMBLER[outCount - M17_SYNC_SIZE];
                        }
                        else if (type == 1) {
                            streamOut.writeBuf[id - M17_LICH_SIZE] = (delay[i++] ^ M17_SCRAMBLER[outCount - M17_SYNC_SIZE]);
                        }
                        else if (type == 2) {
                            packetOut.writeBuf[id - M17_LICH_SIZE] = (delay[i++] ^ M17_SCRAMBLER[outCount - M17_SYNC_SIZE]);
                        }

                        outCount++;
                    }

                    if (outCount >= M17_RAW_FRAME_SIZE) {
                        detect = false;
                        if (type == 0) {
                            if (!linkSetupOut.swap(M17_CUT_FRAME_SIZE)) { return -1; }
                        }
                        else if (type == 1) {
                            if (!lichOut.swap(M17_LICH_SIZE)) { return -1; }
                            if (!streamOut.swap(M17_CUT_FRAME_SIZE)) { return -1; }
                        }
                        else if (type == 2) {
                            if (!lichOut.swap(M17_LICH_SIZE)) { return -1; }
                            if (!packetOut.swap(M17_CUT_FRAME_SIZE)) { return -1; }
                        }
                    }
                    continue;
                }

                // Check for link setup syncword
                if (!memcmp(&delay[i], M17_LSF_SYNC, M17_SYNC_SIZE)) {
                    detect = true;
                    outCount = 0;
                    type = 0;
                    //flog::warn("Found sync frame");
                    continue;
                }

                // Check for stream syncword
                if (!memcmp(&delay[i], M17_STF_SYNC, M17_SYNC_SIZE)) {
                    detect = true;
                    outCount = 0;
                    type = 1;
                    //flog::warn("Found stream frame");
                    continue;
                }

                // Check for packet syncword
                if (!memcmp(&delay[i], M17_PKF_SYNC, M17_SYNC_SIZE)) {
                    detect = true;
                    outCount = 0;
                    type = 2;
                    //flog::warn("Found packet frame");
                    continue;
                }

                i++;
            }

            memmove(delay, &delay[count], 16);

            _in->flush();

            return count;
        }

        stream<uint8_t> linkSetupOut;
        stream<uint8_t> lichOut;
        stream<uint8_t> streamOut;
        stream<uint8_t> packetOut;

    private:
        stream<uint8_t>* _in;

        uint8_t* delay;

        bool detect = false;
        int type;

        int outCount = 0;
    };

    class M17LSFDecoder : public block {
    public:
        M17LSFDecoder() {}

        M17LSFDecoder(stream<uint8_t>* in, void (*handler)(M17LSF& lsf, void* ctx), void* ctx) { init(in, handler, ctx); }

        ~M17LSFDecoder() {
            if (!block::_block_init) { return; }
            block::stop();
            correct_convolutional_destroy(conv);
        }

        void init(stream<uint8_t>* in, void (*handler)(M17LSF& lsf, void* ctx), void* ctx) {
            _in = in;
            _handler = handler;
            _ctx = ctx;

            conv = correct_convolutional_create(2, 5, correct_conv_m17_polynomial);

            block::registerInput(_in);
            block::_block_init = true;
        }

        void setInput(stream<uint8_t>* in) {
            assert(block::_block_init);
            std::lock_guard<std::recursive_mutex> lck(block::ctrlMtx);
            block::tempStop();
            block::unregisterInput(_in);
            _in = in;
            block::registerInput(_in);
            block::tempStart();
        }

        int run() {
            int count = _in->read();
            if (count < 0) { return -1; }

            // Depuncture the data
            int inOffset = 0;
            for (int i = 0; i < M17_ENCODED_LSF_SIZE; i++) {
                if (!M17_PUNCTURING_P1[i % 61]) {
                    depunctured[i] = 0;
                    continue;
                }
                depunctured[i] = _in->readBuf[inOffset++];
            }

            _in->flush();

            // Pack into bytes
            memset(packed, 0, 61);
            for (int i = 0; i < M17_ENCODED_LSF_SIZE; i++) {
                packed[i / 8] |= depunctured[i] << (7 - (i % 8));
            }

            // Run through convolutional decoder
            correct_convolutional_decode(conv, packed, M17_ENCODED_LSF_SIZE, lsf);

            // Decode it and call the handler
            M17LSF decLsf = M17DecodeLSF(lsf);
            if (decLsf.valid) { _handler(decLsf, _ctx); }

            return count;
        }

    private:
        stream<uint8_t>* _in;

        void (*_handler)(M17LSF& lsf, void* ctx);
        void* _ctx;

        uint8_t depunctured[488];
        uint8_t packed[61];
        uint8_t lsf[30];

        correct_convolutional* conv;
    };

    class M17PayloadFEC : public block {
    public:
        M17PayloadFEC() {}

        M17PayloadFEC(stream<uint8_t>* in) { init(in); }

        ~M17PayloadFEC() {
            if (!block::_block_init) { return; }
            block::stop();
            correct_convolutional_destroy(conv);
        }

        void init(stream<uint8_t>* in) {
            _in = in;

            conv = correct_convolutional_create(2, 5, correct_conv_m17_polynomial);

            block::registerInput(_in);
            block::registerOutput(&out);
            block::_block_init = true;
        }

        void setInput(stream<uint8_t>* in) {
            assert(block::_block_init);
            std::lock_guard<std::recursive_mutex> lck(block::ctrlMtx);
            block::tempStop();
            block::unregisterInput(_in);
            _in = in;
            block::registerInput(_in);
            block::tempStart();
        }

        int run() {
            int count = _in->read();
            if (count < 0) { return -1; }

            // Depuncture the data
            int inOffset = 0;
            for (int i = 0; i < M17_ENCODED_PAYLOAD_SIZE; i++) {
                if (!M17_PUNCTURING_P2[i % 12]) {
                    depunctured[i] = 0;
                    continue;
                }
                depunctured[i] = _in->readBuf[inOffset++];
            }

            // Pack into bytes
            memset(packed, 0, 37);
            for (int i = 0; i < M17_ENCODED_PAYLOAD_SIZE; i++) {
                if (!(i % 8)) { packed[i / 8] = 0; }
                packed[i / 8] |= depunctured[i] << (7 - (i % 8));
            }

            // Run through convolutional decoder
            correct_convolutional_decode(conv, packed, M17_ENCODED_PAYLOAD_SIZE, out.writeBuf);

            _in->flush();

            if (!out.swap(M17_PAYLOAD_SIZE / 8)) { return -1; }
            return count;
        }

        stream<uint8_t> out;

    private:
        stream<uint8_t>* _in;

        uint8_t depunctured[296];
        uint8_t packed[37];

        correct_convolutional* conv;
    };

    class M17Codec2Decode : public block {
    public:
        M17Codec2Decode() {}

        M17Codec2Decode(stream<uint8_t>* in) { init(in); }

        ~M17Codec2Decode() {
            if (!block::_block_init) { return; }
            block::stop();
            codec2_destroy(codec);
            delete[] int16Audio;
            delete[] floatAudio;
        }

        void init(stream<uint8_t>* in) {
            _in = in;
            lastConseqTime = std::chrono::high_resolution_clock::now();

            codec = codec2_create(CODEC2_MODE_3200);
            sampsPerC2Frame = codec2_samples_per_frame(codec);
            sampsPerC2FrameDouble = sampsPerC2Frame * 2;
            int16Audio = new int16_t[sampsPerC2FrameDouble];
            floatAudio = new float[sampsPerC2FrameDouble];

            block::registerInput(_in);
            block::registerOutput(&out);
            block::_block_init = true;
        }

        void setInput(stream<uint8_t>* in) {
            assert(block::_block_init);
            std::lock_guard<std::recursive_mutex> lck(block::ctrlMtx);
            block::tempStop();
            block::unregisterInput(_in);
            _in = in;
            block::registerInput(_in);
            block::tempStart();
        }

        bool isReceiving() {
            std::lock_guard<std::recursive_mutex> lck(recvMtx);
            return receiving && !timedOut();
        }

        bool timedOut() {
            std::lock_guard<std::recursive_mutex> lck(recvMtx);
            auto now = std::chrono::high_resolution_clock::now();
            return std::chrono::duration_cast<std::chrono::milliseconds>(now - lastConseqTime).count() > M17_STREAM_TIMEOUT;
        }

        int run() {
            int count = _in->read();
            if (count < 0) { return -1; }

            // Decode frame number
            uint16_t fn = ((uint16_t)_in->readBuf[0] << 8) | _in->readBuf[1];

            // Check if we need to start or stop receiving
            bool consecutive = ((((int)fn - (int)lastFn + M17_END_FN) % M17_END_FN) == 1);
            if (!receiving && consecutive) {
                std::lock_guard<std::recursive_mutex> lck(recvMtx);
                receiving = true;
            }
            else if (receiving && consecutive) {
                std::lock_guard<std::recursive_mutex> lck(recvMtx);
                lastConseqTime = std::chrono::high_resolution_clock::now();;
            }
            else if (receiving && !consecutive && timedOut()) {
                std::lock_guard<std::recursive_mutex> lck(recvMtx);
                receiving = false;
            }

            // Save FN and if we have to stop receiving and it's not the last frame, stop
            lastFn = fn;
            if (!receiving) {
                _in->flush();
                return count;
            }

            // Decode both parts using codec
            codec2_decode(codec, int16Audio, &_in->readBuf[2]);
            codec2_decode(codec, &int16Audio[sampsPerC2Frame], &_in->readBuf[2 + 8]);

            // Convert to float
            volk_16i_s32f_convert_32f(floatAudio, int16Audio, 32768.0f, sampsPerC2FrameDouble);

            // Interleave into stereo samples
            volk_32f_x2_interleave_32fc((lv_32fc_t*)out.writeBuf, floatAudio, floatAudio, sampsPerC2FrameDouble);

            _in->flush();

            if (!out.swap(sampsPerC2FrameDouble)) { return -1; }
            return count;
        }

        stream<stereo_t> out;

    private:
        stream<uint8_t>* _in;

        std::recursive_mutex recvMtx;
        bool receiving = false;
        uint16_t lastFn = 0;
        std::chrono::high_resolution_clock::time_point lastConseqTime;

        int16_t* int16Audio;
        float* floatAudio;

        CODEC2* codec;
        int sampsPerC2Frame = 0;
        int sampsPerC2FrameDouble = 0;
    };

    class M17LICHDecoder : public block {
    public:
        M17LICHDecoder() {}

        M17LICHDecoder(stream<uint8_t>* in, void (*handler)(M17LSF& lsf, void* ctx), void* ctx) { init(in, handler, ctx); }

        void init(stream<uint8_t>* in, void (*handler)(M17LSF& lsf, void* ctx), void* ctx) {
            _in = in;
            _handler = handler;
            _ctx = ctx;
            block::registerInput(_in);
            block::_block_init = true;
        }

        void setInput(stream<uint8_t>* in) {
            assert(block::_block_init);
            std::lock_guard<std::recursive_mutex> lck(block::ctrlMtx);
            block::tempStop();
            block::unregisterInput(_in);
            _in = in;
            block::registerInput(_in);
            block::tempStart();
        }

        int run() {
            int count = _in->read();
            if (count < 0) { return -1; }

            // Zero out block
            memset(chunk, 0, 6);

            // Decode the 4 Golay(24, 12) blocks
            uint32_t encodedBlock;
            uint32_t decodedBlock;
            for (int b = 0; b < 4; b++) {
                // Pack the 24bit block into a byte
                encodedBlock = 0;
                decodedBlock = 0;
                for (int i = 0; i < 24; i++) { encodedBlock |= _in->readBuf[(b * 24) + i] << (23 - i); }

                // Decode
                if (!mobilinkd::Golay24::decode(encodedBlock, decodedBlock)) {
                    _in->flush();
                    return count;
                }

                // Pack the decoded bits into the output
                int id = 0;
                uint8_t temp;
                for (int i = 0; i < 12; i++) {
                    id = (b * 12) + i;
                    chunk[id / 8] |= ((decodedBlock >> (23 - i)) & 1) << (7 - (id % 8));
                }
            }

            _in->flush();

            int partId = chunk[5] >> 5;

            // If the ID of the chunk is zero, start a new LSF
            if (partId == 0) {
                newFrame = true;
                lastId = 0;
                memcpy(&lsf[partId * 5], chunk, 5);
                return count;
            }

            // If we're recording a LSF and a discontinuity shows up, cancel
            if (newFrame && partId != lastId + 1) {
                newFrame = false;
                return count;
            }

            // If we're recording and there's no discontinuity (see above), add the data to the full frame
            if (newFrame) {
                lastId = partId;
                memcpy(&lsf[partId * 5], chunk, 5);

                // If the lsf is complete, send it out
                if (partId == 5) {
                    newFrame = false;
                    M17LSF decLsf = M17DecodeLSF(lsf);
                    if (decLsf.valid) { _handler(decLsf, _ctx); }
                }
            }

            return count;
        }

    private:
        stream<uint8_t>* _in;
        void (*_handler)(M17LSF& lsf, void* ctx);
        void* _ctx;

        uint8_t chunk[6];
        uint8_t lsf[240];
        bool newFrame = false;
        int lastId = 0;
    };

    class M17Decoder : public hier_block {
    public:
        M17Decoder() {}

        M17Decoder(stream<complex_t>* input, float sampleRate, void (*handler)(M17LSF& lsf, void* ctx), void* ctx) {
            init(input, sampleRate, handler, ctx);
        }

        void init(stream<complex_t>* input, float sampleRate, void (*handler)(M17LSF& lsf, void* ctx), void* ctx) {
            _sampleRate = sampleRate;

            demod.init(input, M17_BAUDRATE, sampleRate, M17_DEVIATION, 31, M17_RRC_ALPHA, 1e-6f, 0.01f, 0.01f);
            doubler.init(&demod.out);
            slice.init(&doubler.outA);
            demux.init(&slice.out);
            lsfFEC.init(&demux.linkSetupOut, handler, ctx);
            payloadFEC.init(&demux.streamOut);
            decodeLICH.init(&demux.lichOut, handler, ctx);
            decodeAudio.init(&payloadFEC.out);

            ns2.init(&demux.packetOut);

            diagOut = &doubler.outB;
            out = &decodeAudio.out;

            hier_block::registerBlock(&demod);
            hier_block::registerBlock(&doubler);
            hier_block::registerBlock(&slice);
            hier_block::registerBlock(&demux);
            hier_block::registerBlock(&lsfFEC);
            hier_block::registerBlock(&payloadFEC);
            hier_block::registerBlock(&decodeLICH);
            hier_block::registerBlock(&decodeAudio);

            hier_block::registerBlock(&ns2);

            hier_block::_block_init = true;
        }

        void setInput(stream<complex_t>* input) {
            assert(hier_block::_block_init);
            demod.setInput(input);
        }

        bool isReceiving() {
            return decodeAudio.isReceiving();
        }

        stream<float>* diagOut = NULL;
        stream<stereo_t>* out = NULL;

    private:
        demod::GFSK demod;
        routing::Doubler<float> doubler;
        M17Slice4FSK slice;
        M17FrameDemux demux;
        M17LSFDecoder lsfFEC;
        M17PayloadFEC payloadFEC;
        M17LICHDecoder decodeLICH;
        M17Codec2Decode decodeAudio;

        dsp::sink::Null<uint8_t> ns2;


        float _sampleRate;
    };
}