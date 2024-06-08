#include "custombuffer.h"

CustomBuffer::CustomBuffer(const std::string& device_name,
                           dsp::stream<dsp::complex_t>& stream_buffer):
    gr::sync_block(device_name,
                   gr::io_signature::make(1, 1, sizeof(gr_complex)),
                   gr::io_signature::make(0, 0, 0)),
    stream_buffer(stream_buffer)
{

}

CustomBuffer::~CustomBuffer()
{

}

int CustomBuffer::work(int noutput_items, gr_vector_const_void_star &input_items, gr_vector_void_star &output_items)
{
    const gr_complex* in = static_cast<const gr_complex*>(input_items[0]);
    // memcpy(stream_buffer.writeBuf, in, noutput_items * sizeof(gr_complex));
    // stream_buffer.swap(noutput_items);

    size_t output_buffer_size = noutput_items * sizeof(gr_complex);
    std::cout << "noutput_items : " << noutput_items << " gr_complex : " << sizeof(gr_complex) << " total : " <<output_buffer_size << " bytes" << std::endl;

    return noutput_items;
}

CustomBuffer::sptr CustomBuffer::make(const std::string& device_name,
                                      dsp::stream<dsp::complex_t>& stream_buffer)
{
    return std::make_shared<CustomBuffer>(device_name, stream_buffer);
}

