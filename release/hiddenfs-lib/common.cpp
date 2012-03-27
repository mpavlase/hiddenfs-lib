/**
 * @author Martin Pavlásek (xpavla07@stud.fit.vutbr.cz)
 *
 * Created on 24. březen 2012
 */

#include "common.h"
#include <sstream>
#include <string>

string print_vFile(vFile* f) {
    stringstream ss;
    ss << "inode: [" << f->inode << "], parent: [" << f->parent << "], fn: [" << f->filename << "], size: [" << f->size << "], flags: [" << f->flags << "]";

    return ss.str();
}

string print_vBlock(vBlock* b) {
    stringstream ss;
    ss << "(";
    if(b->used == BLOCK_IN_USE) {
        ss << "[U]";
    } else {
        ss << "[R]";
    }
    ss << ", " << b->hash << ", b: " << b->block << ", f#: " << b->fragment << ")";

    return ss.str();
}