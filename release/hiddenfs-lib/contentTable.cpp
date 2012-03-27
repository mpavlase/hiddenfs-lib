/**
 * @author Martin Pavlásek (xpavla07@stud.fit.vutbr.cz)
 *
 * Created on 14. březen 2012
 */

#include "contentTable.h"
#include "common.h"
#include "exceptions.h"
#include <iomanip>
#include <iostream>
#include <map>
#include <vector>

using namespace std;

contentTable::contentTable(structTable* ST) {
    this->ST = ST;
}

contentTable::contentTable(const contentTable& orig) {
}

contentTable::~contentTable() {
    // uvolnění tabulky fragmentů
    for(table_t::iterator it = this->table.begin(); it != this->table.end(); it++) {
        for(map<int, vector<vBlock*> >::iterator i = it->second.content.begin(); i !=  it->second.content.end(); i++) {
            for(vector<vBlock*>::iterator j = i->second.begin() ; j != i->second.end(); j++)
            {
                delete *j;
            }
        }
    }
}

void contentTable::getReservedBlocks(inode_t inode, vector<vBlock*>* reserved) {
    reserved->assign(this->table[inode].reserved.begin(), this->table[inode].reserved.end());
}

void contentTable::print() {
    cout << "== contentTable ==" << endl;
    cout << setw(6) << left << "parent"
        << "|" << "content" << endl;
    for(table_t::iterator it = this->table.begin(); it != this->table.end(); it++) {
        cout << setw(6) << left << it->first << "|";

        /// @todo tisknout i rezervované bloky
        for(map<int, vector<vBlock*> >::iterator i = it->second.content.begin(); i !=  it->second.content.end(); i++) {
            cout << " [#" << i->first << ": ";
            for(vector<vBlock*>::iterator j = i->second.begin() ; j != i->second.end(); j++)
            {
                cout << print_vBlock(*j);
            }
            cout << "]";
        }
        cout << endl;
    }
}

void contentTable::getMetadata(inode_t inode, tableItem* content) {
    content = &this->table[inode];
}

void contentTable::newContent(inode_t inode) throw (ExceptionInodeExists) {
    tableItem ti;
    table_t::iterator it = this->table.find(inode);

    if(it == table.end()) {
        this->table[inode] = ti;
    } else {
        throw new ExceptionInodeExists(inode);
    }
}

void contentTable::getContent(inode_t inode, char* buffer, size_t* length) {
    tableItem* ti = NULL;
    vFile* file = NULL;
    size_t len = 0;

    this->getMetadata(inode, ti);
    this->ST->findFileByInode(inode, file);

    buffer = new char[file->size];


    for(map<int, vector<vBlock*> >::iterator i = ti->content.begin(); i !=  ti->content.end(); i++) {
        for(vector<vBlock*>::iterator j = i->second.begin() ; j != i->second.end(); j++)
        {
            // spojit fragmenty dohromady
        }
    }
}