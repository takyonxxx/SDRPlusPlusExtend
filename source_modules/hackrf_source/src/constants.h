#ifndef CONSTANTS_H
#define CONSTANTS_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <iomanip>
#include <vector>
#include <mutex>
#include <cmath>
#include <filesystem>

#define DEBUG 1
#define BUF_LEN 262144   //hackrf tx buf
#define BYTES_PER_SAMPLE 2
#define CARRIER_FREQUENCY 100e6
#define DEVIATION 75e3
#define AUDIO_SAMPLE_RATE 44100

enum class TxSendType {
    MIC_ONLY,
    SINWAVE,
    WAV
};

#endif // CONSTANTS_H
