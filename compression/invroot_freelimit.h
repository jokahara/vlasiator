#include <cstdlib>
#include <iostream>

typedef ushort Compf;

#define BLOCK_SIZE 64
#define MIN_VALUE 1e-18f
#define OFFSET 2

class CompressedBlock {
    private:
        typedef union {     
            float f;
            uint32_t i;
        } float_int;    // for converting float into integer format

        ushort range;
        uint magic;
        Compf data[BLOCK_SIZE];

    public:
        CompressedBlock();
        CompressedBlock(const CompressedBlock&);

        void set(float* data);
        void get(float* array) const;
        void clear();

        inline CompressedBlock& operator=(const CompressedBlock& block) {
            range = block.range;
            magic = block.magic;
            if (range == 0) return *this;
            
            for (unsigned int i = 0; i < BLOCK_SIZE; i++)
            {
                data[i] = block.data[i];
            }

            return *this;
        }
};

inline CompressedBlock::CompressedBlock() {
    range = magic = 0;
}

inline CompressedBlock::CompressedBlock(const CompressedBlock& block) {
    range = block.range;
    magic = block.magic;
    if (range == 0) return;

    for (unsigned int i = 0; i < BLOCK_SIZE; i++)
    {
        data[i] = block.data[i];
    }
}

// Compresses given data block of size 64
inline void CompressedBlock::set(float* array) {
    clear();
    
    // find the smallest and the largest values in the array.
    float_int max { .f = array[0] }, min { .f = array[0] };
    
    for (int i = 1; i < BLOCK_SIZE; i++){
        if (array[i] > max.f) max.f = array[i]; 
        else if (array[i] < min.f) min.f = array[i];
    }

    if (min.f < MIN_VALUE) {
        min.f = MIN_VALUE; 
        if (max.f < min.f) max.f = min.f;
    }
    min.i &= 0x3F800000;
    range = (max.i - min.i + 0x3000000) >> 25;
    magic = 0x3FF00000 & ( min.i / range + 0x1FFFFFF);

    //std::cerr << (int)range << " " << (void*) magic << std::endl;

    float_int value; 
    for (int i = 0; i < BLOCK_SIZE; i++) 
    {
        if (array[i] < MIN_VALUE) data[i] = 0;
        else {
            value.f = array[i];
            value.i  = magic - ( value.i / range );
            data[i] = value.i >> 9;      
        }  
    }
}

inline void CompressedBlock::get(float *array) const {
    if (range == 0)
    {
        for (int i = 0; i < BLOCK_SIZE; i++) 
            array[i] = 0.f;
        return;
    }

    float_int value;
    for (int i = 0; i < BLOCK_SIZE; i++) 
    {
        if (data[i] == 0) array[i] = 0.f;
        else {
            value.i = 0xFF |(data[i] << 9);
            value.i = range * (magic - value.i);
            array[i] = value.f;
        }
    }
}

inline void CompressedBlock::clear() {
    range = 0;
    magic = 0;
}
