/**
 * @author Martin Pavlásek (xpavla07@stud.fit.vutbr.cz)
 *
 * Created on 14. březen 2012
 */

#include <map>
#include <iomanip>
#include <iostream>
#include <vector>
#include <string.h>

#include "common.h"
#include "contentTable.h"
#include "exceptions.h"

namespace HiddenFS {
    contentTable::contentTable() {

    }

    contentTable::contentTable(const contentTable& orig) {
    }

    contentTable::~contentTable() {
        // uvolnění tabulky fragmentů
        for(table_t::iterator it = this->table.begin(); it != this->table.end(); it++) {
            for(std::map<int, std::vector<vBlock*> >::iterator i = it->second.content.begin(); i !=  it->second.content.end(); i++) {
                for(std::vector<vBlock*>::iterator j = i->second.begin() ; j != i->second.end(); j++)
                {
                    delete *j;
                }
            }
        }
    }

    void contentTable::serialize(unsigned char** ouputParam, size_t* sizeParam) {
        size_t itemLength;

        std::stringbuf stream;
        itemLength = sizeof(vBlock);
        const size_t chunkSize = itemLength * 500;
        size_t offset = 0;
        unsigned char* buff = new unsigned char[chunkSize];

        // zjištění počtu dále zpracovaných bloků
        // iterace přes všechny soubory
        for(table_t::const_iterator i = this->table.begin(); i != this->table.end(); i++) {
            // iterace přes všechny fragmenty souboru
            for(std::map<int, std::vector<vBlock*> >::const_iterator j = i->second.content.begin(); j != i->second.content.begin(); j++) {
                // iterace přes všechny bloky fragmentů
                for(std::vector<vBlock*>::const_iterator k = j->second.begin(); k != j->second.end(); k++) {
                    // hrubá kopie struktury
                    memcpy(buff + offset, (void*) &((*k)->hash), (*k)->hash.length());
                    offset += (*k)->hash.length();
                    memcpy(buff + offset, (void*) &((*k)->block), sizeof(T_BLOCK_NUMBER));
                    offset += sizeof(T_BLOCK_NUMBER);
                    memcpy(buff + offset, (void*) &((*k)->fragment), sizeof(size_t));
                    offset += sizeof(size_t);
                    memcpy(buff + offset, (void*) &((*k)->used), sizeof(bool));
                    offset += sizeof(bool);
                    memcpy(buff + offset, (void*) &((*k)->length), sizeof(size_t));
                    offset += sizeof(size_t);
                    //stream.sputn((const char*) (*j), itemLength);
                }
            }
        }

        if(true) {
            // výpočet délky a alokace bufferu
            /*
            stream.seekg(std::ios::end);
            *sizeParam = stream.tellg();
            *ouputParam = new unsigned char[*sizeParam];

            stream.seekg(std::ios::end);
            stream.read(*ouputParam, *sizeParam);
            */
        } else {
            //throw ExceptionRuntimeError("");
        }
    }

    void contentTable::deserialize(unsigned char* input, size_t size) {

    }

    void contentTable::getReservedBlocks(inode_t inode, std::vector<vBlock*>* reserved) {
        reserved->assign(this->table[inode].reserved.begin(), this->table[inode].reserved.end());
    }

    void contentTable::print() {
        std::cout << "== contentTable ==" << "\n";
        std::cout << std::setw(6) << std::left << "parent"
            << "|" << "content" << "\n";
        for(table_t::iterator it = this->table.begin(); it != this->table.end(); it++) {
            std::cout << std::setw(6) << std::left << it->first << "|";

            /// @todo tisknout i rezervované bloky
            for(std::map<int, std::vector<vBlock*> >::iterator i = it->second.content.begin(); i !=  it->second.content.end(); i++) {
                std::cout << " [#" << i->first << ": ";
                for(std::vector<vBlock*>::iterator j = i->second.begin() ; j != i->second.end(); j++)
                {
                    std::cout << print_vBlock(*j);
                }
                std::cout << "]";
            }
            std::cout << "\n";
        }
    }

    void contentTable::getMetadata(inode_t inode, tableItem* content) {
        content = &this->table[inode];
    }

    void contentTable::newEmptyContent(inode_t inode) {
        tableItem ti;
        ti.reservedBytes = 0;
        ti.content.clear();
        ti.reserved.clear();

        table_t::iterator iter = this->table.find(inode);

        if(iter == table.end()) {
            this->table[inode] = ti;
        } else {
            throw ExceptionInodeExists(inode);
        }
    }

}