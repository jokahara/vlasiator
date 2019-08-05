#include <iostream>
// 50 % float compressing, much faster than logarithmic approach.
// Values beyond the limit of 1E-3 - 1E-15 will not work.
class CompressedValue
{
    private:
        typedef union {     
            float f;
            uint32_t i;
        } float_int;    // for converting float into integer format
        uint16_t compressed_value;
        
    public:
        CompressedValue();
        CompressedValue(float x);
        float get() const;
        void set(float x);

        operator float() { return get(); }
        operator float() const { return get(); }

        CompressedValue& operator=(float value) { set(value); return *this; }
        CompressedValue& operator+=(float value) { set(get() + value); return *this; }
        CompressedValue& operator*=(float value) { set(get() * value); return *this; }
        
        CompressedValue& operator=(const CompressedValue& value) { 
            compressed_value = value.compressed_value; 
            return *this;    
        }
        CompressedValue& operator+=(CompressedValue value) { 
            set(get() + value.get()); 
            return *this;    
        }
        CompressedValue& operator*=(CompressedValue value) { 
            set(get() * value.get()); 
            return *this;
        }
};

inline CompressedValue::CompressedValue() { compressed_value = 0; }

inline CompressedValue::CompressedValue(float x) { set(x); }

inline void CompressedValue::set(float x)
{
    if (x < 9E-16f) {
        compressed_value = 0;
        return;
    }
    
    // approximation of inverse tenth root by using a magic number: 
    // https://en.wikipedia.org/wiki/Fast_inverse_square_root
    float_int value { .f = x };
    value.i  = 0x45d169c0 - ( value.i / 10 );

    compressed_value = value.i >> 9; 
}

inline float CompressedValue::get() const
{
    if (compressed_value == 0) return 0.0f;
    
    float_int value { .i = 0x400000FF };
    value.i |= compressed_value << 9;
    value.i = 10 * (0x45d169c0 - value.i);

    return value.f;
}