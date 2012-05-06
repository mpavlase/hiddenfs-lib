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

        typedef __u_char table_flag;

        static const table_flag TABLE_STRUCT_TABLE = 's';
        static const table_flag TABLE_CONTENT_TABLE = 'c';

        ~superBlock() {
            for(table_foreign_t::iterator i = this->foreignItems.begin(); i != this->foreignItems.end(); i++) {
                ///@todo proč to způsobuje segfault?
                //delete [] (i->content);
            }

            for(table_known_t::iterator i = this->knownItems.begin(); i != this->knownItems.end(); i++) {
                delete i->first;
            }
        }

        struct tableItem {
            table_flag table;
            vBlock* first;
        };

        struct tableForeign {
            size_t length;
            bytestream_t* content;
        };

        static const size_t RAW_ITEM_SIZE = sizeof(CRC::CRC_t) + sizeof(table_flag) + SIZEOF_vBlock;

        typedef std::vector<tableForeign> table_foreign_t;
        typedef std::vector<tableItem> table_known_t;

        /**
         * Přidá jeden záznam typu SuperBlock, ale v tuto chvíli se ještě nezabýváme obsahem.
         * Při vkládání se provádí hluboká kopie bufferu.
         * @param item vstupní buffer s blokem dat, který by mohl obsahovat záznam superbloku
         * @param itemLength délka vstupního bufferu
         * @deprecated
         */
        void add(bytestream_t* item, size_t itemLength);

        /**
         * Přidá do vnitřního seznamu položky (tableItem) patřící tomuto souborovému systému
         * @param chain seznam prvních článků řetězu, tzn. počátky kopií obsahu stejné tabulk
         * @param tableType určuje typ tabulky, které jsou
         */
        void addKnownItem(std::set<vBlock*>& chain, table_flag tableType);

        /**
         * Naplní seznam 'chain' ukazateli na první bloky řetězů, 'tableType' slouží
         * jako filtr
         * @param chain metoda naplní tento seznam
         * @param tableType filtr, podle kterého budou do 'chain' vloženy ukazatele
         */
        void readKnownItems(std::set<vBlock*>& chain, table_flag tableType);

        void clearKnownItems() {
            this->knownItems.clear();
        }

        /**
         * Roztřídí obsah do patřičných seznamů: validní položky do knownItems,
         * vše ostatní do foreignItems
         * @param buffer vstup dat pro kontrolu
         * @param bufferLength délka vstupního bufferu
         */
        void readItem(bytestream_t* buffer, size_t bufferLength);

        /**
         * Převede obsah superbloku na binární blok dat
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
         * Skrze parametr vrací pouze validní položky superbloku (tj. pouze ty,
         * které se podařilo rozšifrovat)
         * @param items vektor s nalezenými položkami
         */
        void getValidItems(table_foreign_t* items);

        void setEncryptionInstance(IEncryption* inst) {
            this->encryption = inst;
        }

        //void setTables(structTable* ST, contentTable* CT);

        bool isLoaded() {
            return this->loaded;
        }

        /**
         * Používat s rozvahou!
         */
        void setLoaded() {
            this->loaded = true;
        }
    private:
        IEncryption* encryption;
        bool loaded;

        /** seznam všech bloků dat, které jsou položkami superbloku jiného souborového systému */
        table_foreign_t foreignItems;

        /** seznam položek superbloku, který patří tomuto souborovému systému */
        table_known_t knownItems;
    };
}

#endif	/* SUPERBLOCK_H */

