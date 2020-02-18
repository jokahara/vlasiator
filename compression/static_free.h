#include <cstdlib>
#include <iostream>

typedef ushort Compf;

#define BLOCK_SIZE 64
#define MIN_VALUE 1e-17f    // minimum between 1e-17f and 1e-18f recommended
#define OFFSET 2

class CompressedBlock {
    private:
        typedef union {     
            float f;
            uint32_t i;
        } float_int;    // for converting float into integer format
        CompressedBlock() { };

    public:
        static int set(float* data, Compf* p);
        static int get(float* data, Compf* p);
};

// Compresses given data block to p.
inline int CompressedBlock::set(float* data, Compf* p) {

    ushort n_values = 0;
    ushort nonzero[BLOCK_SIZE];
    for (int i = 0; i < BLOCK_SIZE; i++)
    {
        nonzero[i] = (data[i] > MIN_VALUE);
        n_values += nonzero[i];
    }

    // if block contains only zeroes, data pointer is NULL.
    if (!n_values) {
        *p = 0;
        return 1;
    }
    
    int block_size = n_values + OFFSET;

    // find largest and smallest values to compress
    float_int max, min;
    if (n_values == BLOCK_SIZE)
    {
        max.f = data[0];
        min.f = data[0];
        for (int i = 1; i < BLOCK_SIZE; i++){
            if (data[i] > max.f) max.f = data[i]; 
            else if (data[i] < min.f) min.f = data[i];
        }
    }
    else
    {
        max.f = data[0];
        min.f = MIN_VALUE;
        for (int i = 1; i < BLOCK_SIZE; i++){
            if (data[i] > max.f) max.f = data[i]; 
        }
    }

    uint range = (max.i - min.i + 0x3FFFFF) >> 21;
    uint magic = 0x3FFFC000 & ( min.i / range + 0x1FFFFF);
    
    // if block contains very few zeroes, the whole block is stored.
    // otherwise the locations of zeroes are marked into 64-bit int.
    if (n_values >= BLOCK_SIZE - 4)
    {   
        Compf* temp = p + OFFSET;
        float_int value; 
        for (int i = 0; i < BLOCK_SIZE; i++) 
        {
            if (nonzero[i]) {
                value.f = data[i];
                value.i = magic - ( value.i / range );
                temp[i] = value.i >> 5;        
            } 
            else temp[i] = 0;
        }
    }
    else
    {
        // pointer to start of compressed data
        Compf* temp = p + OFFSET + sizeof(ulong) / sizeof(Compf);

        // bitmask for marking where values in the array are less than MIN_VALUE
        ulong &mask = *(ulong*) (p + OFFSET);
        mask = 0;

        float_int value; 
        for (int i = 0; i < BLOCK_SIZE; i++)
        {
            if (nonzero[i]) {
                mask |= (1UL << i);

                // compression of value with the fast inverse root method
                value.f = data[i];
                value.i = magic - ( value.i / range );
                *temp++ = value.i >> 5;
            }
        }

        block_size += sizeof(ulong) / sizeof(Compf);
    }
    // saving number of values, range and magic number at start of the array
    *p = n_values + (range << 8);
    *(p + 1) = (magic >> 14);

    return block_size;
}

inline int CompressedBlock::get(float *data, Compf *p) {
    int n_values = (*p & 0xFF);
    if (n_values == 0) {
        for (int i = 0; i < BLOCK_SIZE; i++) 
            data[i] = 0.f;
        return 1;
    }

    int range = *p >> 8;
    int magic = *(p+1) << 14;

    if (n_values >= BLOCK_SIZE - 4)
    {
        float_int value;
        Compf *temp = p + OFFSET;
        for (int i = 0; i < BLOCK_SIZE; i++) 
        {
            if (temp[i]) {
                value.i = 0xF + (temp[i] << 5);
                value.i = range * (magic - value.i);
                data[i] = value.f;
            }
            else data[i] = 0.f;
        }
        return n_values + OFFSET;
    }
    else
    {
        Compf *temp = p + OFFSET + sizeof(ulong) / sizeof(Compf);
        const ulong mask = *(ulong*) (p + OFFSET);

        float_int value;
        for (int i = 0; i < BLOCK_SIZE; i++)
        {
            if ((mask >> i) & 1UL) {
                value.i = 0xF + (*temp++ << 5);
                value.i = range * (magic - value.i);
                data[i] = value.f;
            }
            else data[i] = 0.f;
        }
        return n_values + OFFSET + sizeof(ulong) / sizeof(Compf);
    }
}