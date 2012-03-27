/**
 * @author Martin Pavlásek (xpavla07@stud.fit.vutbr.cz)
 *
 * Created on 18. březen 2012
 */


#include "hiddenfs-lib.h"
#include <fuse/fuse_common.h>
#include <string>
#include <cstring>
#include <iostream>

/*
struct fuse_file_info {
	// Open flags.	 Available in open() and release()
	int flags;

	// Old file handle, don't use
	unsigned long fh_old;

	// In case of a write operation indicates if this was caused by a
	    writepage
	int writepage;

	// Can be filled in by open, to use direct I/O on this file.
	    Introduced in version 2.4
	unsigned int direct_io : 1;

	// Can be filled in by open, to indicate, that cached file data
	    need not be invalidated.  Introduced in version 2.4
	unsigned int keep_cache : 1;

	// Indicates a flush operation.  Set in flush operation, also
	    maybe set in highlevel lock operation and lowlevel release
	    operation.	Introduced in version 2.6
	unsigned int flush : 1;

	// Can be filled in by open, to indicate that the file is not
	    seekable.  Introduced in version 2.8
	unsigned int nonseekable : 1;

	// Padding.  Do not use
	unsigned int padding : 28;

	// File handle.  May be filled in by filesystem in open().
	    Available in all other file operations
	uint64_t fh;

	// Lock owner id.  Available in locking operations and flush
	uint64_t lock_owner;
};
*/


using namespace std;

allocatorEngine::allocatorEngine(contentTable* CT) {
    this->CT = CT;
}

void allocatorEngine::allocate(inode_t inode, char* content, size_t length) {
    switch(this->strategy) {
        /**
         * Maximálně využít úložiště
         */
        case SPEED: {


            break;
        }

        /**
         * Do jednoho fyzického souboru alokovat max. jeden virtuální soubor
         */
        case SECURITY: {

            break;
        }

        /**
         * Jeden blok ukládat do více fragmentů
         */
        case REDUNDANCY: {

            break;
        }
    }
}

void allocatorEngine::setStrategy(ALLOC_METHOD strategy) throw (ExceptionNotImplemented) {
    switch(strategy) {
        case SPEED:
        case SECURITY:
        case REDUNDANCY: {
            break;
        }
        default: {
            stringstream ss;
            ss << "allocatorEngine::allocate - strategy '";
            ss << this->strategy;
            ss << "' is not implemented.";

            throw new ExceptionNotImplemented(ss.str());
        }
    }

    this->strategy = strategy;
}

allocatorEngine::ALLOC_METHOD allocatorEngine::getStrategy() {
    return this->strategy;
}
// -----------------------------------------------

hiddenFs::hiddenFs() {
    this->HT = new hashTable();
    this->ST = new structTable();
    this->CT = new contentTable(this->ST);
    this->allocator = new allocatorEngine(this->CT);

    this->setAllocationMethod(allocatorEngine::SPEED);
    this->cacheHashTable = true;
}

hiddenFs::~hiddenFs() {
    delete this->CT;
    delete this->HT;
    delete this->ST;
    delete this->allocator;
}

int hiddenFs::fuse_getattr(const char* path, struct stat* stbuf) {
/*
int hiddenFs::fuse_getattr(const char* path, char* b, size_t c, off_t d, fuse_file_info* e) {
    int res = 0;
    memset(stbuf, 0, sizeof(struct stat));
    if(strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
    }
    else if(strcmp(path, hello_path) == 0) {
        stbuf->st_mode = S_IFREG | 0444;
        stbuf->st_nlink = 1;
        stbuf->st_size = strlen(hello_str);
    }
    else
        res = -ENOENT;
    return res;
}
*/
    return 0;
}

int hiddenFs::fuse_create(const char* path, mode_t mode, struct fuse_file_info* file_i) {
    inode_t inode;
    hiddenFs* hfs = GET_INSTANCE;
    vFile* file = new vFile;

    /// @todo nastavit mode podle 'mode'
    file->flags = vFile::FLAG_NONE | vFile::FLAG_READ | vFile::FLAG_WRITE | vFile::FLAG_EXECUTE;

    inode = hfs->ST->newFile(file);
    hfs->CT->newContent(inode);

    return 0;
}

int hiddenFs::fuse_mkdir(const char* path, mode_t) {
    return 0;
}

int hiddenFs::fuse_open(const char* path, struct fuse_file_info* file_i) {
    return 0;
}

int hiddenFs::fuse_read(const char* path, char*, size_t, off_t, struct fuse_file_info* file_i) {
    return 0;
}

int hiddenFs::fuse_readdir(const char* path, void*, fuse_fill_dir_t, off_t, struct fuse_file_info* file_i) {
    return 0;
}

int hiddenFs::fuse_rename(const char* from, const char* to) {
    return 0;
}

int hiddenFs::fuse_rmdir(const char* path) {
    return 0;
}

int hiddenFs::fuse_write(const char* path, const char*, size_t, off_t, struct fuse_file_info* file_i) {
    return 0;
}

int hiddenFs::run(int argc, char* argv[]) {
    string path;

    if(argc == 2) {
        path.assign(argv[1]);
    } else {
        path.assign("../../mp3");
    }

    this->storageRefreshIndex(path);

    // 1) find boot-block

    // 2) podle potřeby založit ST, CT, HT

    // 3) načíst ST, CT, HT

    vFile* file = new vFile;
    file->filename = "soubor1.txt";
    file->flags = 0;
    file->parent = 1;
    file->size = 100;
    this->ST->newFile(file);
    this->ST->print();

    char* content = new char[file->size];
    string content_string = "obsah souboru";
    size_t length = sizeof(content_string.c_str());
    memcpy(content, content_string.c_str(), length);

    this->allocator->allocate(file->inode, content, length);

    cout << endl;
    this->CT->print();

    // set allowed FUSE operations
    struct fuse_operations fsOperations;

    fsOperations.getattr = this->fuse_getattr;
    fsOperations.open = this->fuse_open;
    fsOperations.read = this->fuse_read;
    fsOperations.write = this->fuse_write;
    fsOperations.create  = this->fuse_create;
    fsOperations.rename  = this->fuse_rename;
    fsOperations.mkdir  = this->fuse_mkdir;
    fsOperations.rmdir  = this->fuse_rmdir;
    fsOperations.readdir = this->fuse_readdir;

    // private data = pointer to actual instance
    /** @todo vyřazení FUSE z provozu!! */
    //return fuse_main(argc, argv, &fsOperations, this);
    return 0;
}

void hiddenFs::enableCacheHashTable(bool cache) {
    this->cacheHashTable = cache;
}

void hiddenFs::setAllocationMethod(allocatorEngine::ALLOC_METHOD method) {
    this->allocator->setStrategy(method);
}

