#include "database.h"
#include <fstream>

namespace mysql
{

    /* 
     * ========================================= CRow
     * ========================================================
     */

    const CValue& CRow::operator[](const std::string& name) const
    {
        if (_table)
        {
            const CColumnDesc* colDesc = _table->find_column(name);
            if (colDesc)
            {
                try
                {
                    return _data.at(colDesc->get_position());
                }
                catch (...)
                {
                    ;
                }
            }
        }
        return _null_value;
    }

    int CTable::tune(uint8_t* data, size_t size, const CFormatDescriptionLogEvent& fmt)
    {
        if (_tuned) return 0;

        int rc;
        if ((rc = CTableMapLogEvent::tune(data, size, fmt)) == 0 && CTableMapLogEvent::is_valid())
        {
            _row.set_table(this);
            _row.resize(_column_count);
            uint8_t* type = _column_types;
            for (unsigned int i = 0; i < _column_count; ++i)
            {
                _row[i].pos(i);
                _row[i]._type = (CValue::EColumnType) (* type);
                type++;
            }
            
            _tuned = true;
        }

        return rc;
    }

    int CTable::update(CRowLogEvent& rlev)
    {
        const uint8_t* pfields;
        size_t len;
        CMemBlock nullfileds_bitmap;

        if (!rlev.is_valid() || _table_id != rlev._table_id)
            return -1;

        pfields = rlev.rows_data();
        len = rlev.rows_len();

        _new_rows.clear();
        _rows.clear();
        while (len > 0)
        {
            uint32_t used_columns_1bit_count = rlev.used_columns_bitmap()->bit1_count(rlev._ncolumns);
            uint32_t null_bitmap_bytes_size = (used_columns_1bit_count + 7) / 8;
            nullfileds_bitmap.assign(pfields,null_bitmap_bytes_size);//Bit-field indicating whether each field in the row is NULL. 
            pfields+=null_bitmap_bytes_size;
            update_row(_row, &pfields, &len, rlev._ncolumns, rlev.used_columns_bitmap(), &nullfileds_bitmap);

            _rows.push_back(_row);
            if (!len) break;

            if (rlev.get_type_code() == UPDATE_ROWS_EVENT)
            {
                uint32_t used_columns_ai_1bit_count = rlev.used_columns_afterimage_bitmap()->bit1_count(rlev._ncolumns);
                uint32_t ai_null_bitmap_bytes_size = (used_columns_ai_1bit_count + 7) / 8;
                nullfileds_bitmap.assign(pfields, ai_null_bitmap_bytes_size);
                pfields+=ai_null_bitmap_bytes_size;
                update_row(_row, &pfields, &len, rlev._ncolumns, rlev.used_columns_afterimage_bitmap(), &nullfileds_bitmap);
                _new_rows.push_back(_row);
            }
        }

        return len == 0 ? 0 : -1;
    }

    int CTable::update_row(CRow& row, const uint8_t** pdata, size_t* len, uint64_t ncolumns,CMemBlock *usedcolumns_bitmap, CMemBlock *nullfileds_bitmap)
    {
        CValue::EColumnType type;
        uint32_t metadata;
        uint32_t length;
        uint8_t* pmetadata;

        ssize_t size = *len;
        const uint8_t* data = *pdata;
        pmetadata = _metadata;

        for (uint64_t i = 0; i < ncolumns; ++i)
        {
            if (size <= 0)
            {
                row[i].reset();
                continue;
            } // NOTE: achtung; exception is here

            type = row[i]._type;//(CValue::EColumnType)*(_column_types + i);
            switch (CValue::calc_metadata_size(type))
            {
                case 0:
                {
                    metadata = 0;
                    break;
                }
                case 1:
                {
                    metadata = *pmetadata++;
                    break;
                }
                case 2:
                {
                    metadata = *(uint16_t*) pmetadata;
                    pmetadata += 2;
                    break;
                }
                default:
                    metadata = 0;
            }

            row[i].reset();
            if (usedcolumns_bitmap->bit_at(i))
            {
                if (!nullfileds_bitmap->bit_at(i))
                {
                    length = CValue::calc_field_size(type, data, metadata);
                    row[i].tune(type, data, metadata, length);
                    data += length;
                    size -= length;
                }
            }
        }
        *pdata = data;
        *len = size < 0 ? 0 : size;

        return size >= 0 ? 0 : -1;
    }

    void CTable::add_column(const std::string& column_name, int column_pos, const std::string& column_type)
    {
        _data[column_name].update(column_pos, column_type);
    }

}

