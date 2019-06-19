#include <cmath>

// Quite fast, 50 % float compressing with small error.
// Designed to work for values between 1E-3 - 1E-15.
class LogCompressedValue
{
    private:
        typedef union {     
            float f;
            long i;
        } float_int;    // for converting float into integer format
        unsigned short compressed_value;
        
    public:
        LogCompressedValue();
        LogCompressedValue(float x);
        float get() const;
        void set(float x);
        
        operator float() { return get(); }
        operator float() const { return get(); }

        LogCompressedValue& operator=(float value) { set(value); return *this; }
        LogCompressedValue& operator+=(float value) { set(get() + value); return *this; }
        
        LogCompressedValue& operator=(const LogCompressedValue& value) { 
            compressed_value = value.compressed_value; 
            return *this;    
        }
        LogCompressedValue& operator+=(LogCompressedValue value) { 
            set(get() + value.get()); 
            return *this;    
        }
};

inline LogCompressedValue::LogCompressedValue() { compressed_value = 0; }

inline LogCompressedValue::LogCompressedValue(float x) { set(x); }

inline void LogCompressedValue::set(float x)
{
    float_int fi { .f = x ? log10f(x) : 0 };
    compressed_value = fi.i >> 9; 
}

inline float LogCompressedValue::get() const
{
    float_int fi { .i = 3 << 30};
    fi.i |= compressed_value << 9;
    fi.i |= 0xFE;                   // average error correction;

    return exp10f(fi.f);
}

