#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <iomanip>
#include <vector>
#include <mutex>

#define DEBUG 1
#define BUF_LEN 262144         //hackrf tx buf
#define BUF_NUM  63
#define BYTES_PER_SAMPLE  2

typedef struct _WaveHeader_t
{
    char chunkID[4];  			// 1-4		"RIFF"
    int32_t chunkSize; 			// 5-8
    char format[4];				// 9-12		"WAVE"
    char subchunkID[4];			// 13-16	"fmt\0"
    int32_t subchunkSize;		// 17-20
    uint16_t audioFormat;		// 21-22 	PCM = 1
    uint16_t numChannels;		// 23-24
    int32_t sampleRate;			// 25-28
    int32_t bytesPerSecond;		// 29-32
    uint16_t blockAlign;		// 33-34
    uint16_t bitDepth;			// 35-36	16bit support only
    char dataID[4];				// 37-40	"data"
    int32_t dataSize;			// 41-44
} WaveHeader;

typedef struct _WaveData_t
{
    _WaveHeader_t header;
    int16_t *samples;
    int32_t size;
    int32_t sampleRate;
    uint16_t bitDepth;
} WaveData;

/*
 * Prototypes
 */
void printHeaderInfo(WaveHeader);
WaveData* wavRead(char[],size_t);
void dumpDataToFile(WaveData);



WaveData* wavRead(const char fileName[],size_t fileNameSize)
{
    //
    if (fileName[fileNameSize] != '\0')
    {
        fprintf(stderr,"wavRead: Invalid string format.\n");
    } else
    {
        FILE* filePtr = fopen(fileName, "r");
        if (filePtr == NULL)
        {
            perror("Unable to open file");
        }
        else
        {
            // Read header.
            WaveHeader header;
            fread(&header, sizeof(header), 1, filePtr);

            if (DEBUG)
                printHeaderInfo(header);

            // Check if the file is of supported format.
            if (	strncmp(header.chunkID, 	"RIFF", 4)	||
                strncmp(header.format, 		"WAVE", 4)	||
                strncmp(header.subchunkID, "fmt" , 3)	||
                strncmp(header.dataID, 		"data", 4) 	||
                header.audioFormat != 1 				||
                header.bitDepth != 16)
            {
                fprintf(stderr, "Unsupported file type.\n");
            }
            else
            {
                // Initialize the data struct.
                WaveData *data = (WaveData*) malloc(sizeof(WaveData));
                data->header = header;
                data->sampleRate = header.sampleRate;
                data->bitDepth	= header.bitDepth;
                data->size		= header.dataSize;

                // Read data.
                // ToDo: Add support for 24-32bit files.
                // 24bit samples are best converted to 32bits
                data->samples = (int16_t*) malloc(header.dataSize * sizeof(int16_t));
                fread(data->samples, sizeof(float), header.dataSize, filePtr);
                fclose(filePtr);
                return data;
            }
        }
    }
    return NULL;
}


void dumpDataToFile(WaveData waveData)
{
    // Dump data into a text file.
    FILE* outputFilePtr = fopen("output.txt","w");
    if (outputFilePtr == NULL)
    {
        perror("");
    }
    else
    {
        int i;
        for (i = 0; i < waveData.size; ++i)
        {
            fprintf(outputFilePtr,"%d\n",waveData.samples[i]);
        }
        fclose(outputFilePtr);
    }
}

/*
 * Prints the wave header
 */
void printHeaderInfo(WaveHeader hdr)
{
    char buf[5];
    printf("Header Info:\n");
    strncpy(buf, hdr.chunkID, 4);
    buf[4] = '\0';
    printf("	Chunk ID: %s\n",buf);
    printf("	Chunk Size: %d\n", hdr.chunkSize);
    strncpy(buf,hdr.format,4);
    buf[4] = '\0';
    printf("	Format: %s\n", buf);
    strncpy(buf,hdr.subchunkID,4);
    buf[4] = '\0';
    printf("	Sub-chunk ID: %s\n", buf);
    printf("	Sub-chunk Size: %d\n", hdr.subchunkSize);
    printf("	Audio Format: %d\n", hdr.audioFormat);
    printf("	Channel Count: %d\n", hdr.numChannels);
    printf("	Sample Rate: %d\n", hdr.sampleRate);
    printf("	Bytes per Second: %d\n", hdr.bytesPerSecond);
    printf("	Block alignment: %d\n", hdr.blockAlign);
    printf("	Bit depth: %d\n", hdr.bitDepth);
    strncpy(buf,hdr.dataID, 4);
    buf[4] = '\0';
    printf("	Data ID: %s\n", buf);
    printf("	Data Size: %d\n", hdr.dataSize);
}

float _last_in_samples[4] = { 0.0, 0.0, 0.0, 0.0 };

void interpolation(float * in_buf, uint32_t in_samples, float * out_buf, uint32_t out_samples) {

    uint32_t i;		/* Input buffer index + 1. */
    uint32_t j = 0;	/* Output buffer index. */
    float pos;		/* Position relative to the input buffer
                        * + 1.0. */

    /* We always "stay one sample behind", so what would be our first sample
    * should be the last one wrote by the previous call. */
    pos = (float)in_samples / (float)out_samples;
    while (pos < 1.0)
    {
        out_buf[j] = _last_in_samples[3] + (in_buf[0] - _last_in_samples[3]) * pos;
        j++;
        pos = (float)(j + 1)* (float)in_samples / (float)out_samples;
    }

    /* Interpolation cycle. */
    i = (uint32_t)pos;
    while (j < (out_samples - 1))
    {

        out_buf[j] = in_buf[i - 1] + (in_buf[i] - in_buf[i - 1]) * (pos - (float)i);
        j++;
        pos = (float)(j + 1)* (float)in_samples / (float)out_samples;
        i = (uint32_t)pos;
    }

    /* The last sample is always the same in input and output buffers. */
    out_buf[j] = in_buf[in_samples - 1];

    /* Copy last samples to _last_in_samples (reusing i and j). */
    for (i = in_samples - 4, j = 0; j < 4; i++, j++)
        _last_in_samples[j] = in_buf[i];
}


void modulation(float * input, int8_t * output, uint32_t mode) {
    double fm_deviation = 0.0;
    float gain = 0.9;

    double fm_phase = 0.0;

    int hackrf_sample = 2000000;

    if (mode == 0) {
        fm_deviation = 2.0 * M_PI * 75.0e3 / hackrf_sample; // 75 kHz max deviation WBFM
    }
    else if (mode == 1)
    {
        fm_deviation = 2.0 * M_PI * 5.0e3 / hackrf_sample; // 5 kHz max deviation NBFM
    }

    //AM mode
    if (mode == 2) {
        for (uint32_t i = 0; i < BUF_LEN; i++) {
            double	audio_amp = input[i] * gain;

            if (fabs(audio_amp) > 1.0) {
                audio_amp = (audio_amp > 0.0) ? 1.0 : -1.0;
            }

            output[i * BYTES_PER_SAMPLE] = (int8_t)(audio_amp*127.0);
            output[i * BYTES_PER_SAMPLE + 1] = 0;
        }
    }
    //FM mode
    else {

        for (uint32_t i = 0; i < BUF_LEN / 2; i++) {

            double	audio_amp = input[i] * gain;

            if (fabs(audio_amp) > 1.0) {
                audio_amp = (audio_amp > 0.0) ? 1.0 : -1.0;
            }
            fm_phase += fm_deviation * audio_amp;
            while (fm_phase > (float)(M_PI))
                fm_phase -= (float)(2.0 * M_PI);
            while (fm_phase < (float)(-M_PI))
                fm_phase += (float)(2.0 * M_PI);

            output[i * BYTES_PER_SAMPLE] = (int8_t)(sin(fm_phase)*127.0);
            output[i * BYTES_PER_SAMPLE + 1] =(int8_t)(cos(fm_phase)*127.0);
        }
    }
}
