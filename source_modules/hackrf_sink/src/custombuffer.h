#ifndef CUSTOMBUFFER_H
#define CUSTOMBUFFER_H
#include <gnuradio/sync_block.h>
#include <signal_path/signal_path.h>

class CustomBuffer : public gr::sync_block
{

public:
    typedef std::shared_ptr<CustomBuffer> sptr;
    static sptr make(const std::string& device_name, dsp::stream<dsp::complex_t>& stream_buffer);

    CustomBuffer(const std::string& device_name, dsp::stream<dsp::complex_t>& stream_buffer);
    ~CustomBuffer() override;

private:
    int work(int noutput_items, gr_vector_const_void_star& input_items, gr_vector_void_star& output_items) override;    
    dsp::stream<dsp::complex_t>& stream_buffer;
};

#endif // CUSTOMBUFFER_H
