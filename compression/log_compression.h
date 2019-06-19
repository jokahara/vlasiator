#include <cmath>
#include <iostream>
// Quite fast, 50 % float compressing with small error.
// Designed to work for values between 1E-3 - 1E-15.
class CompressedValue
{
    private:
        typedef union {     
            float f;
            long i;
        } float_int;    // for converting float into integer format
        unsigned short compressed_value;
        
    public:
        CompressedValue();
        CompressedValue(float x);
        float get() const;
        void set(float x);
        //static float *decompressArray(CompressedValue *array, int size);

        operator float() { return get(); }
        operator float() const { return get(); }

        CompressedValue& operator=(float value) { set(value); return *this; }
        CompressedValue& operator+=(float value) { set(get() + value); return *this; }
        
        CompressedValue& operator=(const CompressedValue& value) { 
            compressed_value = value.compressed_value; 
            return *this;    
        }
        CompressedValue& operator+=(CompressedValue value) { 
            set(get() + value.get()); 
            return *this;    
        }
        CompressedValue& operator*=(CompressedValue value) { 
            float_int v1 { .i = 0xC00000FE }, v2 { .i = 0xC00000FE };
            v1.i |= compressed_value << 9;
            v2.i |= value.compressed_value << 9;

            v1.f += v2.f;
            compressed_value = v1.i >> 9;
            return *this;
        }
        CompressedValue& operator/=(CompressedValue value) { 
            float_int v1 { .i = 0xC00000FE }, v2 { .i = 0xC00000FE };
            v1.i |= compressed_value << 9;
            v2.i |= value.compressed_value << 9;

            v1.f -= v2.f;
            compressed_value = v1.i >> 9;
            return *this;
        }
};

inline CompressedValue::CompressedValue() { compressed_value = 0; }

inline CompressedValue::CompressedValue(float x) { set(x); }

inline void CompressedValue::set(float x)
{
    if (x < 1E-17f)
    {
        compressed_value = 0;
    }
    else
    {
        float_int fi { .f = log10f(x) };
        compressed_value = fi.i >> 9; 
    }   
}

inline float CompressedValue::get() const
{
    if (compressed_value == 0) return 0.0f;

    float_int fi { .i = 0xC00000FE };
    fi.i |= compressed_value << 9;

    return exp10f(fi.f);
}
/* 
inline float *CompressedValue::decompressArray(CompressedValue *array, int size){
    float cp[size];
    for (int i = 0; i < size; i++)
    {
        cp[i] = array[i];
    }
    
    return cp;
}
*/
