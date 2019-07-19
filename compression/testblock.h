
#include <vector>

class CompressedBlock {

    private:
        std::vector<float> v;
        
    public:
        CompressedBlock();
        CompressedBlock(const CompressedBlock&);

        void set(float* data);
        void get(float* data) const;
        void clear() {
            for (int i = 0; i < 64; i++)
            {
                v[i] = 0.f;
            }
        }

        int compressedSize() {return 64*4;}
        const int compressedSize() const {return 64*4;}

        float &operator[](const unsigned int i) {
            return v[i];
        }
        const float &operator[](const unsigned int i) const {
            return v[i];
        }
        CompressedBlock& operator=(const CompressedBlock& block) {
            for (int i = 0; i < 64; i++)
            {
                v[i] = block.v[i];
            }
            return *this;
        }
};

inline CompressedBlock::CompressedBlock() {
    v.resize(64);
}

// use of this not recommended because it is slow
inline CompressedBlock::CompressedBlock(const CompressedBlock& block) {
    v.resize(64);
    for (int i = 0; i < 64; i++)
    {
        v[i] = block.v[i];
    }
    
}

// Compresses given data block of size 64
// returns the compressed size (or zero if unsuccesful)
inline void CompressedBlock::set(float* data) {
    for (int i = 0; i < 64; i++)
    {
        v[i] = data[i];
    }
}

inline void CompressedBlock::get(float* data) const {
    for (int i = 0; i < 64; i++)
    {
        data[i] = v[i];
    }
}