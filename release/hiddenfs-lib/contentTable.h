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
         * Provede dump hlavní tabulky do podoby řetězce
         * @param ouput metoda naplní seznam entitami pro pozdější uložení
         */
        void serialize(chainList_t* output);

        void deserialize(chainList_t input);

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
         * Přiřadí blok k inode (sama uvnitř volá setBlockAsUsed)
         * @param inode identifikace souboru, kterému rozšiřujeme obsah
         * @param block
         */
        void addContent(inode_t inode, vBlock* block) {
            if(block->used) {
                try {
                    this->newEmptyContent(inode);
                } catch (ExceptionInodeExists) { }

                // zde to padá, protože přistupuju do nezinicializvaného pole!
                // špatně deserializovaná složka .block v vBlock
                //this->table[inode].content[block->fragment].push_back(block);
                //tableItem ti = this->table[inode];
                //std::cout << "ti.content.size() = " << this->table[inode].content.size() << "\n";
                this->table[inode].content[block->fragment].push_back(block);
                this->setBlockAsUsed(inode, block);
                /*
                std::cout << "ti.content.size() = " << this->table[inode].content.size() << "\n";
                for(std::map<fragment_t, std::vector<vBlock*> >::iterator i = this->table[inode].content.begin(); i != this->table[inode].content.end(); i++) {
                    std::cout << "fragment = " << i->first << ", ";
                    for(std::vector<vBlock*>::iterator j = i->second.begin(); j != i->second.end(); j++) {
                        std::cout << print_vBlock(*j);
                    }
                    std::cout << "\n";
                }
                std::cout << "............................\n";
                this->print();
                */
            } else {
                // následující dva řádky obstarává volání setBlockAsReserved()
                //this->table[inode].reserved.insert(block);
                //this->table[inode].reservedBytes += block->length;
                this->setBlockAsReserved(inode, block);
            }
        }

        /**
         * Naplní parametr reserved seznamem bloků, které má soubor určený inode zarezervovány
         * @param inode
         * @param reserved
         */
        void getReservedBlocks(inode_t inode, std::vector<vBlock*>* reserved);

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
         * Odstraní blok ze všech pomocných seznamů, takže se jeví jako neobsazený
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

