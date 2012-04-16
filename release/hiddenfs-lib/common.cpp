/**
 * @author Martin Pavlásek (xpavla07@stud.fit.vutbr.cz)
 *
 * Created on 24. březen 2012
 */
#include <sstream>
#include <string>

#include "common.h"

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
            ss << "[U]";
        } else {
            ss << "[R]";
        }

        ss << ", " << b->hash << ", b: " << b->block << ", f#: " << b->fragment << ")";

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

    CRC::CRC_t CRC::update_crc(CRC_t crc, unsigned char *buf, int len) {
        CRC_t c = crc ^ 0xffffffffL;
        int n;

        for (n = 0; n < len; n++) {
            c = crc_table[(c ^ buf[n]) & 0xff] ^ (c >> 8);
        }

        return c ^ 0xffffffffL;
    }

    CRC::CRC_t CRC::calculate(unsigned char *buf, int len) {
        return this->update_crc(0L, buf, len);
    }
}