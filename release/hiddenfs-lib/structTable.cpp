/**
 * @author Martin Pavlásek (xpavla07@stud.fit.vutbr.cz)
 *
 * Created on 14. březen 2012
 */

#include "structTable.h"
#include "common.h"
#include <string>
#include <iostream>
#include <iomanip>

using namespace std;

structTable::structTable() {
    this->maxInode = 0;
    // TODO: vytvořit root?
}

structTable::structTable(const structTable& orig) {
}

structTable::~structTable() {
    // uvolnění hlavní tabulky
    for(table_t::iterator it = this->table.begin(); it != this->table.end(); it++) {
        delete it->second;
    }

    /*
    // uvolnění pomocné tabulky
    for(map<inode_t, set<inode_t> >::iterator it = this->tableDirContent.begin(); it != this->tableDirContent.end(); it++) {
        delete it->second;
    }
    */
}



void structTable::findFileByInode(inode_t inode, vFile* file) throw (ExceptionFileNotFound)  {
    table_t::iterator i = table.find(inode);

    /*
    cout << i << endl;
    if(i == NULL) {
        throw new FileNotFound;
    }
    */

    //memcpy(file, i->second, sizeof(vFile));
    file = i->second;
}

void structTable::findFileByName(string filename, inode_t parent, vFile* file) throw (ExceptionFileNotFound)  {

}


void structTable::print() {
    cout << "== structTable.table (inode -> metadata souboru) ==" << endl;
    cout << setw(5) << left << "inode"
        << "|" << setw(6) << left << "parent"
        << "|" << setw(20) << left << "filename"
        << "|" << setw(4) << left << "size" << endl;
    cout << "--------------------------------------" << endl;
    for(table_t::iterator it = this->table.begin(); it != this->table.end(); it++) {
        cout << print_vFile(it->second) << endl;
    /*
        cout << setw(5) << left << it->second->inode
            << "|" << setw(6) << left << it->second->parent
            << "|" << setw(20) << left << it->second->filename
            << "|" << setw(4) << left << it->second->size
            << endl;
    */
    }
    cout << endl;

    cout << "== structTable.tableDirContent (parent -> content) ==" << endl;
    cout << setw(6) << left << "parent"
        << "|" << "content" << endl;
    for(map<inode_t, set<inode_t> >::iterator it = this->tableDirContent.begin(); it != this->tableDirContent.end(); it++) {
        cout << setw(6) << left << it->first << "|";
        for(set<inode_t>::iterator i = it->second.begin(); i != it->second.end(); i++) {
            cout << *i << " ";
        }
    }

    cout << endl;
}

inode_t structTable::newFile(vFile* file) {
    inode_t ret_inode;

    if(this->table.begin() != this->table.end()) {
        ret_inode = this->table.end()->second->inode;
    } else {
        ret_inode = FIRST_INODE;
    }

    file->inode = ret_inode;

    // vložení souboru do hlavní tabulky
    this->table[ret_inode] = file;

    // vložení souboru do tabulky rodičů
    this->tableDirContent[file->parent].insert(ret_inode);

    this->maxInode = ret_inode;
    return ret_inode;
}

void structTable::removeFile(inode_t inode) {
    vFile fileInfo;

    //try {
        this->findFileByInode(inode, &fileInfo);
        this->table.erase(inode);
        this->tableDirContent[fileInfo.parent].erase(inode);
    //}
    //catch (ExceptionFileNotFound e) {
        //this->log()
    //}
}