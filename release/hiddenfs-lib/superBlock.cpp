/**
 * @author Martin Pavlásek (xpavla07@stud.fit.vutbr.cz)
 *
 * Created on 17. duben 2012
 */

#include "superBlock.h"

namespace HiddenFS {
    void superBlock::deserialize(bytestream_t* buffer, size_t size) {
        //const size_t rawItemSize = sizeof(CRC::CRC_t) + sizeof(__u_char) + sizeof(SIZEOF_vBlock);
        size_t offset = 0;
        tableItem item;
        //vBlock* itemBlock;

        assert(buffer != NULL);

        while(offset + this->rawItemSize <= size) {
            memset(&item, '\0', sizeof(item));

            /*
            // naplnění kontrolního součtu
            memcpy(&(item.checksum), buffer + offset, sizeof(item.checksum));
            offset += sizeof(item.checksum);

            // naplnění typu tabulky
            memcpy(&(item.table), buffer + offset, sizeof(item.table));
            offset += sizeof(item.table);

            // naplnění umístění prvního bloku z řetězu
            unserialize_vBlock(&itemBlock, buffer + offset, SIZEOF_vBlock);
            memcpy(&(item.first), itemBlock, sizeof(vBlock));
            offset += sizeof(SIZEOF_vBlock);
            */
        }

        this->loaded = true;
    }

    /*
    void superBlock::deserializeItem(bytestream_t* buffer) {

    }
    */

    void superBlock::serialize(bytestream_t** buffer, size_t* size) {

    }
}
