/**
 * @author Martin Pavlásek (xpavla07@stud.fit.vutbr.cz)
 *
 * Created on 18. březen 2012
 */


#include <cstring>
#include <exception>
#include <iostream>
#include <string>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>

#include "hiddenfs-lib.h"
#include "common.h"

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


namespace HiddenFS {

    void hiddenFs::allocatorFindFreeBlock_sameHash(vBlock* block, T_HASH hash) {
        /// @todo doimplementovat!
        throw ExceptionBlockNotFound();
    }

    void hiddenFs::allocatorFindFreeBlock_differentHash(vBlock* block, T_HASH exclude) {
        /// @todo doimplementovat!
        throw ExceptionBlockNotFound();
    }

    void hiddenFs::allocatorFindFreeBlock_any(vBlock* block) {
        /// @todo doimplementovat!
        throw ExceptionBlockNotFound();
    }

    // hotovo
    void hiddenFs::allocatorFindFreeBlock_any(vBlock* block, T_HASH prefered) {
        try {
            this->allocatorFindFreeBlock_sameHash(block, prefered);
        } catch(ExceptionBlockNotFound) {
            // i tato metoda může vyhazovat výjimku ExceptionBlockNotFound,
            // což je v pořádku akorát to znamená, že v systému se už nenacházejí žádné volné bloky.
            try {
                 this->allocatorFindFreeBlock_differentHash(block, prefered);
            } catch (ExceptionBlockNotFound) {
                throw ExceptionDiscFull();
            }
        }
    }

    size_t hiddenFs::allocatorAllocate(inode_t inode, const char* buffer, size_t lengthParam, off_t offsetParam) {
        vFile* file;
        size_t length;
        unsigned int redundancyAmount;
        off_t offset;
        std::string filePath;
        vBlock block;

        std::map<int, std::vector<vBlock*> >::iterator mapit;
        std::vector<vBlock*>::iterator vectit;
        contentTable::tableItem cTableItem;

        this->ST->findFileByInode(inode, &file);

        // len(CRC) = 4B
        // len(BLOK) = 100B
        // len(fragment) = len(BLOK) - len(CRC)
        // BLOK = fragment + CRC

        // 1) rozdělit celý soubor na fragmenty
        // 2) vypočítat počet bloků (myslet na redundanci!)
        // 3) načíst stávající obsah do bufferu pro případ obnovy
        // 4) v cyklu uložit fragmenty do bloků
        // - pokud se něco pokazí vrátit chybovou ERRNO

        /*
        /// Maximálně využít úložiště - do jednoho fyzického souboru ukládat bloky více virtuálních souborů
        if((this->allocatorFlags & ALLOCATOR_SPEED) != 0) {
                // size = požadovaná délka pro uložení
                // 1) if size > allocated + reserved
                this->CT->getMetadata(inode, &cTableItem);

                // pro uložení nového obsahu máme k dispozici dostatek místa
                if(cTableItem.reservedBytes + file->size <= lengthParam) {
                    offset = 0;

                    // průchod přes všechny již obsazené bloky
                    for(mapit = cTableItem.content.begin(); mapit != cTableItem.content.end(); mapit++) {
                           redundancyCompleted = 0;
                        for(vectit = mapit->second.begin(); vectit != mapit->second.end(); vectit++) {

                            // omezení počtu kopií (=množství redundance)
                            if(redundancyCompleted >= this->allocatorRedundancy) {
                                break;
                            }

                            try {
                                this->allocatorFindFreeBlock_any(&block, (*vectit)->hash);
                            } catch(ExceptionDiscFull) {
                                /// @todo doimplementovat!
                            }

                            this->HT->find(block.hash, &filePath);
                            /// @todo dump vBlock do bufferu
                            //this->writeBlock(filePath, block.block, buff, length);
                            redundancyCompleted++;
                        } // konec vectit

                        //offset += délka zapsaného bloku

                    } // konec mapit
                } // konec eventuality, pokud máme k dispozici dost rezervovaných bloků
        }
        */

            /** security
             * Do jednoho fyzického souboru alokovat max. jeden virtuální soubor
             */
             /// @todo doimplementovat!

            /** redundant
             * Jeden blok ukládat do více fragmentů
             */
            /// @todo doimplementovat!

        // "Na disku není dostatek místa"
        throw ExceptionDiscFull();
    }

    void hiddenFs::allocatorSetStrategy(unsigned char strategy, unsigned int redundancy = 1) {
        this->allocatorFlags = 0;
        unsigned int defaultRedundancy = 1;

        // příznaky ALLOCATOR_SECURITY a ALLOCATOR_SPEED jsou navzájem výlučné
        if((strategy & ALLOCATOR_SECURITY) > 0) {
            this->allocatorFlags = (this->allocatorFlags & ~ALLOCATOR_SPEED) | ALLOCATOR_SECURITY;
        }

        if((strategy & ALLOCATOR_SPEED) > 0) {
            this->allocatorFlags = (this->allocatorFlags & ~ALLOCATOR_SECURITY) | ALLOCATOR_SPEED;
        }

        if((strategy & ALLOCATOR_REDUNDANCY) > 0) {
            this->allocatorFlags = this->allocatorFlags | ALLOCATOR_REDUNDANCY;
            if(redundancy > 0) {
                this->allocatorRedundancy = redundancy;
            } else {
                _D("hiddenFs::allocatorSetStrategy - množství redundance musí být > 0 (nastavuji na " << defaultRedundancy << ").");
                this->allocatorRedundancy = defaultRedundancy;
            }
        } else {
            this->allocatorRedundancy = defaultRedundancy;
        }

            /*
            default: {
                stringstream ss;
                ss << "allocatorEngine::allocate - strategy '";
                ss << this->allocatorStrategy;
                ss << "' is not implemented.";

                throw ExceptionNotImplemented(ss.str());
            }
            */
    }

    /*
    hiddenFs::ALLOC_METHOD hiddenFs::allocatorGetStrategy() {
        return this->allocatorStrategy;
    }
    */

    hiddenFs::hiddenFs() {
        this->HT = new hashTable();
        this->ST = new structTable();
        this->CT = new contentTable();

        this->allocatorSetStrategy(ALLOCATOR_SPEED);
        this->cacheHashTable = true;

        /** výchozí délka bloku: 4kB */
        //BLOCK_MAX_LENGTH = (1 << 12);
        //this->BLOCK_LENGH = (1 << 10);
    }

    hiddenFs::~hiddenFs() {
        delete this->CT;
        delete this->HT;
        delete this->ST;
    }

    /** do stbuf nastavit délku souboru */
    int hiddenFs::fuse_getattr(const char* path, struct stat* stbuf) {
        int ret = 0;
        inode_t inode;
        vFile* file;
        hiddenFs* hfs = GET_INSTANCE;

        std::cout << "\n------------------------------------- \"" << path << "\" -- GET ATTR --\n";

        try {
            inode = hfs->ST->pathToInode(path);
            hfs->ST->findFileByInode(inode, &file);
        } catch (ExceptionFileNotFound) {
            return -ENOENT;
        }

        std::cout << "-------------------------------------\n";
        std::cout << print_vFile(file) << "\n";
        std::cout << "-------------------------------------\n";
        hfs->ST->print();
        std::cout << "-------------------------------------\n";
        std::cout << "-------------------------------------\n";

        memset(stbuf, 0, sizeof(struct stat));
        stbuf->st_uid = getuid();
        stbuf->st_gid = stbuf->st_uid;
        stbuf->st_mode = S_IWUSR | S_IRUSR;
        if(file->flags & vFile::FLAG_DIR) {
            stbuf->st_mode |= S_IFDIR;
        } else {
            stbuf->st_mode |= S_IFREG;
            stbuf->st_size = file->size;
        }

        return ret;
    }

    int hiddenFs::fuse_create(const char* path, mode_t mode, struct fuse_file_info* file_i) {
        std::cout << "\n&&&&&&&&& &&&&&&& &&&&&&& &&&&&&& &&&&&&& &&&&&&&\n";
        std::cout << "&&&&&&&&& &&&&&&& &&&&&&&      fuse_creat: " << path << "\n";
        std::cout << "&&&&&&&&& &&&&&&& &&&&&&& &&&&&&& &&&&&&& &&&&&&&\n\n";

        inode_t inode;
        hiddenFs* hfs = GET_INSTANCE;
        vFile* file = new vFile;

        hfs->ST->print();

        /// @todo nastavit mode podle 'mode'
        file->flags = vFile::FLAG_NONE | vFile::FLAG_READ | vFile::FLAG_WRITE | vFile::FLAG_EXECUTE;

        hfs->ST->splitPathToFilename(path, &file->parent, &file->filename);

        std::cout << print_vFile(file) << "\n";

        inode = hfs->ST->newFile(file);

        hfs->ST->print();

        hfs->CT->newEmptyContent(inode);

        return 0;
    }

    int hiddenFs::fuse_mkdir(const char* path, mode_t mode) {
        inode_t parent;
        std::string pathStr = path;
        std::string filename;
        vFile* file;
        hiddenFs* hfs = GET_INSTANCE;
        std::cout << " <<  <<<<<<<<<<<<<<<<<<<<<<<< " << "\n";
        std::cout << " << <<<<<<<<<" << path << "<<<<<<<<<< " << "\n";
        std::cout << " << mode: " << mode << "\n";
        std::cout << (mode|S_IFDIR) << "\n";
        std::cout << " <<  <<<<<<<<<<<<<<<<<<<<<<<< " << "\n";

        hfs->ST->splitPathToFilename(pathStr, &parent, &filename);
        hfs->ST->print();

        file = new vFile;
        file->flags |= vFile::FLAG_DIR;
        file->filename = filename;
        file->parent = parent;
        hfs->ST->newFile(file);

        std::cout << " <<<<<<<<<<<<<<<<<<<<<<<<<< \n";
        std::cout << " <<<<<<<<<<<" << path << "<<<<<<<<<< \n";
        std::cout << "parent: " << parent << ", filename: " << filename << "\n";
        std::cout << " <<<<<<<<<<<<<<<<<<<<<<<<<< \n";

        return 0;
    }

    int hiddenFs::fuse_open(const char* path, struct fuse_file_info* file_i) {
        // povolit otevření souboru
        return 0;

        // nemáme oprávnění pro čtení tohoto souboru
        return -EACCES;

        // soubor neexistuje
        return -ENOENT;
        return 0;
    }

    int hiddenFs::fuse_read(const char* path, char* buffer, size_t size, off_t offset, struct fuse_file_info* file_i) {

        // 1) překlad cesty na cílový i-node
        inode_t inode;
        vFile* file;
        hiddenFs* hfs = GET_INSTANCE;



        std::cout << " <<<<<<<<<<<<<<<<<<<<<<<<<< \n";
        std::stringstream str;
        str << "fuse_read('";
        str << path;
        str << "')\n";

        try {
            inode = hfs->ST->pathToInode(path, &file);
            str << "length: " << file->size << "\n";
        } catch (ExceptionFileNotFound) {
            std::cout << " ## " << "path se nepodařilo přeložit na inode..." << " ##\n";
            /** @todo po doladění fuse_read odkomentovat! */
            //return -ENOENT;
        }
        std::cout << "Obsah souboru: -" << str.str() << "-\n";
        size = str.str().length();
        memcpy(buffer, str.str().c_str(), size);
        std::cout << "buffer: -" << buffer << "-\n";

        return size;



        // 2) načíst obsah
        char* bufferContent;
        size_t sizeContent;

        hfs->getContent(file->inode, &bufferContent, &sizeContent);

        // 3) naplnění obsahu
        if(offset > sizeContent) {
            // ochrana proti čtení "za soubor"
            if(size + offset > sizeContent) {
                size = sizeContent - offset;
            }

            memcpy(buffer, bufferContent + offset, size);
        } else {
            size = 0;
        }

        return size;
    }

    int hiddenFs::fuse_readdir(const char* path, void* buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* file_i) {
        inode_t inode;
        vFile* file;
        hiddenFs* hfs = GET_INSTANCE;
        std::map<inode_t, std::set<inode_t> >::iterator it;

        /// @todo přidání následujících dvou řádek způsobuje SEGFAULT - jak je to možně?
        //filler(buffer, ".", NULL, 0);
        //filler(buffer, "..", NULL, 0);

        try {

            inode = hfs->ST->pathToInode(path);
            hfs->ST->findFileByInode(inode, &file);

            it = hfs->ST->directoryContent(file->inode);
            for(std::set<inode_t>::iterator i = it->second.begin(); i != it->second.end(); i++) {
                // kořenový adresář nevypisovat, protože se jedná jen o logický záznam
                if(*i == structTable::ROOT_INODE) {
                    continue;
                }

                hfs->ST->findFileByInode((*i), &file);
                filler(buffer, file->filename.c_str(), NULL, 0);
            }
        } catch (ExceptionFileNotFound) {
            return -ENOENT;
        }

        return -errno;
    }

    int hiddenFs::fuse_rename(const char* from, const char* to) {
        inode_t inodeParent, inode;
        std::string filenameTo;
        hiddenFs* hfs = GET_INSTANCE;
        vFile* fileFrom;
        vFile* fileTo;

        // Rozfázování jednotlivých operací:

        // 1) vyhledat "from"
        // 2) vyhledat "to"
        // 2a) pokud "to" EXISTUJE
        //     - ST->removeFile(from) -- pouze obsah, cestu budeme pořebovat
        //     - ST->moveFile(from(inode), to(inode) parent)
        // 2b) pokud "to" NEEXISTUJE
        //     - ST->moveFile(from(inode), to(inode) parent)

        inode = hfs->ST->pathToInode(from);
        hfs->ST->findFileByInode(inode, &fileFrom);

        hfs->ST->splitPathToFilename(to, &inodeParent, &filenameTo);
        try {
            hfs->ST->findFileByName(filenameTo, inodeParent, &fileTo);

            // v cílovém adresáři se už tento souboru nachází -> starý smazat
            hfs->ST->removeFile(fileTo->inode);
        } catch (ExceptionFileNotFound) { }

        hfs->ST->moveInode(fileFrom, inodeParent);

        fileFrom->filename = filenameTo;

        return 0;
    }

    int hiddenFs::fuse_rmdir(const char* path) {
        std::cout << " <<  <<<<<<<<<<<<<<<<<<<<<<<< \n";
        std::cout << " << <<<<<<<<<" << path << "<<<<<<<<<< \n";
        std::cout << " <<  <<<<<<<<<<<<<<<<<<<<<<<< \n";
        throw "hiddenFs::fuse_rmdir ještě není implementované!";

        return 0;
    }

    int hiddenFs::fuse_write(const char* path, const char* buffer, size_t size, off_t offset, struct fuse_file_info* file_i) {
        vFile* file;
        inode_t inode;
        hiddenFs* hfs = GET_INSTANCE;
        size_t writeSize = 0;

        inode = hfs->ST->pathToInode(path, &file);
        /// @todo fake hodnota!
        file->size = 30;

        try {
            writeSize = hfs->allocatorAllocate(inode, buffer, size, offset);
        } catch (ExceptionDiscFull) {
            return -ENOSPC;
        }

        /// @todo vracet skutečný počet zapsaných bytů
        return size;
    }

    int hiddenFs::run(int argc, char* argv[]) {
        std::string path;



        if(argc == 2) {
            path.assign(argv[1]);
        } else {
            path.assign("../../mp3_examples");
        }

        /*
        std::string st;
        st = "abced";
        this->writeBlock("funnyman.mp3", 3, (char *)st.c_str(), st.length() + 1);
        char* buf = new char[500];
        this->readBlock("funnyman.mp3", 3, buf, 500);
        std::cout << "Obsah načteného bloku: " << buf << "\n";
        */

        //this->storageRefreshIndex(path);

        // 1) find boot-block

        // 2) podle potřeby založit ST, CT, HT

        // 3) načíst ST, CT, HT

        inode_t inode_ret;

        vFile* file;
        file = new vFile;
        file->filename = "soubor2.txt";
        file->flags = vFile::FLAG_NONE;
        file->parent = 1;
        file->size = 37;
        this->ST->newFile(file);

        file = new vFile;
        file->filename = "dir1";
        file->flags = vFile::FLAG_DIR;
        file->parent = 1;
        file->size = 0;
        inode_ret = this->ST->newFile(file);

        file = new vFile;
        file->filename = "souborJiný.txt";
        file->flags = vFile::FLAG_NONE;
        file->parent = inode_ret;
        file->size = 123;
        this->ST->newFile(file);
        this->ST->print();

        vFile* ftest;
        this->ST->findFileByInode(1, &ftest);

        /*
        char* content = new char[file->size];
        string content_string = "obsah souboru";
        size_t length = sizeof(content_string.c_str());
        memcpy(content, content_string.c_str(), length);

        this->allocator->allocate(file->inode, content, length);

        cout << endl;
        this->CT->print();
        */

        // set allowed FUSE operations
        static struct fuse_operations fsOperations;

        fsOperations.getattr = this->fuse_getattr;
        fsOperations.readdir = this->fuse_readdir;
        fsOperations.open = this->fuse_open;
        fsOperations.read = this->fuse_read;
        fsOperations.write = this->fuse_write;
        fsOperations.create  = this->fuse_create;
        fsOperations.rename  = this->fuse_rename;
        fsOperations.mkdir  = this->fuse_mkdir;
        fsOperations.rmdir  = this->fuse_rmdir;

        //delete [] content;

        // private data = pointer to actual instance
        /** @todo vyřazení FUSE z provozu!! */
        return fuse_main(argc, argv, &fsOperations, this);
        return 0;
    }

    void hiddenFs::enableCacheHashTable(bool cache) {
        this->cacheHashTable = cache;
    }

    bool hiddenFs::checkSum(char* content, size_t contentLength, checksum_t checksum) {
        /** @todo implementova CRC nebo jiný součtový algoritmus */
        std::cout << "implementovat hiddenFs::checkSum!\n";
        return true;
    }

    void hiddenFs::reconstructBlock(vBlock** block, char* buffer, size_t length) {

    }

    void hiddenFs::dumpBlock(vBlock* block, char* buffer, size_t length) {

    }

    void hiddenFs::getContent(inode_t inode, char** buffer, size_t* length) {
        contentTable::tableItem* ti = NULL;
        vFile* file = NULL;
        char* fragmentBuffer = new char[BLOCK_MAX_LENGTH];
        size_t len = 0;                     // pozice posledního zapsaného fragmentu
        *length = 0;
        int counter = 0;                    // pořadové číslo posledního zapsaného fragmentu
        size_t blockLenRet = 0;
        size_t blockLen = 0;                // celková délka bloku (nejen jeho využitého obsahu)
        size_t fragmentContentLen = 0;
        blockContent* block = new blockContent;

        this->CT->getMetadata(inode, ti);
        this->ST->findFileByInode(inode, &file);

        *buffer = new char[file->size];

        for(std::map<int, std::vector<vBlock*> >::iterator i = ti->content.begin(); i !=  ti->content.end(); i++) {

            // detekce chybějícího fragmentu
            if(counter != i->first) {
                std::stringstream ss;
                ss << "Soubor s inode č. " << inode << " v souboru neobsahuje všechny fragmenty!";
                throw ExceptionRuntimeError(ss.str());
            }

            // nalezení prvního nepoškozeného bloku a připojení ke zbytku skutečného souboru
            for(std::vector<vBlock*>::iterator j = i->second.begin() ; j != i->second.end(); j++)
            {
                blockLenRet = 0;
                fragmentContentLen = (*j)->length;
                blockLen = fragmentContentLen + sizeof(checksum_t);

                blockLenRet = this->readBlock((*j)->hash, (*j)->block, fragmentBuffer, blockLen);

                // podařilo se načíst fragment celý?
                if(blockLenRet != blockLen) {
                    if(j == i->second.end()) {
                        std::stringstream ss;
                        ss << "Blok č. " << (*j)->block << " v souboru s hash " << (*j)->hash << " se nepodařilo načíst celý!";
                        throw ExceptionRuntimeError(ss.str());
                    }

                    // pokusit se načíst další z kopie bloku
                    continue;
                }

                memcpy(block, fragmentBuffer, blockLen);

                // není obsah poškozený?
                if(!this->checkSum(fragmentBuffer, fragmentContentLen, block->checksum)) {
                    if(j == i->second.end()) {
                        std::stringstream ss;
                        ss << "Blok č. " << (*j)->block << " v souboru s hash " << (*j)->hash << " je poškozený!";
                        throw ExceptionRuntimeError(ss.str());
                    }

                    // pokusit se načíst další z kopie bloku
                    continue;
                }

                /** @todo spojit dohromady */
                memcpy(*buffer + len, block->content, fragmentContentLen);

                counter++;
                len += fragmentContentLen;
                break;
            }
        }

        if(len != file->size) {
            std::stringstream ss;
            ss << "Soubor s inode č. " << inode << " v souboru neobsahuje všechny fragmenty!";
            throw ExceptionRuntimeError(ss.str());
        }

        *length = len;
    }


    size_t hiddenFs::readBlock(std::string filename, T_BLOCK_NUMBER block, char* buff, size_t length) {
        void* context;
        size_t ret;

        context = this->createContext(filename);
        ret = this->readBlock(context, block, buff, length);
        this->flushContext(context);
        this->freeContext(context);

        return ret;
    }

    void hiddenFs::writeBlock(std::string filename, T_BLOCK_NUMBER block, char* buff, size_t length) {
        void* context;

        context = this->createContext(filename);
        this->writeBlock(context, block, buff, length);
        this->flushContext(context);
        this->freeContext(context);
    }

    void hiddenFs::removeBlock(std::string filename, T_BLOCK_NUMBER block) {
        void* context;

        context = this->createContext(filename);
        this->removeBlock(context, block);
        this->flushContext(context);
        this->freeContext(context);
    }
}