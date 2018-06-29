#include "memblock.h"

namespace mysql {

CMemBlock::CMemBlock()
:_size(0)
,_block(NULL)
{
}

CMemBlock::~CMemBlock()
{
    if (_block != NULL)
    {
        delete _block;
        
        _block = NULL;
        _size = 0;
    }
}

int CMemBlock::assign(const uint8_t *buf, uint32_t size)
{
    if (_block != NULL)
    {
        delete _block;
        
        _block = NULL;
        _size = 0;
    }

    _block = new uint8_t[size];
    if (_block == NULL)
        return -1;

    memcpy(_block, buf, size);
    _size = size;

    return 0;
}

uint8_t CMemBlock::byte_at(uint32_t index)
{
    assert(index < _size);
    return *(_block + index);
}

bool CMemBlock::bit_at(uint32_t pos)
{
    uint32_t mchar = pos / 8;     //字节序号
    uint32_t nbit = pos & 0x07;  //字节中的bit序号
    assert(mchar < _size);
    
    return ((_block[mchar] >> nbit) & 0x01) == 0x01 ? true : false;
}

uint32_t CMemBlock::bit1_count()
{
    uint32_t count = 0;
    
    for (uint32_t i = 0; i < _size; i++)
    {
        uint8_t p = *(_block + i);
        while (p != 0)
        {
            if ((p & 0x01) != 0)
                count++;
            p = p >> 1;
        }
    }

    return count;
}

uint32_t CMemBlock::bit1_count(uint32_t bit_set_size)
{
    uint32_t count = 0;
    assert(bit_set_size <= _size * 8);
    
    for (uint32_t i = 0; i < bit_set_size; i++)
    {
        if (bit_at(i))
            count++;
    }

    return count;
}

}