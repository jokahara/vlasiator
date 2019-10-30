#include <cstdlib>

typedef ushort Compf;

#define BLOCK_SIZE 64
#define MIN_VALUE 1e-20f

class CompressedBlock {
    private:
        typedef union {     
            float f;
            uint32_t i;
        } float_int;    // for converting float into integer format

        Compf data[BLOCK_SIZE];
        ushort magic, range;

    public:
        CompressedBlock();
        CompressedBlock(const CompressedBlock&);

        void set(float* data);
        void get(float* array) const;
        void clear();

        inline CompressedBlock& operator=(const CompressedBlock& block) {
            clear();
            
            magic = block.magic;
            range = block.range;
            if (!magic) return *this;
            
            for (unsigned int i = 0; i < BLOCK_SIZE; i++)
            {
                data[i] = block.data[i];
            }

            return *this;
        }
};

inline CompressedBlock::CompressedBlock() {
    clear();
}

inline CompressedBlock::CompressedBlock(const CompressedBlock& block) {
    magic = block.magic;
    range = block.range;
    if (!magic) return;
    
    for (unsigned int i = 0; i < BLOCK_SIZE; i++)
    {
        data[i] = block.data[i];
    }
}

// Compresses given data block of size 64
inline void CompressedBlock::set(float* array) {
    clear();

    int n_values = 0;
    for (int i = 0; i < BLOCK_SIZE; i++)
    {
        if (array[i] > MIN_VALUE) n_values++;
    }

    if (!n_values) return;
    
    // find largest and smallest values to compress
    float_int max, min;
    if (n_values == BLOCK_SIZE)
    {
        max.f = array[0];
        min.f = array[0];
        for (int i = 1; i < BLOCK_SIZE; i++){
            if (array[i] > max.f) max.f = array[i]; 
            else if (array[i] < min.f) min.f = array[i];
        }
    }
    else
    {
        max.f = array[0];
        min.f = MIN_VALUE;
        for (int i = 1; i < BLOCK_SIZE; i++){
            if (array[i] > max.f) max.f = array[i]; 
        }
    }

    min.i &= 0x3F800000;
    range = (max.i - min.i + 0x3000000) >> 25;
    int m = 0x3FF00000 & ( min.i / range + 0x1FFFFFF);
    
    float_int value; 
    for (int i = 0; i < BLOCK_SIZE; i++) 
    {
        if (array[i] < MIN_VALUE) data[i] = 0;
        else {
            value.f = array[i];
            value.i  = m - ( value.i / range );
            data[i] = value.i >> 9;        
        }   
    }
    magic = m >> 16;
}

inline void CompressedBlock::get(float *array) const {
    
    if (!magic)
    {
        for (int i = 0; i < BLOCK_SIZE; i++) 
            array[i] = 0.f;
        return;
    }

    int m = magic << 16;
    float_int value;
    for (int i = 0; i < BLOCK_SIZE; i++) 
    {
        if (data[i] == 0) array[i] = 0.f;
        else {
            value.i = 0xFF |(data[i] << 9);
            value.i = range * (m - value.i);
            array[i] = value.f;
        }
    }
}

inline void CompressedBlock::clear() {
    range = magic = 0;
}