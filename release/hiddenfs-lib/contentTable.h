/**
 * @author Martin Pavlásek (xpavla07@stud.fit.vutbr.cz)
 *
 * Created on 14. březen 2012
 */

#ifndef CONTENTTABLE_H
#define	CONTENTTABLE_H

#include <map>
#include <set>
#include <vector>

#include "common.h"
//#include "hiddenfs-lib.h"
#include "types.h"
#include "exceptions.h"
#include "structTable.h"

namespace HiddenFS {
    class contentTable {
    public:
        typedef struct {
            /** Množina rezervovaných bloků (nad rámec bloků ve složce "content"
             * této struktury) je pouze pomocným seznamem, resp. jiným
             * pohledem na tabulku content */
            std::set<vBlock*> reserved;

            /** součet délek všech rezervovaných bloků, lze odvodit z průchodu
             * složkou struktury "content" */
            size_t reservedBytes;

            /** množina obsazených bloků \
             * mapování: číslo fragmentu -> pole bloků,
             * které uchovávají obsah fragmentu */
            std::map<fragment_t, std::vector<vBlock*> > content;
        } tableItem;

        /** datový typ pro mapování pomocných dat k souboru určeným inode */
        typedef std::map<inode_t, tableItem> table_t;

        contentTable();
        //contentTable(const contentTable& orig);
        virtual ~contentTable();

        /**
         * Provede dump hlavní tabulky do podoby seznamu entit jednotného formátu
         * @param ouput metoda naplní seznam entitami pro pozdější uložení
         */
        void serialize(chainList_t* output);

        /**
         * Provádí rekonstrukci seznamu entit a rovnou plní obsah hlavní i všech
         * přidružených pomocných tabulek
         * @param input vstupní seznam bloků
         */
        void deserialize(const chainList_t& input);

        /**
         * user-friendly obsah tabulky
         */
        void print();

        /*
         * Find and iterate through all fragments
         * @param inode file identified by inode
         * @param buffer content of file
         */
        /**
         * Vrací metadata obsahu k souboru (množina fragmentů, rezervované bloky atd.)
         * @param inode inode souboru
         * @param content metadata obsahu souboru
         */
        void getMetadata(inode_t inode, tableItem& content);

        /**
         * Inicializuje vnitřní struktury pro ukložení fragmentů
         * Je nutné volat ještě před jakoukoli manipulací s uskladněným inode
         * @param inode inode souboru
         */
        void newEmptyContent(inode_t inode);

        /**
         * Odstraní inode z tabulky. Metoda volající tuto zodpovídá za uvolnění
         * naalokovaných bloků.
         * @param inode identifikace souboru, který chceme odstranit
         */
        void removeFile(inode_t inode);

        /**
         * Přiřadí blok k inode (sama uvnitř volá setBlockAsUsed)
         * @param inode identifikace souboru, kterému rozšiřujeme obsah
         * @param block
         */
        void addContent(inode_t inode, vBlock* block) {
            if(block->used) {
                if(this->table.find(inode) == this->table.end()) {
                    throw ExceptionFileNotFound(inode);
                }

                try {
                    this->newEmptyContent(inode);
                } catch (ExceptionInodeExists) { }

                // zde to padá, protože přistupuju do nezinicializvaného pole!
                // špatně deserializovaná složka .block v vBlock
                this->table[inode].content[block->fragment].push_back(block);
                this->setBlockAsUsed(inode, block);
            } else {
                // následující dva řádky obstarává volání setBlockAsReserved()
                //this->table[inode].reserved.insert(block);
                //this->table[inode].reservedBytes += block->length;
                this->setBlockAsReserved(inode, block);
            }
        }

        bool isInodePresent(inode_t inode) {
            return (this->table.find(inode) != this->table.end());
        }

        /**
         * Vyhledá veškeré bloky obsazené, či rezervované k souboru
         * @param inode identifikace souboru
         * @param block metoda naplní tento seznam bloky, které byly přiřazeny
         * k tomuto souboru a měly by být volající metodou fyzicky uvolněn.
         * @throw ExceptionFileNotFound pokud inode v tabulce neexistuje
         */
        void findAllBlocks(inode_t inode, std::set<vBlock*>& blocks) {
            if(this->table.find(inode) == this->table.end()) {
                throw ExceptionFileNotFound(inode);
            }

            // zkopírování rezervovaných bloků
            this->getReservedBlocks(inode, blocks);
            //blocks.insert(this->table[inode].reserved.begin(), this->table[inode].reserved.end());

            // zkopírování bloků obsahu
            this->findUsedBlocks(inode, blocks);
        }

        /**
         * Nalezne všechny obsazené bloky, které tento soubor obsadil
         * @param inode identifikace souboru, jehož obsah chceme nalézt
         * @param blocks seznam všech obsazených bloků
         * @throw ExceptionFileNotFound pokud inode v tabulce neexistuje
         */
        void findUsedBlocks(inode_t inode, std::set<vBlock*>& blocks) {
            if(this->table.find(inode) == this->table.end()) {
                throw ExceptionFileNotFound(inode);
            }

            // zkopírování bloků obsahu
            std::map<fragment_t, std::vector<vBlock*> >::iterator i;
            for(i = this->table[inode].content.begin(); i != this->table[inode].content.end(); i++) {
                blocks.insert(i->second.begin(), i->second.end());
            }
        }

        /**
         * Naplní parametr reserved seznamem bloků, které má soubor určený inode zarezervovány
         * @param inode identifikace souboru, jehož obsah chceme nalézt
         * @param reserved seznam všech rezervovaných bloků
         * @throw ExceptionFileNotFound pokud inode v tabulce neexistuje
         */
        void getReservedBlocks(inode_t inode, std::set<vBlock*>& reserved) {
            if(this->table.find(inode) == this->table.end()) {
                throw ExceptionFileNotFound(inode);
            }

            reserved.insert(this->table[inode].reserved.begin(), this->table[inode].reserved.end());
        }

        /**
         * Vyhledá a vrátí jakýkoli (takže první dostupný) zarezervovaný blok
         * bez ohledu na inode, kterému soubor patřil. Metoda neruší příslušnost
         * bloku k souboru, to už musí provést metoda volající tuto.
         * @param inode soubor, kterému dotyčný blok původně patřil
         * @param block umístění volného bloku
         * @throw ExceptionDiscFull pokud se v systému nevyskytují žádné rezervované bloky
         */
        void findAnyReservedBlock(inode_t& inode, vBlock*& block);

        /**
         * Zruší bloku příznak 'reserved' a odstraní jej z patřičných pomocných seznamů
         * @param inode identifikace souboru, kterému tento blok patří
         * @param block operovaný blok
         */
        void setBlockAsUsed(inode_t inode, vBlock* block) {
            block->used = true;

            assert(this->table.find(inode) != this->table.end());

            // Byl blok mezi rezervovanými?
            if(this->table[inode].reserved.find(block) != this->table[inode].reserved.end()) {
                // ano byl, proto jej odstraníme ze skupiny rezervovaných
                this->table[inode].reservedBytes -= block->length;
                this->table[inode].reserved.erase(block);
            }
        }

        /**
         * Nastaví bloku příznak 'reserved' a zařadí jej do patřičných pomocných seznamů
         * @param inode identifikace souboru, kterému tento blok patří
         * @param block operovaný blok
         */
        void setBlockAsReserved(inode_t inode, vBlock* block) {
            block->used = false;

            assert(this->table.find(inode) != this->table.end());

            // Je blok mezi rezervovanými?
            if(this->table[inode].reserved.find(block) == this->table[inode].reserved.end()) {
                // není, proto jej mezi ně zařadíme
                this->table[inode].reservedBytes += block->length;
                this->table[inode].reserved.insert(block);
            }
        }

        /**
         * Odstraní blok z pomocného seznamu uchovávající rezerovované bloky,
         * takže se jeví jako neobsazený
         * @param inode identifikace souboru, kterému tento blok patří
         * @param block operovaný blok
         */
        void setBlockAsUnused(inode_t inode, vBlock* block) {
            /* Principielně tato metoda provádí to samé, co setBlockAsUsed():
             *  odstraní blok ze seznamu rezervovaných a sníží sumu ve složce reservedBytes */
            this->setBlockAsUsed(inode, block);

            block->used = false;
        }
    private:
        /** map[inode_t] = tableItem */
        table_t table;
    };
}

#endif	/* CONTENTTABLE_H */

