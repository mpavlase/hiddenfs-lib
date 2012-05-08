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
        /*assert(itemLength == RAW_ITEM_SIZE);

        bytestream_t* itemCopy = new bytestream_t[RAW_ITEM_SIZE];
        memcpy(itemCopy, item, RAW_ITEM_SIZE);
        this->foreignItems.push_back(itemCopy);
        */
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
        size_t offset;
        vBlock* block;
        bytestream_t* buffOut;
        size_t buffOutLength;
        CRC::CRC_t checksumComputed;
        CRC::CRC_t checksumRead;

        assert(bufferLength == RAW_ITEM_SIZE);

        this->encryption->decrypt(buffer, bufferLength, &buffOut, &buffOutLength);

        assert(bufferLength == RAW_ITEM_SIZE);

        // naplnění kontrolního součtu
        offset = 0;
        memcpy(&checksumRead, buffOut + offset, sizeof(checksumRead));
        offset += sizeof(checksumRead);

        CRC c = CRC();
        checksumComputed = c.calculate(buffOut + offset, buffOutLength - offset);

        if(checksumRead == checksumComputed) {
            // naplnění typu tabulky
            memcpy(&(item.table), buffOut + offset, sizeof(item.table));
            offset += sizeof(item.table);

            // naplnění umístění prvního bloku z řetězu
            unserialize_vBlock(buffOut + offset, SIZEOF_vBlock, &block);
            item.first = new vBlock;
            memcpy(item.first, block, sizeof(vBlock));
            offset += SIZEOF_vBlock;

            std::cout << "superBlock::readItem table=" << (char) item.table << ", first=" << print_vBlock(item.first) << std::endl;

            this->knownItems.push_back(item);
        } else {
            tableForeign tfi;

            tfi.length = bufferLength;

            assert(tfi.length == RAW_ITEM_SIZE);

            tfi.content = new bytestream_t[tfi.length];
            memcpy(tfi.content, buffer, tfi.length);
            offset = tfi.length;
            this->foreignItems.push_back(tfi);
        }

        assert(offset == bufferLength);
    }

    void superBlock::serialize(bytestream_t** bufferOut, size_t* size) {
        bytestream_t* blockBuff;
        bytestream_t* buffer;
        bytestream_t* bufferToEncode;
        bytestream_t* bufferEncoded;

        size_t offset;
        size_t offsetToEncode;
        size_t bufferEncodedLength;
        size_t blockBuffLen;

        CRC c = CRC();
        CRC::CRC_t checksum;
        buffer = new bytestream_t[hiddenFs::BLOCK_USABLE_LENGTH];
        bufferToEncode = new bytestream_t[hiddenFs::BLOCK_USABLE_LENGTH];

        offset = 0;

        // Nejdříve uložit známé bloky
        for(table_known_t::iterator i = this->knownItems.begin(); i != this->knownItems.end(); i++) {
            memset(bufferToEncode, '\0', sizeof(bufferToEncode));
            offsetToEncode = 0;

            //offsetStartItem = offset;

            // vyhrazení místa pro kontrolní součet, který dopočítáme až nakonec
            offsetToEncode += sizeof(checksum);

            // naplnění typu tabulky
            memcpy(bufferToEncode + offsetToEncode, &(i->table), sizeof(i->table));
            offsetToEncode += sizeof(i->table);

            // naplnění umístění prvního bloku z řetězu
            serialize_vBlock(i->first, &blockBuff, &blockBuffLen);
            memcpy(bufferToEncode + offsetToEncode, blockBuff, blockBuffLen);
            assert(blockBuffLen == SIZEOF_vBlock);
            offsetToEncode += blockBuffLen;

            // zápis kontrolního součtu
            checksum = c.calculate(bufferToEncode + sizeof(checksum), offsetToEncode - sizeof(checksum));
            memcpy(bufferToEncode, &checksum, sizeof(checksum));
            //pBytes(buffer, offset);

            // nakonec zašifrujeme obsah
            this->encryption->encrypt(bufferToEncode, offsetToEncode, &bufferEncoded, &bufferEncodedLength);
            assert(bufferEncodedLength == RAW_ITEM_SIZE);
            memcpy(buffer + offset, bufferEncoded, bufferEncodedLength);
            offset += bufferEncodedLength;

            // obsah je už zkopírovaný, lze proto uvolnit naalokované pole bytů
            delete [] bufferEncoded;
            delete [] blockBuff;
        }

        delete [] bufferToEncode;

        for(table_foreign_t::iterator i = this->foreignItems.begin(); i != this->foreignItems.end(); i++) {
            std::cout << "sizeof foreign prvku: " << i->length << std::endl;
            memcpy(buffer + offset, i->content, i->length);
            offset += i->length;
        }

        assert(offset <= hiddenFs::BLOCK_USABLE_LENGTH);

        *bufferOut = buffer;
        *size = offset;
    }

    void superBlock::addKnownItem(std::set<vBlock*>& chain, table_flag tableType) {
        tableItem ti;
        std::cout << "superBlock::addKnownItem chain(z parametru).size() = " << chain.size() << std::endl;

        for(std::set<vBlock*>::iterator i = chain.begin(); i != chain.end(); i++) {
            ti.table = tableType;
            ti.first = *i;

            std::cout << "  - table=" << (char) ti.table << ", first=" << print_vBlock(ti.first) << std::endl;

            this->knownItems.push_back(ti);
        }
    }

    void superBlock::readKnownItems(std::set<vBlock*>& chain, table_flag tableType) {
        std::cout << "superBlock::readKnownItems this->knownItems.size() = " << this->knownItems.size() << std::endl;

        for(table_known_t::iterator i = this->knownItems.begin(); i != knownItems.end(); i++) {
            std::cout << "superBlock::readKnownItems table = '" << (char) (i->table) << "', " << print_vBlock(i->first) << std::endl;
            if(i->table == tableType) {
                chain.insert(i->first);
            }
        }
    }
}
