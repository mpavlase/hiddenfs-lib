/**
 * @author Martin Pavlásek (xpavla07@stud.fit.vutbr.cz)
 *
 * Created on 24. březen 2012
 */
#include <sstream>
#include <string>

#include "common.h"

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