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
#include <map>
#include <exception>

using namespace std;

structTable::structTable() {
    this->maxInode = 0;
    // TODO: vytvořit root?
    vFile* root = new vFile;
    root->filename = "/";
    root->flags = vFile::FLAG_DIR;
    root->parent = 1;
    this->newFile(root);
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



void structTable::findFileByInode(inode_t inode, vFile** file) throw (ExceptionFileNotFound)  {
    table_t::iterator i = table.find(inode);

    if(i == table.end()) {
        throw ExceptionFileNotFound(inode);
    }

    *file = i->second;
}

void structTable::findFileByName(string filename, inode_t parent, vFile** file) throw (ExceptionFileNotFound)  {
    set<inode_t> dirContent = this->tableDirContent[parent];
    vFile* tmpFile = NULL;

    for(set<inode_t>::iterator it = dirContent.begin(); it != dirContent.end(); it++) {
        this->findFileByInode(*it, &tmpFile);

        if(tmpFile->filename == filename) {
            *file = tmpFile;
            break;
        } else {
            tmpFile = NULL;
        }
    }

    if(tmpFile == NULL) {
        throw ExceptionFileNotFound(filename);
    }
}


void structTable::print() {
    cout << "== structTable.table (inode -> metadata souboru) ==" << endl;
    /*
    cout << setw(5) << left << "inode"
        << "|" << setw(6) << left << "parent"
        << "|" << setw(20) << left << "filename"
        << "|" << setw(4) << left << "size" << endl;
    cout << "--------------------------------------" << endl;
    */
    for(table_t::iterator it = this->table.begin(); it != this->table.end(); it++) {
        cout << print_vFile(it->second) << endl;
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
        cout << endl;
    }

    cout << endl;
}

inode_t structTable::newFile(vFile* file) {
    inode_t ret_inode;

    if(this->table.size() == 0) {
        ret_inode = ROOT_INODE;
    } else {
        table_t::reverse_iterator rit = this->table.rbegin();
        ret_inode = rit->second->inode + 1;
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
    vFile* fileInfo;

    //try {
        this->findFileByInode(inode, &fileInfo);
        this->table.erase(inode);
        this->tableDirContent[fileInfo->parent].erase(inode);
    //}
    //catch (ExceptionFileNotFound e) {
        //this->log()
    //}
}

inode_t structTable::pathToInode(const char* path, vFile** retFile) {
    string str = path;

    return this->pathToInode(str, retFile);
}

inode_t structTable::pathToInode(string path) {
    vFile* retFile;

    return this->pathToInode(path, &retFile);
}

inode_t structTable::pathToInode(const char* path) {
    vFile* retFile;
    string str = path;

    return this->pathToInode(str, &retFile);
}
inode_t structTable::pathToInode(string path, vFile** retFile) {

    //cout << "překlad cesty: '" << path << "' na inode:";
    string dirname;
    inode_t inode = -1;
    inode_t parent_inode;
    vFile* file;

    if(path == "/") {
        return structTable::ROOT_INODE;
    }

    // 1 = délka oddělovače (datový typ char má délku vždy 1)
    const unsigned int DELIM_LENGTH = 1;
    unsigned int pos = DELIM_LENGTH;

    // odseknutí prvního a posledního lomítka
    pos = path.find_first_of(PATH_DELIM);
    if(pos != string::npos) {
        path = path.substr(pos + DELIM_LENGTH);
    }
    pos = path.find_last_of(PATH_DELIM);
    if(pos == path.length()) {
        path = path.substr(0, pos - 1);
    }

    pos = 0;
    parent_inode = structTable::ROOT_INODE;
    try {
        // průchod přes všechny adresáře cesty
        while((pos = path.find_first_of(PATH_DELIM)) != string::npos) {
            dirname = path.substr(0, pos);
            this->findFileByName(dirname, parent_inode, &file);
            parent_inode = file->inode;

            pos += DELIM_LENGTH;
            path = path.substr(pos);
        }

        //throw ExceptionFileNotFound(inode);

        this->findFileByName(path, parent_inode, &file);
        inode = file->inode;
        *retFile = file;
    } catch(ExceptionFileNotFound) {
        throw ExceptionFileNotFound(path);
    }

    return inode;
}

map<inode_t, set<inode_t> >::iterator structTable::directoryContent(inode_t inode) {
    return this->tableDirContent.find(inode);
}

void structTable::splitPathToFilename(string path, inode_t* parent, string* filename) {
    unsigned int pos;
    string destPath;

    pos = path.find_last_of(PATH_DELIM);
    if(pos == path.length()) {
        *parent = ROOT_INODE;
        *filename = "";
        return;
    }

    // oddělení cesty od samozného názvu souboru
    // pokud je path: /soubor.txt,
    if(pos == 0) {
        *parent = ROOT_INODE;
        *filename = path.substr(pos + 1);

        return;
    } else {
        destPath = path.substr(0, pos);
        *parent = this->pathToInode(destPath);

        // +1 je délka oddělovače (char má délku vždy 1)
        *filename = path.substr(pos + 1);
    }
}

void structTable::moveInode(vFile* file, inode_t parent) {
    // Volající metoda musí zajistit, aby se v novém umístění nenacházely dva soubory stejného jména!
    this->tableDirContent[file->parent].erase(file->inode);
    file->parent = parent;
    this->tableDirContent[parent].insert(file->inode);

    /// @todo tisknout obsah pouze pro ladění!
    this->print();
}