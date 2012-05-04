/**
 * @author Martin Pavlásek (xpavla07@stud.fit.vutbr.cz)
 *
 * Created on 17. duben 2012
 */

#include "superBlock.h"
#include "hiddenfs-lib.h"

namespace HiddenFS {
    void superBlock::add(bytestream_t* item, size_t itemLength) {
        assert(false); /// @deprecated
        assert(itemLength == RAW_ITEM_SIZE);

        bytestream_t* itemCopy = new bytestream_t[RAW_ITEM_SIZE];
        memcpy(itemCopy, item, RAW_ITEM_SIZE);
        this->foreignItems.push_back(itemCopy);
    }

    void superBlock::deserialize(bytestream_t* buffer, size_t size) {
        assert(buffer != NULL);
        assert(size <= hiddenFs::BLOCK_USABLE_LENGTH);

        size_t offset;
        bytestream_t emptyBuff[RAW_ITEM_SIZE];

        offset = 0;
        memset(emptyBuff, '\0', sizeof(emptyBuff));

        while(offset + RAW_ITEM_SIZE <= size) {
            // detekce 'zarážky'
            if(memcmp(emptyBuff, buffer + offset, RAW_ITEM_SIZE) == 0) {
                break;
            }

            this->readItem(buffer + offset, RAW_ITEM_SIZE);
            offset += RAW_ITEM_SIZE;
        }

        this->loaded = true;
    }

    void superBlock::readItem(bytestream_t* buffer, size_t bufferLength) {
        tableItem item;
        size_t offset = 0;
        vBlock* block;
        bytestream_t* buffOut;
        size_t buffOutLength;
        CRC::CRC_t checksum;

        assert(bufferLength == RAW_ITEM_SIZE);

        this->encryption->decrypt(buffer, bufferLength, &buffOut, &buffOutLength);

        // naplnění kontrolního součtu
        memcpy(&checksum, buffer + offset, sizeof(checksum));
        offset += sizeof(checksum);

        CRC c = CRC();
        checksum = c.calculate(buffOut + offset, buffOutLength - offset);

        if(checksum != checksum) {
            this->foreignItems.push_back(buffer);
            throw ExceptionRuntimeError("Blok neobsahuje korektní obsah.");
        }

        // naplnění typu tabulky
        memcpy(&(item.table), buffer + offset, sizeof(item.table));
        offset += sizeof(item.table);

        // naplnění umístění prvního bloku z řetězu
        unserialize_vBlock(buffer + offset, SIZEOF_vBlock, &block);
        memcpy(&(item.first), block, sizeof(vBlock));
        offset += sizeof(SIZEOF_vBlock);

        assert(offset == bufferLength);

        this->knownItems.push_back(item);
    }

    void superBlock::serialize(bytestream_t** bufferOut, size_t* size) {
        bytestream_t* blockBuff;
        bytestream_t* buffer;

        size_t offset;
        size_t offsetStartItem;
        size_t blockBuffLen;

        CRC c = CRC();
        CRC::CRC_t checksum;
        buffer = new bytestream_t[hiddenFs::BLOCK_USABLE_LENGTH];

        offset = 0;

        // Nejdříve uložit známé bloky
        for(table_known_t::iterator i = this->knownItems.begin(); i != this->knownItems.end(); i++) {
            offsetStartItem = offset;

            // vyhrazení místa pro kontrolní součet, který dopočítáme až nakonec
            offset += sizeof(checksum);


            // naplnění typu tabulky
            memcpy(buffer + offset, &(i->table), sizeof(i->table));
            offset += sizeof(i->table);

            // naplnění umístění prvního bloku z řetězu
            serialize_vBlock(i->first, &blockBuff, &blockBuffLen);
            memcpy(buffer + offset, blockBuff, blockBuffLen);
            assert(blockBuffLen == SIZEOF_vBlock);
            offset += blockBuffLen;

            checksum = c.calculate(buffer + offsetStartItem + sizeof(checksum), sizeof(i->table) + blockBuffLen);
            memcpy(buffer + offsetStartItem - sizeof(checksum), &checksum, sizeof(checksum));
        }

        for(table_t::iterator i = this->foreignItems.begin(); i != this->foreignItems.end(); i++) {
            std::cout << "sizeof foreign prvku: " << sizeof(*i) << std::endl;
            memcpy(buffer + offset, *i, sizeof(*i));
            offset += sizeof(*i);
        }

        assert(offset <= hiddenFs::BLOCK_USABLE_LENGTH);

        *bufferOut = buffer;
    }

    void superBlock::addKnownItem(std::set<vBlock*>& chain, table_flag tableType) {
        tableItem ti;

        for(std::set<vBlock*>::iterator i = chain.begin(); i != chain.end(); i++) {
            ti.table = tableType;
            ti.first = *i;
            this->knownItems.push_back(ti);
        }
    }

    void superBlock::readKnownItems(std::set<vBlock*>& chain, table_flag tableType) {
        for(table_known_t::iterator i = this->knownItems.begin(); i != knownItems.end(); i++) {
            if(i->table == tableType) {
                chain.insert(i->first);
            }
        }
    }
}
