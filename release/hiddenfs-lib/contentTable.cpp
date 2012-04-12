/**
 * @author Martin Pavlásek (xpavla07@stud.fit.vutbr.cz)
 *
 * Created on 14. březen 2012
 */

#include <map>
#include <iomanip>
#include <iostream>
#include <vector>

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