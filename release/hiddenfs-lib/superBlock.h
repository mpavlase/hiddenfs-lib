/**
 * @author Martin Pavlásek (xpavla07@stud.fit.vutbr.cz)
 *
 * Created on 17. duben 2012
 */

#ifndef SUPERBLOCK_H
#define	SUPERBLOCK_H

#include <vector>

#include "common.h"
#include "contentTable.h"
#include "structTable.h"

namespace HiddenFS {
    class superBlock {
    public:
        superBlock() {
            this->encryption = NULL;
            this->loaded = false;
        };
        superBlock(const superBlock& orig);

        static const size_t rawItemSize = sizeof(CRC::CRC_t) + sizeof(__u_char) + sizeof(SIZEOF_vBlock);

        struct tableItem {
            CRC::CRC_t checksum;
            __u_char table;
            vBlock first;
        };

        typedef std::vector<bytestream_t[rawItemSize]> table_t;

        /**
         * Převede obsah superbloku na blok dat
         * @param buffer výstupní buffer pro serializovaná data, metoda
         * sama alokuje dostatečnou kapacitu ukazatele
         * @param size délka bufferu
         */
        void serialize(bytestream_t** buffer, size_t* size);

        /**
         * Provádí parsování vstupního bufferu a naplní interní tabulku všemi záznamy
         * (i těmi, které nepatří této instanci souborového systému)
         * @param buffer blok dat, který je třeba rozparsovat
         * @param size délka bufferu
         */
        void deserialize(bytestream_t* buffer, size_t size);

        /**
         *
         * @param buffer
         * @param output
         */
        //void deserializeItem(bytestream_t* buffer, tableItem* output);

        /**
         * Skrze parametr vrací pouze validní položky superbloku (tj. pouze ty,
         * které se podařilo rozšifrovat)
         * @param items vektor s nalezenými položkami
         */
        void getValidItems(table_t* items);

        void setEncryptionInstance(IEncryption* inst) {
            this->encryption = inst;
        }

        //void setTables(structTable* ST, contentTable* CT);

        bool isLoaded() {
            return this->loaded;
        }
    private:
        IEncryption* encryption;
        bool loaded;
    };
}

#endif	/* SUPERBLOCK_H */

