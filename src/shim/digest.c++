#include "digest.h++"

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Digest::Digest(std::string hex, int bit_length)
{
    this->_bit_length = bit_length;
    this->_hex = hex;

    this->_bytes = new unsigned char[bit_length / 8];
    {
        char *raw_bytes, *raw_hex;
        raw_bytes = (char *)this->_bytes;
        raw_hex = (char *)this->_hex.c_str();

        for (int i = 0; i < (bit_length / 8); i++) {
            unsigned int val;

            val = (*raw_hex) - '0';
            val <<= 8;
            raw_hex++;

            val += (*raw_hex) - '0';
            raw_hex++;

            *raw_bytes = val;
            raw_bytes++;
        }
    }
}

Digest::~Digest(void)
{
    delete[] this->_bytes;
}

int Digest::byte_length(void) const
{
    return this->_bit_length / 8;
}

const unsigned char *Digest::bytes(void) const
{
    return this->_bytes;
}