/**
 * @author Martin Pavlásek (xpavla07@stud.fit.vutbr.cz)
 *
 * Created on 24. březen 2012
 */
#include <math.h>
#include <sstream>
#include <string>
#include <iomanip>

#include "common.h"
//#include "hiddenfs-lib.h"

namespace HiddenFS {

    std::string print_vFile(vFile* f) {
        std::stringstream ss;

        ss << "inode: [" << f->inode << "], parent: [" << f->parent << "], fn: [" << f->filename << "], size: [" << f->size << "], flags: [";

        if(f->flags & vFile::FLAG_DIR) {
            ss << "D";
        }

        if(f->flags & vFile::FLAG_READ) {
            ss << "R";
        }

        if(f->flags & vFile::FLAG_WRITE) {
            ss << "W";
        }

        if(f->flags & vFile::FLAG_EXECUTE) {
            ss << "X";
        }

        ss<< "]";

        return ss.str();
    }

    std::string print_vBlock(vBlock* b) {
        std::stringstream ss;
        ss << "(";

        if(b->used == BLOCK_IN_USE) {
            ss << "[  Used  ]";
        } else {
            ss << "[Reserved]";
        }

        ss << ", ";// << b->hash;
        ss << b->hash;
        ss << ", b: ";
        ss << b->block;
        ss << ", f#: " << b->fragment;
        ss << ", size: " << b->length << ")";

        return ss.str();
    }

    CRC::CRC() {
        this->make_crc_table();
    }

    void CRC::make_crc_table(void) {
        unsigned long c;
        int n, k;

        for (n = 0; n < 256; n++) {
            c = (unsigned long) n;

            for (k = 0; k < 8; k++) {
                if (c & 1) {
                    c = 0xedb88320L ^ (c >> 1);
                } else {
                    c = c >> 1;
                }
            }

            crc_table[n] = c;
        }
    }

    CRC::CRC_t CRC::update_crc(CRC_t crc, __u_char *buf, int len) {
        CRC_t c = crc ^ 0xffffffffL;
        int n;

        for (n = 0; n < len; n++) {
            c = crc_table[(c ^ buf[n]) & 0xff] ^ (c >> 8);
        }

        return c ^ 0xffffffffL;
    }

    CRC::CRC_t CRC::calculate(__u_char *buf, int len) {
        return this->update_crc(0L, buf, len);
    }

    bool idByteIsDataBlock(id_byte_t b) {
        return (0 < b && b < ID_BYTE_BOUNDARIES[0]);
    };

    bool idByteIsSuperBlock(id_byte_t b) {
        return (ID_BYTE_BOUNDARIES[0] < b && b < ID_BYTE_BOUNDARIES[1]);
    };

    bool idByteIsChain(id_byte_t b) {
        return (ID_BYTE_BOUNDARIES[1] < b && b < idByteMaxValue());
    }

    const id_byte_t idByteMaxValue() {
        /* Protože je datový typ id_byte_t bezznaménkový, lze pro nejvyšší
         * dosažitelnou hodnotu jednoduše využít podtečení */
        return -1;
    };

    id_byte_t idByteGenDataBlock() {
        // interval (0; ID_BYTE_BOUNDARIES[0])

        return (rand() % (ID_BYTE_BOUNDARIES[0] - 1) ) + 1;
    }

    id_byte_t idByteGenSuperBlock() {
        // interval (ID_BYTE_BOUNDARIES[0]; ID_BYTE_BOUNDARIES[1])

        return ID_BYTE_BOUNDARIES[0] + (rand() % (ID_BYTE_BOUNDARIES[1] - ID_BYTE_BOUNDARIES[0] - 1)) + 1;
    }

    id_byte_t idByteGenChain() {
        // interval (ID_BYTE_BOUNDARIES[1]; idByteMaxValue())

        return ID_BYTE_BOUNDARIES[1] + (rand() % (idByteMaxValue() - ID_BYTE_BOUNDARIES[1] - 1)) + 1;
    }

    void pBytes(bytestream_t* input, size_t len) {
        std::cout << "\tprintBytes: len=" << len << "\nHEX --->";

        for(unsigned int i = 0; i < len; i++) {
            std::cout.width(2);
            std::cout << std::setfill('0') << std::hex << std::uppercase << (int)input[i];
            if(i%2 == 1) {
                std::cout << " ";
            }
        }
        std::cout << std::dec << "\nASCII --->";
        std::cout.write((const char*)input, len);
        std::cout << "\n" << std::flush;
    }

    void serialize_vBlock(vBlock* input, bytestream_t** output, size_t* size) {
        size_t rawItemSize = SIZEOF_vBlock;

        assert(input != NULL);
        assert(size != NULL);

        size_t offset = 0;
        *output = new bytestream_t[rawItemSize];
        //pBytes(*output, rawItemSize);
        //memset(*output, '\0', rawItemSize);

        // kopírování složky 'block'
        memcpy(*output + offset, &(input->block), sizeof(input->block));
        offset += sizeof(input->block);

        // kopírování složky 'fragment'
        memcpy(*output + offset, &(input->fragment), sizeof(input->fragment));
        offset += sizeof(input->fragment);

        // kopírování složky 'hash'
        convertAsciiToHash(*output + offset, input->hash);
        //memcpy(*output + offset, input->hash.c_str(), hash_raw_t_sizeof);
        offset += hash_raw_t_sizeof;

        // kopírování složky 'length'
        memcpy(*output + offset, &(input->length), sizeof(input->length));
        offset += sizeof(input->length);

        // kopírování složky 'used'
        memcpy(*output + offset, &(input->used), sizeof(input->used));
        offset += sizeof(input->used);

        *size = offset;
    };

    void unserialize_vBlock(bytestream_t* input, size_t size, vBlock** output) {
        size_t rawItemSize = SIZEOF_vBlock;

        assert(output != NULL);
        assert(input != NULL);

        *output = new vBlock;
        size_t offset = 0;

        assert(size == rawItemSize);

        // kopírování složky 'block'
        memcpy(&((*output)->block), input + offset, sizeof((*output)->block));
        offset += sizeof((*output)->block);

        // kopírování složky 'fragment'
        memcpy(&((*output)->fragment), input + offset, sizeof((*output)->fragment));
        offset += sizeof((*output)->fragment);

        // kopírování složky 'hash'
        (*output)->hash.clear();
        convertHashToAscii((*output)->hash, (const hash_raw_t*)input + offset, hash_raw_t_sizeof);
        //(*output)->hash.assign((const char*)input + offset, 0, hash_raw_t_sizeof);
        offset += hash_raw_t_sizeof;

        // kopírování složky 'length'
        memcpy(&((*output)->length), input + offset, sizeof((*output)->length));
        offset += sizeof((*output)->length);

        // kopírování složky 'used'
        memcpy(&((*output)->used), input + offset, sizeof((*output)->used));
        offset += sizeof((*output)->used);
    };

    void convertHashToAscii(hash_ascii_t& output, const hash_raw_t* input, size_t inputLength) {
        CryptoPP::HexEncoder encoder;
        encoder.Put((const byte*) input, inputLength);

        size_t size = encoder.MaxRetrievable();
        assert(size > 0);
        output.resize(size);
        encoder.Get((byte*)output.data(), output.size());
    }

    void convertAsciiToHash(hash_raw_t* output, hash_ascii_t& input) {
        CryptoPP::HexDecoder decoder;

        decoder.Put((byte*)input.data(), input.size());
        decoder.MessageEnd();

        size_t size = decoder.MaxRetrievable();
        if(size) {
            assert(size == hash_raw_t_sizeof);
            decoder.Get((byte*)output, size);
        }
    }
}