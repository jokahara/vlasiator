#include <cstdlib>
#include <iostream>

typedef ushort Compf;

#define BLOCK_SIZE 64
#define MIN_VALUE 1e-20f
#define OFFSET 2

class CompressedBlock {
    private:
        typedef union {     
            double f;
            uint64_t i;
        } double_long;    // for converting double into integer format

        Compf *data;

    public:
        CompressedBlock();
        CompressedBlock(const CompressedBlock&);
        ~CompressedBlock() { if (data) free(data); };

        void set(double* data);
        void get(double* array) const;
        void clear();
        
        #define COMP_SIZE
        size_t compressedSize() const;
        bool hasData() const;

        inline Compf* getCompressedData() { return data; };
        inline void prepareToReceiveData(size_t size, bool clearData);

        inline CompressedBlock& operator=(const CompressedBlock& block) {
            clear();
            
            if (!block.data) return *this;
            
            size_t size = block.compressedSize();
            data = (Compf*) malloc(size);
            for (unsigned int i = 0; i < size / sizeof(Compf); i++)
            {
                data[i] = block.data[i];
            }

            return *this;
        }

        // add data of block to another one
        inline CompressedBlock& operator+=(const CompressedBlock& block) {
            if (block.hasData()) {
                if (this->hasData()) {
                    double targetData[BLOCK_SIZE], incomingData[BLOCK_SIZE];
                    block.get(incomingData);
                    this->get(targetData);

                    for (int i = 0; i < BLOCK_SIZE; i++) {
                        targetData[i] += incomingData[i];
                    }
                    this->set(targetData);
                } else {
                    *this = block;
                }
            }
            return *this;
        }
};

inline CompressedBlock::CompressedBlock() {
    data = NULL;
}

inline CompressedBlock::CompressedBlock(const CompressedBlock& block) {
    data = NULL;
    
    if (!block.data) return;

    size_t size = block.compressedSize();
    data = (Compf*) malloc(size);
    for (unsigned int i = 0; i < size / sizeof(Compf); i++)
    {
        data[i] = block.data[i];
    }
}

// Compresses given data block of size 64
inline void CompressedBlock::set(double* array) {
    std::cerr << "!";
    clear();

    ushort n_values = 0;
    ushort nonzero[BLOCK_SIZE];
    for (int i = 0; i < BLOCK_SIZE; i++)
    {
        nonzero[i] = (array[i] > MIN_VALUE);
        n_values += nonzero[i];
    }

    // if block contains only zeroes, data pointer is NULL.
    if (!n_values) return;
    
    // find largest and smallest values to compress
    double_long max, min;
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
    //std::cerr << max.f << " - " << min.f << std::endl;
    min.i &= (0x3FFUL << 52);
    uint range = (max.i - min.i + (3UL << 53)) >> 54;
    ulong magic = (0x3FFEUL << 48) & ( min.i / range + 0x3FFFFFFFFFFFFFUL);
    //std::cerr << "R = " << range << ", M = " << (void*)magic << std::endl;

    // if block contains very few zeroes, the whole block is stored.
    // otherwise the locations of zeroes are marked into 64-bit int.
    if (n_values >= BLOCK_SIZE - 4)
    {
        data = (Compf*) malloc((BLOCK_SIZE + 1) * sizeof(Compf));

        Compf* temp = data + OFFSET;
        double_long value; 
        for (int i = 0; i < BLOCK_SIZE; i++) 
        {
            if (nonzero[i]) {
                value.f = array[i];
                value.i  = magic - ( value.i / range );
                temp[i] = value.i >> 38;     
            }   
            else temp[i] = 0;
        }
    }
    else
    {
        data = (Compf*) malloc((n_values + OFFSET) * sizeof(Compf) + sizeof(long));
        // pointer to start of compressed data
        Compf* temp = data + OFFSET + sizeof(ulong) / sizeof(Compf);

        // bitmask for marking where values in the array are less than MIN_VALUE
        ulong &mask = *(ulong*) (data + OFFSET);
        mask = 0;

        double_long value; 
        for (int i = 0; i < BLOCK_SIZE; i++)
        {
            if (nonzero[i]) {
                mask |= (1UL << i);

                // compression of value with the fast inverse root method
                value.f = array[i];
                value.i  = magic - ( value.i / range );
                *temp++ = value.i >> 38;
            }
        }
    }
    // saving number of values, range and magic number at start of the array
    *data = n_values + (range << 8);
    *(data + 1) = (magic >> 48);
}

inline void CompressedBlock::get(double *array) const {
    if (!hasData()) {
        for (int i = 0; i < BLOCK_SIZE; i++) 
            array[i] = 0.f;
        return;
    }

    int range = *data >> 8;
    long magic = ( (long)*(data+1) | 0x700UL ) << 48;

    if ((*data & 0xFF) >= BLOCK_SIZE - 4)
    {
        double_long value;
        Compf *temp = data + OFFSET;
        for (int i = 0; i < BLOCK_SIZE; i++) 
        {
            if (temp[i]) {
                value.i = (1UL << 38) |((ulong) temp[i] << 38);
                value.i = range * (magic - value.i);
                array[i] = value.f;
            }
            else array[i] = 0.0;
        }
    }
    else
    {
        Compf *temp = data + OFFSET + sizeof(ulong) / sizeof(Compf);
        const ulong mask = *(ulong*) (data + OFFSET);

        double_long value;
        for (int i = 0; i < BLOCK_SIZE; i++)
        {
            if ((mask >> i) & 1UL) {
                value.i = (1UL << 37) |((ulong)*temp++ << 38);
                value.i = range * (magic - value.i);
                array[i] = value.f;
            }
            else array[i] = 0.0;
        }
    }
}

inline void CompressedBlock::clear() {
    if (data) free(data);
    data = NULL;
}

inline bool CompressedBlock::hasData() const {
    return data;
}

// pre-allocate memory before MPI transfer
inline void CompressedBlock::prepareToReceiveData(size_t size, bool clearData=true) { 
    if (clearData) clear(); 
    else data = NULL;

    if (size == 0) return;
    data = (Compf*) malloc(size);
    //data[0] = 0;
}

inline size_t CompressedBlock::compressedSize() const { 
    if(!hasData()) return 0;

    Compf n_values = *data & 0xFF;
    return (n_values >= BLOCK_SIZE - 4) 
            ? (BLOCK_SIZE + OFFSET) * sizeof(Compf)
            : (n_values + OFFSET) * sizeof(Compf) + sizeof(long); 
}