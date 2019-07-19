#include "zfp.h"
#include <cstdlib>

#define PRECISION 16
#define BLOCK_SIZE 64

class CompressedBlock {

    private:
        uchar* buffer;
        unsigned short bufsize;
        zfp_stream* zfp;
        
    public:
        CompressedBlock();
        ~CompressedBlock();
        CompressedBlock(const CompressedBlock&);

        unsigned int set(float* data);
        unsigned int get(float* array);
        const unsigned int get(float* array) const;
        void clear();

        size_t compressedSize() const;

        CompressedBlock& operator=(const CompressedBlock& block) {
            clear();
            bufsize = block.bufsize;
            if (block.buffer)
            {
                buffer = (uchar*) malloc(bufsize);
                for (uint i = 0; i < bufsize; i++)
                {
                    buffer[i] = block.buffer[i];
                }
            }
            return *this;
        }
};

inline CompressedBlock::CompressedBlock() {
    buffer = NULL;
    bufsize = 0;

    zfp = zfp_stream_open(NULL);
    zfp_stream_set_precision(zfp, PRECISION);
}

// use of this not recommended because it is slow
inline CompressedBlock::CompressedBlock(const CompressedBlock& block) {
    buffer = NULL;
    bufsize = 0;

    zfp = zfp_stream_open(NULL);
    zfp_stream_set_precision(zfp, PRECISION);
    *this = block;
}

// Compresses given data block of size 64
// returns the compressed size (or zero if unsuccesful)
inline unsigned int CompressedBlock::set(float* data) {
    clear();

    zfp_field* field = zfp_field_1d(data, zfp_type_float, BLOCK_SIZE);
    //zfp_field* field = zfp_field_3d(data, zfp_type_float, 4, 4, 4);

    bufsize = zfp_stream_maximum_size(zfp, field);  // estimate of compressed size
    uchar temp[bufsize];                            // temporary buffer to compress data to
    
    // associate bit stream with allocated buffer
    bitstream* stream = stream_open(temp, bufsize);
    zfp_stream_set_bit_stream(zfp, stream);

    bufsize = zfp_compress(zfp, field);

    // copy compressed data to smaller buffer
    buffer = (uchar*) malloc(bufsize);
    for (int i = 0; i < bufsize; i++)
    {
        buffer[i] = temp[i];
    }
    
    zfp_field_free(field);
    stream_close(stream);
    
    return bufsize;
}

inline unsigned int CompressedBlock::get(float *array) {
    if (buffer) {
        bitstream* stream = stream_open(buffer, bufsize);
        zfp_stream_set_bit_stream(zfp, stream);

        zfp_field* field = zfp_field_1d(array, zfp_type_float, BLOCK_SIZE);
        //zfp_field* field = zfp_field_3d(array, zfp_type_float, 4, 4, 4);
        zfp_decompress(zfp, field);

        zfp_field_free(field);
        stream_close(stream);
    }
    else
    {
        for (int i = 0; i < BLOCK_SIZE; i++) array[i] = 0.f;
    }
    return bufsize;
}

inline const unsigned int CompressedBlock::get(float *array) const {
    if (buffer) {
        bitstream* stream = stream_open(buffer, bufsize);
        zfp_stream_set_bit_stream(zfp, stream);

        zfp_field* field = zfp_field_1d(array, zfp_type_float, BLOCK_SIZE);
        //zfp_field* field = zfp_field_3d(array, zfp_type_float, 4, 4, 4);
        zfp_decompress(zfp, field);
        
        zfp_field_free(field);
        stream_close(stream);
    }
    else
    {
        for (int i = 0; i < BLOCK_SIZE; i++) array[i] = 0.f;
    }
    return bufsize;
}

inline void CompressedBlock::clear() {
    if(buffer) {
        free(buffer);
        bufsize = 0;
        buffer = NULL;
    }
}

inline size_t CompressedBlock::compressedSize() const {
    return bufsize + sizeof(zfp_stream);
}

inline CompressedBlock::~CompressedBlock() {
    if(buffer) {
        free(buffer);
    }
    if(zfp) zfp_stream_close(zfp);
}
