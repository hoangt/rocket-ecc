#define __STDC_FORMAT_MACROS

#include "bigint.h++"
#include <assert.h>
#include <iostream>
#include <stack>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Converts a hex character to an integer value. */
static int hex2int(unsigned char c);

BigInt::BigInt(std::string hex, int bit_length)
    : _bit_length(bit_length),
      _overflow(false)
{
    const char *str;

    str = hex.c_str();

    assert(this->bit_length() == 256);
    sscanf(str, "%016" PRIX64 "%016" PRIX64 "%016" PRIX64 "%016" PRIX64,
           this->_data + 0,
           this->_data + 1,
           this->_data + 2,
           this->_data + 3);
}

BigInt::BigInt(std::string hex, int offset, int bit_length)
    : _bit_length(bit_length),
      _overflow(false)
{
    const char *str;

    str = hex.c_str() + (offset / 4);

    assert(this->bit_length() == 256);
    sscanf(str, "%016" PRIX64 "%016" PRIX64 "%016" PRIX64 "%016" PRIX64,
           this->_data + 0,
           this->_data + 1,
           this->_data + 2,
           this->_data + 3);
}

BigInt::BigInt(const BigInt &other)
    : _bit_length(other._bit_length),
      _overflow(other._overflow)
{
    assert(this->bit_length() == other.bit_length());

    for (int i = 0; i < this->word_length(); i++)
        this->_data[i] = other._data[i];
}

BigInt::BigInt(int value, int bit_length)
    : _bit_length(bit_length),
      _overflow(false)
{
    for (int i = 0; i < this->word_length(); i++)
        this->_data[i] = 0;

    /* FIXME: GCC with optimizations complains when this isn't here.
     * I have no idea why.  The Internet suggests the warning
     * (-Werror=array-bounds) has lots of false positives, so I think
     * it's probably best to just work around. */
    assert(this->word_length() > 0);
    if (this->word_length() <= 0)
        abort();

    this->_data[this->word_length() - 1] = value;
}

BigInt::BigInt(const bigint_datum_t *a, bool overflow, int bit_length)
    : _bit_length(bit_length),
      _overflow(overflow)
{
    for (int i = 0; i < this->word_length(); i++)
        this->_data[i] = a[this->word_length() - i - 1];
}

std::string BigInt::hex(void) const
{
    char buf[1024];

    switch (this->bit_length()) {
    case 256:
        sprintf(buf, "%016" PRIX64 "%016" PRIX64 "%016" PRIX64 "%016" PRIX64,
                 this->_data[0],
                 this->_data[1],
                 this->_data[2],
                 this->_data[3]
            );
        break;

    case 512:
        sprintf(buf, "%016" PRIX64 "%016" PRIX64 "%016" PRIX64 "%016" PRIX64 "%016" PRIX64 "%016" PRIX64 "%016" PRIX64 "%016" PRIX64,
                 this->_data[0],
                 this->_data[1],
                 this->_data[2],
                 this->_data[3],
                 this->_data[4],
                 this->_data[5],
                 this->_data[6],
                 this->_data[7]
            );
        break;

    default:
        abort();
    }

    return buf;
}

const char *BigInt::hex_cstr(void) const
{
    char *out;

    out = (char *)malloc(strlen(this->hex().c_str()) + 1);
    strcpy(out, this->hex().c_str());

    return out;
}

const unsigned char *BigInt::byte_str(void) const
{
    const char *hex;
    unsigned char *bytes, *bytes_out;

    hex = this->hex_cstr();
    bytes = (unsigned char *)malloc((size_t)this->byte_length());
    bytes_out = bytes;

    for (int i = 0; i < this->byte_length(); i++) {
        unsigned int val;

        val = hex2int(*hex);
        val <<= 4;
        hex++;

        val += hex2int(*hex);
        hex++;

        *bytes = val;
        bytes++;
    }

    return bytes_out;
}

BigInt BigInt::cof(void) const
{
    BigInt o(*this);
    o._overflow = false;
    return o;
}

BigInt BigInt::extend(int new_bit_length) const
{
    BigInt out(0, new_bit_length);
    assert(new_bit_length >= this->bit_length());

    int offset = out.word_length() - this->word_length();
    for (int i = 0; i < this->word_length(); i++)
        out._data[i + offset] = this->_data[i];

    return out;
}

BigInt BigInt::trunc(int new_bit_length) const
{
    BigInt out(0, new_bit_length);
    assert(new_bit_length <= this->bit_length());

    int offset = this->word_length() - out.word_length();
    for (int i = 0; i < this->word_length(); i++)
        out._data[i] = this->_data[i + offset];

    return out;
}

BigInt operator+(const BigInt &a, const BigInt &b)  __attribute__((weak));
BigInt operator+(const BigInt &a, const BigInt &b) 
{
    assert(sizeof(a._data[0]) == 8);
    assert(a.bit_length() == b.bit_length());

    bigint_double_datum_t sum = 0;
    BigInt out(0, a.bit_length());
    for (int i = a.word_length()-1; i >= 0; i--) {
        sum += a._data[i];
        sum += b._data[i];
        out._data[i] = sum;
        sum >>= sizeof(sum) * 4;
    }

    out._overflow = (sum > 0) || a._overflow || b._overflow;

    return out;
}

BigInt operator-(const BigInt &a, const BigInt &b) __attribute__((weak));
BigInt operator-(const BigInt &a, const BigInt &b)
{
    assert(a.bit_length() == b.bit_length());

    BigInt out(a + (~b) + BigInt(1, a.bit_length()));

    if (a >= b)
        out._overflow = false;
    else
        out._overflow = true;

    return out;
}

BigInt operator*(const BigInt &a, const BigInt &b) __attribute__((weak));
BigInt operator*(const BigInt &a, const BigInt &b)
{
    assert(sizeof(a._data[0]) == 8);
    assert(a.bit_length() == b.bit_length());

    BigInt out(0, a.bit_length());
    bigint_double_datum_t sum = 0;
    for (int p = a.word_length()-1; p >= 0; p--) {
        int count, i ,j;
        bigint_double_datum_t overflow;

        count = a.word_length() - p;
        i = a.word_length() - count;
        j = a.word_length() - 1;
        overflow = 0;
        while (count > 0) {
            bigint_double_datum_t ba = a._data[i];
            bigint_double_datum_t bb = b._data[j];
            bigint_double_datum_t prod = ba * bb;

            if (BIGINT_DOUBLE_DATUM_MAX - prod < sum)
                overflow++;

            sum += (ba * bb);

            i++;
            j--;
            count--;
        }

        out._data[p] = sum;
        sum >>= sizeof(sum) * 4;
        sum += (overflow << (sizeof(sum) * 4));
    }

    out._overflow = (sum > 0) || a._overflow || b._overflow;

    return out;
}

BigInt operator%(const BigInt &x, const BigInt &m) __attribute__((weak));
BigInt operator%(const BigInt &x, const BigInt &m)
{
    assert(m != 0);
    assert(x.bit_length() == m.bit_length());

    /* These two are special cases: the ModInt initialization code
     * calls this operator a whole bunch of times and I don't want to
     * bust out a whole shift/mod thing if I don't have to.  Usually
     * ModInt calls it with one of these two enabled, so this makes it
     * easier. */
#ifndef SKIP_SHORT_MOD
    if (x < m)
        return x;
    if (x - m < m)
        return x - m;

    assert(x > m);
#endif

    BigInt rem(x);
    BigInt acc(0, x.bit_length());

    for (int i = x.bit_length()-1; i >= 0; i--) {
        BigInt tri = m << i;

        acc = acc + acc;
        if (rem > tri) {
            rem = rem - tri;
            acc = acc + m;
        }
        
    }

    if (rem >= m)
        return rem - m;

    return rem;
}

BigInt BigInt::operator~(void) const
{
    BigInt out(0, this->bit_length());

    assert (this->bit_length() == out.bit_length());
    for (int i = 0; i < this->word_length(); i++)
        out._data[i] = ~this->_data[i];

    return out;
}

BigInt BigInt::operator<<(int i) const
{
    static const int max_bit_length = (sizeof(bigint_datum_t) * 8);

    assert(i < BIGINT_MAX_BIT_LENGTH);
    assert(i < this->bit_length());

    if (i > max_bit_length) {
        BigInt near = (*this << max_bit_length);
        BigInt far = near << (i - max_bit_length);

        if (near._overflow)
            far._overflow = true;

        return far;
    }

    if (i == max_bit_length) {
        BigInt out(*this);

        out._overflow = (out._data[0] > 0);

        for (int j = 0; j < this->word_length()-1; j++)
            out._data[j] = out._data[j+1];

        out._data[this->word_length()-1] = 0;
        return out;
    }

    BigInt out(*this);

    {
        bigint_double_datum_t d;
        d = this->_data[0];
        d <<= i;
        d >>= (sizeof(d) * 4);
        out._overflow = (d > 0);
    }

    for (int j = 0; j < this->word_length()-1; j++) {
        bigint_double_datum_t d;

        d = this->_data[j];
        d <<= (sizeof(d) * 4);
        d |= this->_data[j+1];

        d <<= i;

        out._data[j] = (d >> (sizeof(d) * 4));
    }

    out._data[this->word_length()-1] <<= i;
    if (i == max_bit_length)
        out._data[this->word_length()-1] = 0;

    return out;
}

BigInt BigInt::operator>>(int i) const
{
    BigInt out(*this);

    for (int j = 1; j < this->word_length(); j++) {
        bigint_double_datum_t d;

        d = this->_data[j-1];
        d <<= (sizeof(d) * 4);
        d += this->_data[j];

        d >>= i;

        out._data[j] = d;
    }

    out._data[0] = this->_data[0] >> i;

    return out;
}

bool operator==(const BigInt &a, const BigInt &b)
{
    assert(a.bit_length() == b.bit_length());

    for (int i = 0; i < a.word_length(); i++)
        if (a._data[i] != b._data[i])
            return false;

    return true;
}

bool operator>(const BigInt &a, const BigInt &b)
{
    assert(a.bit_length() == b.bit_length());

    if (a._overflow && !b._overflow)
        return true;
    if (!a._overflow && b._overflow)
        return false;

    for (int i = 0; i < a.word_length(); i++) {
        if (a._data[i] > b._data[i])
            return true;
        if (a._data[i] < b._data[i])
            return false;
    }

    return false;
}

bool operator>=(const BigInt &a, const BigInt &b)
{
    assert(a.bit_length() == b.bit_length());

    if (a._overflow && !b._overflow)
        return true;
    if (!a._overflow && b._overflow)
        return false;

    for (int i = 0; i < a.word_length(); i++) {
        if (a._data[i] > b._data[i])
            return true;
        if (a._data[i] < b._data[i])
            return false;
    }

    return true;
}

BigInt add_shift_one(const BigInt &a, const BigInt &b)
{
    BigInt out = (a >> 1) + (b >> 1);

    if (a.is_odd() && b.is_odd())
        return out + 1;

    return out;
}

void BigInt::write_array(bigint_datum_t *a) const
{
    for (int i = 0; i < this->word_length(); i++)
        a[i] = this->_data[this->word_length() - i - 1];
}

int hex2int(unsigned char c)
{
    switch (c) {
    case '0': return 0;
    case '1': return 1;
    case '2': return 2;
    case '3': return 3;
    case '4': return 4;
    case '5': return 5;
    case '6': return 6;
    case '7': return 7;
    case '8': return 8;
    case '9': return 9;
    case 'a': return 10;
    case 'b': return 11;
    case 'c': return 12;
    case 'd': return 13;
    case 'e': return 14;
    case 'f': return 15;
    case 'A': return 10;
    case 'B': return 11;
    case 'C': return 12;
    case 'D': return 13;
    case 'E': return 14;
    case 'F': return 15;
    }

    return -1;
}

#ifdef BIGINT_TEST_HARNESS
int main(int argc, char **argv)
{
    int i;
    std::stack<BigInt> stack;

    for (i = 1; i < argc; i++) {
        std::cerr << "argv[" << i << "] = " << argv[i] << "\n";

        if (strcmp(argv[i], "+") == 0) {
            BigInt a = stack.top(); stack.pop();
            BigInt b = stack.top(); stack.pop();
            std::cerr << "sum1 " << a.hex() << "\n";
            std::cerr << "sum2 " << b.hex() << "\n";
            stack.push(a + b);
        } else if (strcmp(argv[i], "-") == 0) {
            BigInt a = stack.top(); stack.pop();
            BigInt b = stack.top(); stack.pop();
            std::cerr << "diff1 " << a.hex() << "\n";
            std::cerr << "diff2 " << b.hex() << "\n";
            stack.push(a - b);
        } else if (strcmp(argv[i], "x") == 0) {
            BigInt a = stack.top(); stack.pop();
            BigInt b = stack.top(); stack.pop();
            std::cerr << "prod1 " << a.hex() << "\n";
            std::cerr << "prod2 " << b.hex() << "\n";
            stack.push(a * b);
        } else if (strcmp(argv[i], "%") == 0) {
            BigInt a = stack.top(); stack.pop();
            BigInt b = stack.top(); stack.pop();
            std::cerr << "mod1 " << a.hex() << "\n";
            std::cerr << "mod2 " << b.hex() << "\n";
            stack.push(a % b);
        } else if (strcmp(argv[i], "lsh") == 0) {
            BigInt d = stack.top(); stack.pop();
            int b = atoi(argv[++i]);
            std::cerr << "lshD " << d.hex() << "\n";
            std::cerr << "lshB " << b << "\n";
            stack.push(d << b);
        } else if (strcmp(argv[i], "rsh") == 0) {
            BigInt d = stack.top(); stack.pop();
            int b = atoi(argv[++i]);
            std::cerr << "rshD " << d.hex() << "\n";
            std::cerr << "rshB " << b << "\n";
            stack.push(d >> b);
        } else {
            stack.push(BigInt(argv[i], BIGINT_TEST_BIT_LENGTH));
            std::cerr << "read " << stack.top().hex() << "\n";
        }
    }

    for (i = 0; i < TEST_NEWLINE_COUNT; i++)
        std::cout << "\n";

    std::cout << stack.top().hex() << " " << stack.top().of_str() << "\n";
}
#endif
