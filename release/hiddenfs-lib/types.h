/**
 * @author Martin Pavlásek (xpavla07@stud.fit.vutbr.cz)
 *
 * Created on 18. březen 2012, 19:48
 */

#ifndef TYPES_H
#define	TYPES_H

#include <bits/types.h>
#include <string>
#include <sstream>

using namespace std;

/** data type of number of block inside real storage item */
typedef unsigned int T_BLOCK_NUMBER;
typedef int inode_t;
typedef char flags_t;
typedef string T_HASH;

const int BLOCK_IN_USE = 1;
const int BLOCK_RESERVED = 0;

/**
 * Reprezentace virtuálního souboru - skrývaného souboru
 */
struct vFile {
    static const flags_t FLAG_NONE      = 0;
    static const flags_t FLAG_READ      = 1 << 0;
    static const flags_t FLAG_WRITE     = 1 << 1;
    static const flags_t FLAG_EXECUTE   = 1 << 2;
    static const flags_t FLAG_DIR       = 1 << 3;

    vFile() {
        flags = FLAG_NONE;
        size = 0;
        filename = "";
        inode = -1;
    }
    /*
    vFile(const vFile& f) {
        inode = f.inode;
        parent = f.parent;
        size = f.size;
        cout << "---------#######x--------" << endl;
        cout << f.filename << endl;
        filename.assign(f.filename);
        flags = f.flags;
    }
    */
    /** i-node identifikátor soboru */
    inode_t inode;

    /** číslo inode rodiče */
    inode_t parent;

    /** délka souboru v bytech */
    size_t size;

    /** název skrývaného souboru */
    string filename;

    /** příznaky souboru (D, RWX) */
    flags_t flags;
};

/** abstrakce datového bloku obsahující právě jeden fragment */
struct vBlock {
    /** identifikace skutečného souboru */
    T_HASH hash;

    /** pořadové číslo bloku ve skutečném souboru (nesmí se opakovat v rámci vBlock) */
    T_BLOCK_NUMBER block;

    /** číslo fragmentu virtuálního souboru (může se opakovat v rámci vFile) */
    int fragment;

    /** příznak, jestli je tento blok použitý (BLOCK_IN_USE), nebo jen rezervovaný (BLOCK_RESERVED) */
    int used;

    /** délka využité části bloku (délka blockContent.content) */
    size_t length;
};

#endif	/* TYPES_H */

