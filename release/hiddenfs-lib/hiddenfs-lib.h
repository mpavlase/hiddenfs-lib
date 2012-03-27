/**
 * @author Martin Pavlásek (xpavla07@stud.fit.vutbr.cz)
 *
 * Created on 14. březen 2012
 */


#define FUSE_USE_VERSION 26
#include <fuse.h>
#include "types.h"
#include "contentTable.h"
#include "structTable.h"
#include "hashTable.h"
#include <string>

#define GET_INSTANCE (hiddenFs*) fuse_get_context()->private_data

using namespace std;
struct fuse_file_info;

/**
 * Dohlíží nad alokací bloků
 */
class allocatorEngine {
public:
    typedef enum {
        /** maximize utilization of storage (more blocks per storage item) */
        SPEED = 1,

        /** prefered one virtual file per one storage item (one file per storage item) */
        SECURITY,

        /** one block will be save into several storage items */
        REDUNDANCY
    } ALLOC_METHOD;

    allocatorEngine(contentTable* CT);

    /**
     * Set strategy during allocation new blocks
     * @param strategy type of strategy (enum)
     * @todo set as bitwise operation
     */
    void setStrategy(ALLOC_METHOD strategy) throw (ExceptionNotImplemented);
    ALLOC_METHOD getStrategy();

    /***/
    void allocate(inode_t inode, char* content, size_t length);

private:
    contentTable* CT;
    ALLOC_METHOD strategy;
};

class hiddenFs {
public:
    hiddenFs();
    virtual ~hiddenFs();

    /**
     * Main executable method
     * @param argc number of arguments passed by command line
     * @param argv array of arguments passed by command line
     * @return exit status of FUSE
     */
    int run(int argc, char* argv[]);

    /**
     * true - uchovávání HashTable v metadatech spolu se StructTable atd.
     * false - při každém mount zaindexovat adresář znovu
     * @param enable/disable
     */
    void enableCacheHashTable(bool);


    /**
     * Set allocation strategy
     * @param method type of allocation strategy
     */
    void setAllocationMethod(allocatorEngine::ALLOC_METHOD method);

protected:
    /**
     * Provádí zápis bloku do skutečného souboru
     * @param filename adresa ke skutečnému souboru, ze kterého se má blok načíst
     * @param block pořadové číslo bloku v rámci souboru
     * @param buff obsah bloku
     * @param length maximální délka bloku
     * @return skutečně načtená délka bloku
     */
    virtual size_t readBlock(string filename, T_BLOCK_NUMBER block, char* buff, size_t length) throw (ExceptionBlockOutOfRange) = 0;

    /**
     * Provádí zápis bloku do skutečného souboru
     * @param filename adresa ke skutečnému souboru, do kterého se má blok schovat
     * @param block pořadové číslo bloku v rámci souboru
     * @param buff obsah bloku
     * @param length délka zapisovaného bloku
     */
    virtual void writeBlock(string filename, T_BLOCK_NUMBER block, char* buff, size_t length) = 0;

    /**
     * Rebuild content of hash table (mapping filename of real file to uniqe string)
     * @param filename path to directory as storage
     */
    virtual void storageRefreshIndex(string filename) = 0;

    contentTable* CT;
    structTable* ST;
    hashTable* HT;
    allocatorEngine* allocator;

    bool cacheHashTable;

    static int fuse_getattr(const char* path, struct stat* stbuf);
    //static int fuse_getattr(const char* path, char* b, size_t c, off_t d, fuse_file_info* e);
    static int fuse_open(const char* path, struct fuse_file_info* file_i);
    static int fuse_read(const char* path, char *, size_t, off_t, struct fuse_file_info* file_i);
    static int fuse_write(const char* path, const char *, size_t, off_t, struct fuse_file_info* file_i);
    // maybe fuse_release
    static int fuse_create(const char* path, mode_t, struct fuse_file_info* file_i);
    static int fuse_rename(const char* from, const char* to);
    static int fuse_mkdir(const char* path, mode_t);
    static int fuse_rmdir(const char* path);
    static int fuse_readdir(const char* path, void *, fuse_fill_dir_t, off_t, struct fuse_file_info* file_i);
};

