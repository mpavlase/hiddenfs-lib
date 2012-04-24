/**
 * @author Martin Pavlásek (xpavla07@stud.fit.vutbr.cz)
 *
 * Created on 18. březen 2012
 */


#include <cstring>
#include <exception>
#include <iostream>
#include <string>
#include <sstream>
#include <fstream>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>

#include "hiddenfs-lib.h"

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

    void hiddenFs::allocatorFindFreeBlock_sameHash(vBlock* block, hash_t hash) {
        /// @todo doimplementovat!
        throw ExceptionBlockNotFound();
    }

    void hiddenFs::allocatorFindFreeBlock_differentHash(vBlock* block, hash_t exclude) {
        /// @todo doimplementovat!
        throw ExceptionBlockNotFound();
    }

    void hiddenFs::allocatorFindFreeBlock_any(vBlock* block) {
        /// @todo doimplementovat!
        throw ExceptionBlockNotFound();
    }

    // hotovo
    void hiddenFs::allocatorFindFreeBlock_any(vBlock* block, hash_t prefered) {
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
        vFile* file = NULL;
        size_t length = 0;
        unsigned int redundancyAmount = 0;
        off_t offset = 0;
        std::string filePath = "";
        vBlock block;

        // fake!
        length = 0;
        redundancyAmount = 0;
        offset = 0;

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

    hiddenFs::hiddenFs(IEncryption* instance = NULL) {
        srand(time(NULL));

        this->HT = new hashTable();
        this->ST = new structTable();
        this->CT = new contentTable();
        this->SB = new superBlock();
        this->checksum = new CRC;
        if(instance == NULL) {
            instance = new DefaultEncryption();
        }
        this->encryption = instance;

        this->SB = new superBlock();
        this->SB->setEncryptionInstance(this->encryption);

        this->FIRST_BLOCK_NO = 1;

        /*
        std::stringbuf s;
        s.sputn("abc", 3);
        s.sputc(0);
        s.sputc('d');
        //std::cout << sizeof(vBlock) << std::endl;
        int si = 5;
        si = sizeof(vFile);
        si = sizeof(vBlock);
        si = sizeof(hash_t);
        si = sizeof(HiddenFS::superBlock::tableItem);
        char* bf = new char[si];
        char c;
        s.sgetn(bf, si);

        for(int i = 0; i < si; i++) {
            std::cout.flags(std::stringstream::hex | std::stringstream::showbase);
            std::cout.setf(std::ios::hex | std::ios::showbase);
            c = bf[i];
            std::cout << (unsigned char)c << " ";
            //printf("%X ", bf[i]);
        }
        int si = sizeof(blockContent);
        throw ExceptionRuntimeError("...");
        */

        this->allocatorSetStrategy(ALLOCATOR_SPEED);
        //this->cacheHashTable = true;
    }

    hiddenFs::~hiddenFs() {
        delete this->HT;
        delete this->ST;
        delete this->CT;
        delete this->checksum;
        delete this->encryption;
        delete this->SB;
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
            return -ENOENT;
        }
        std::cout << "Obsah souboru: -" << str.str() << "-\n";
        size = str.str().length();
        memcpy(buffer, str.str().c_str(), size);
        std::cout << "buffer: -" << buffer << "-\n";

        return size;



        // 2) načíst obsah
        bytestream_t* bufferContent;
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
        //HiddenFS::hash_t_sizeof.set(20);

        if(argc == 2) {
            path.assign(argv[1]);
        } else {
            path.assign("../../mp3_examples");
        }

        //std::cout << "hash_sizeof: " << this->hash_t_sizeof() << std::endl;
        /*
        smartConst<int> smart;
        smart.set(14);
        std::cout << "hash_sizeof: " << smart.get() << std::endl;
        assert(false);
        */

        //std::string st;
        //st = "NetBeans";
        //size_t tt = 2000;
        //char* buf = new char[tt];

        /*
        this->writeBlock("funnyman.mp3", 16, (char *)st.c_str(), 500);
        this->writeBlock("funnyman.mp3", 21, (char *)st.c_str(), 500);
        this->writeBlock("funnyman.mp3", 71, (char *)st.c_str(), 500);
        this->readBlock("funnyman.mp3", 15, buf, 500);
        this->writeBlock("funnyman.mp3", 15, (char *)st.c_str(), st.length() + 1);

        st = "blabol";
        this->writeBlock("funnyman.mp3", 110, (char *)st.c_str(), st.length() + 1);

        st = "LTD-ESP";
        this->writeBlock("funnyman.mp3", 4, (char *)st.c_str(), st.length() + 1);
        */
        //this->readBlock("funnyman.mp3", 15, buf, 500);
        //std::cout << "Obsah načteného bloku: " << buf << "\n";

        /*
        this->writeBlock("funnyman.mp3", 15, (char *)st.c_str(), tt);
        this->writeBlock("funnyman.mp3", 16, (char *)st.c_str(), tt);
        this->writeBlock("funnyman.mp3", 20, (char *)st.c_str(), tt);
        this->writeBlock("funnyman.mp3", 21, (char *)st.c_str(), tt);
        this->writeBlock("funnyman.mp3", 70, (char *)st.c_str(), tt);
        this->writeBlock("funnyman.mp3", 71, (char *)st.c_str(), tt);
        this->writeBlock("funnyman.mp3", 80, (char *)st.c_str(), tt);
        this->writeBlock("funnyman.mp3", 81, (char *)st.c_str(), tt);
        this->writeBlock("funnyman.mp3", 82, (char *)st.c_str(), tt);
        this->writeBlock("funnyman.mp3", 83, (char *)st.c_str(), tt);
        this->writeBlock("funnyman.mp3", 84, (char *)st.c_str(), tt);
        this->writeBlock("funnyman.mp3", 85, (char *)st.c_str(), tt);
        this->writeBlock("funnyman.mp3", 86, (char *)st.c_str(), tt);
        //this->removeBlock("funnyman.mp3", 4);
        //this->removeBlock("funnyman.mp3", 15);
        //this->removeBlock("funnyman.mp3", 110);

        //this->readBlock("funnyman.mp3", 13, buf, 500);
        //std::cout << "Obsah načteného bloku: " << buf << "\n";
        //this->createContext("funnyman.mp3");
        throw ExceptionRuntimeError("ruční konec...");
        */

        vBlock* bl = new vBlock;
        bl->block = 3;
        bl->fragment = 1;
        bl->hash = "hashString";
        bl->length = 4096;
        bl->used = true;


        this->storageRefreshIndex(path);
        this->HT->print();

        if(this->HT->size() == 0) {
            std::cerr << "Úložiště neobsahuje žádné dostupné soubory.\n";

            /// @todo 1 nahradit konstantou
            return 1;
        }

        // 1) nalezení boot-block
        bytestream_t* bufferBlock;
        id_byte_t idByte;

        for(hashTable::table_t_constiterator i = this->HT->begin(); i != this->HT->end(); i++) {
            std::cout << "V souboru: " << i->second << " hledám superblock...\n";
            try {
                this->readBlock((hash_t)i->first, FIRST_BLOCK_NO, &bufferBlock, BLOCK_MAX_LENGTH, &idByte);
                this->SB->deserialize(bufferBlock, BLOCK_MAX_LENGTH);
                break;
            } catch (...) {
                std::cout << "SB nenalezen.\n\n";
                continue;
            }
        }

        //bytestream_t* ctBuf = NULL;
        //size_t ctBufSize = 0;

        std::cout << "=====================================\n";
        std::cout << "=====================================\n";
        std::cout << "=====================================\n";

        inode_t inode_ret;

        vFile* file;
        file = new vFile;
        file->filename = "soubor2.txt";
        file->flags = vFile::FLAG_NONE;
        file->parent = 1;
        file->size = 37;
        this->ST->newFile(file);

        this->CT->newEmptyContent(file->inode);

        vBlock* vb= new vBlock();
        vb->block = 123;
        vb->fragment = 4;
        vb->hash = "hahsXXXXXXXXXXXXXXXX";
        vb->length = 14;
        vb->used = true;
        this->CT->addContent(file->inode, vb);

        //assert(false);

        //this->allocator->allocate(file->inode, content, length);

        std::cout << "\n";
        this->CT->print();

        chainList_t chainCT;
        this->CT->serialize(&chainCT);
        std::cout << "Počet entit v CT: " << chainCT.size() << "\n";
        std::cout.write((char*) chainCT[0].content, chainCT[0].length);
        std::cout << "---" << std::endl;

        delete this->CT;
        this->CT = new contentTable();
        this->CT->deserialize(chainCT);
        this->CT->print();


        assert(false);

        // první spuštění - super block zatím neexistuje, bude nutné jej vytvořit.
        if(!this->SB->isLoaded()) {
            // hlavní záznam uložit do prvního souboru (prvního z pohledu seřazených hash
            hashTable::table_t_constiterator i;
            bytestream_t* sbBuffer;
            size_t sbBufferLen = 0;
            idByte = idByteGenSuperBlock();

            // serializovat ST
            // serializovat CT

            for(hashTable::table_t_constiterator i = this->HT->begin(); i != this->HT->end(); i++) {
                try {
                    this->readBlock(i->first, FIRST_BLOCK_NO, &sbBuffer, BLOCK_MAX_LENGTH, &idByte);
                } catch (ExceptionBlockNotUsed) {
                    this->SB->serialize(&sbBuffer, &sbBufferLen);

                    if(sbBufferLen > BLOCK_MAX_LENGTH) {
                        std::cerr << "Superblok se nevejde do jediného bloku!\n";
                        return 0;
                    }

                    this->writeBlock(i->first, FIRST_BLOCK_NO, sbBuffer, sbBufferLen, idByte);
                } /*catch (std::exception& e) {
                    std::cerr << e.what() << "\n";
                    continue;
                }*/
            }

        }

        // 2) podle potřeby založit ST, CT, HT

        // 3) načíst ST, CT, HT

        // 3c) nové načtení obsahu hashTable

        assert(false);

        //inode_t inode_ret;

        //vFile* file;
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

    void hiddenFs::packData(bytestream_t* input, size_t inputSize, bytestream_t** output, size_t* outputSize, id_byte_t id_byte) {
        size_t dataOffset;
        checksum_t crc;

        if(inputSize > BLOCK_USABLE_LENGTH) {
            throw ExceptionRuntimeError("Vstupní data jsou delší, než je maximální dostupná délka jednoho bloku");
        }

        // jednotlivé složky jsou uloženy v následujícím pořadí:
        // | CRC | id_byte |     užitný obsah bloku     |
        dataOffset = sizeof(checksum_t) + sizeof(id_byte_t);

        *output = new blockContent;
        *outputSize = sizeof(*output);

        // uložení identifikačního byte
        memcpy(*output + sizeof(checksum_t), &id_byte, sizeof(id_byte));

        // uložení užitného obsahu bloku
        memcpy(*output + dataOffset, input, inputSize);

        // výpočet kontrolního součtu ze všech složek struktury, pochopitelně
        // až na složku se součtem samotným
        crc = this->checksum->calculate(((bytestream_t*) (*output) + sizeof(checksum_t)), *outputSize - sizeof(checksum_t));

        // zápis kontrolního součtu
        memcpy(*output, &crc, sizeof(checksum_t));
    }

    void hiddenFs::unpackData(bytestream_t* input, size_t inputSize, bytestream_t** output, size_t* outputSize, id_byte_t* id_byte) {
        checksum_t crcOrig, crcCalculated;
        size_t dataOffset;

        // extrakce kontrolního součtu
        memcpy(&crcOrig, input, sizeof(checksum_t));
        crcCalculated = this->checksum->calculate((input + sizeof(checksum_t)), inputSize - sizeof(checksum_t));

        if(crcCalculated != crcOrig) {
            throw ExceptionRuntimeError("Kontrolní součet bloku nesouhlasí!");
        }

        // extrakce identifikačního byte
        memcpy(id_byte, input + sizeof(checksum_t), sizeof(id_byte_t));

        dataOffset = sizeof(checksum_t) + sizeof(id_byte_t);
        *outputSize = inputSize - dataOffset;

        // extrakce užitné části
        *output = new bytestream_t[*outputSize];
        memcpy(output, input + dataOffset, *outputSize);
    }

    void hiddenFs::getContent(inode_t inode, bytestream_t** buffer, size_t* length) {
        contentTable::tableItem* ti = NULL;
        vFile* file = NULL;
        bytestream_t* fragmentBuffer;
        bytestream_t* fragmentBufferPost;
        size_t len = 0;                     // pozice posledního zapsaného fragmentu
        *length = 0;
        unsigned int counter = 0;                    // pořadové číslo posledního zapsaného fragmentu
        size_t blockLen = 0;                // celková délka bloku (nejen jeho využitého obsahu)
        size_t fragmentContentLen;
        id_byte_t idByte;

        this->CT->getMetadata(inode, ti);
        this->ST->findFileByInode(inode, &file);

        // naalokování bufferu pro kompletní obsah souboru
        std::stringbuf stream;

        // iterace přes všechny fragmenty souboru
        for(std::map<fragment_t, std::vector<vBlock*> >::iterator i = ti->content.begin(); i !=  ti->content.end(); i++) {
            // detekce chybějícího fragmentu
            if(counter != i->first) {
                std::stringstream ss;

                ss << "Soubor s inode č. " << inode << " v souboru neobsahuje všechny fragmenty!";
                throw ExceptionRuntimeError(ss.str());
            }

            // nalezení prvního nepoškozeného bloku a připojení fragmentu ke zbytku skutečného souboru
            for(std::vector<vBlock*>::iterator j = i->second.begin() ; j != i->second.end(); j++) {
                /**
                 * 1) v try {} načíst blok podle hashe přes blockRead
                 * 2) unpackData
                 * 3)
                 */

                try {
                    fragmentContentLen = this->readBlock((*j)->hash, (*j)->block, &fragmentBuffer, BLOCK_MAX_LENGTH, &idByte);
                    //this->unpackData(fragmentBuffer, fragmentContentLen, &fragmentBufferPost, &blockLen, &identifyByte);

                    // dokončit!
                    assert(false);
                    //stream.sputn((const char*) fragmentBufferPost, blockLen);
                    //memcpy(*buffer + len, fragmentBufferPost, blockLen);

                    // uvolnění alokovaných prostředků
                    delete fragmentBuffer;
                    delete fragmentBufferPost;
                } catch(...) {
                    /* při načítání bloku něco nebylo v pořádku - nezkoumáme dál
                     * co se stalo, protože stejně se vždy pokusíme načíst další
                     * kopii bloku, pokud nějaká existuje */
                    continue;
                }

                // počítadlo fragmentů
                counter++;

                // celková délka streamu
                len += blockLen;

                break;
            }
        }

        *buffer = new bytestream_t[len];

        //dešifrování obsahu
        this->encryption->decrypt(*buffer, len, &fragmentBuffer, &blockLen);

        if(blockLen != file->size) {
            std::stringstream ss;
            ss << "Soubor s inode č. " << inode << " v souboru neobsahuje všechny fragmenty!";

            throw ExceptionRuntimeError(ss.str());
        }

        *length = blockLen;
    }


    size_t hiddenFs::readBlock(hash_t hash, block_number_t block, bytestream_t** buff, size_t length, id_byte_t* idByte) {
        void* context = NULL;
        size_t ret;
        std::string filename;

        bytestream_t* buff2 = NULL;

        this->HT->find(hash, &filename);
        context = this->createContext(filename);
        buff2 = new bytestream_t[length];
        *buff = NULL;

        try {
            ret = this->readBlock(context, block, buff2, length);
        } catch (ExceptionBlockNotFound& bnf) {
            // uklidit po sobě...
            if(buff2 != NULL) {
                delete buff2;
            }

            // distribuovat výjimku výše, protože zde ještě nevíme jak s ní naložit
            throw bnf;
        } catch (ExceptionBlockNotUsed& bnu) {
            // uklidit po sobě...
            if(buff2 != NULL) {
                delete buff2;
            }

            // distribuovat výjimku výše, protože zde ještě nevíme jak s ní naložit
            throw bnu;
        }

        try {
            this->unpackData(buff2, length, buff, &ret, idByte);

            this->flushContext(context);
            this->freeContext(context);
        } catch (ExceptionRuntimeError& e) {
            delete buff2;

            throw e;
        }

        return ret;
    }

    void hiddenFs::writeBlock(hash_t hash, block_number_t block, bytestream_t* buff, size_t length, id_byte_t idByte) {
        void* context;
        std::string filename;
        bytestream_t* buff2 = NULL;
        size_t length2;

        try {
            this->HT->find(hash, &filename);
            context = this->createContext(filename);

            this->packData(buff, length, &buff2, &length2, idByte);

            if(length2 > BLOCK_MAX_LENGTH) {
                throw ExceptionRuntimeError("Pokus o zapsání bloku dat, který je delší, než maximální povolená délka.");
            }

            this->writeBlock(context, block, buff2, length2);

            this->flushContext(context);
            this->freeContext(context);
        } catch (std::exception& e) {
            // uklidit po sobě...
            if(buff2 != NULL) {
                delete buff2;
            }

            // distribuovat výjimku výše, protože zde ještě nevíme jak s ní naložit
            std::cerr << "" << e.what() << std::endl;
            throw e;
        }
    }

    void hiddenFs::removeBlock(hash_t hash, block_number_t block) {
        void* context;
        std::string filename;

        this->HT->find(hash, &filename);

        context = this->createContext(filename);

        this->removeBlock(context, block);

        this->flushContext(context);
        this->freeContext(context);
    }
}