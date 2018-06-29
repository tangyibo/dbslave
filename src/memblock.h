#ifndef MEMBLOCK_H
#define MEMBLOCK_H

#include <stdint.h>
#include <string.h>
#include <assert.h>

namespace mysql {
    
class CMemBlock
{
    public:
        CMemBlock();
        ~CMemBlock();

        int assign(const uint8_t *buf, uint32_t size);
        
        /*
         * 按字节取值
         */
        uint8_t byte_at(uint32_t index);
        
        /*
         * 按比特位取值
         * 备注：可用于判断某个bit位是否为 1
         */
        bool bit_at(uint32_t pos);
        
        /*
         * 获取 0~_size 范围内 bit位为 1 的位数
         */
        uint32_t bit1_count();
        
        /*
         * 获取 0~bit_set_size 范围内 bit位为 1 的位数
        */
        uint32_t bit1_count(uint32_t bit_set_size);

    private:
        uint32_t    _size;
        uint8_t     *_block;
};

}

#endif
