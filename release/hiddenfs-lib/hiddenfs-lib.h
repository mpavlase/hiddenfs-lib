/**
 * @author Martin Pavlásek (xpavla07@stud.fit.vutbr.cz)
 *
 * Created on 14. březen 2012
 */


#define FUSE_USE_VERSION 26
#include <fuse.h>
#include <string>
#include <iostream>

#include "common.h"
#include "contentTable.h"
#include "hashTable.h"
#include "structTable.h"
#include "superBlock.h"
#include "types.h"

#define GET_INSTANCE (hiddenFs*) fuse_get_context()->private_data

//#define BLOCK_LENGH (1 << 10)

namespace HiddenFS {

    class hiddenFs {
    public:
        /**
         * Umožňuje nastavit vlastní implementaci šifrování
         * @param instance instance objektu implementující rozhraní IEncryption
         */
        hiddenFs(IEncryption* instance);
        virtual ~hiddenFs();

        /** datový typ kontrolního součtu */
        typedef CRC::CRC_t checksum_t;

        static const size_t BLOCK_USABLE_LENGTH = BLOCK_MAX_LENGTH - sizeof(checksum_t) - sizeof(id_byte_t);

        /** maximální využití každého fyzického souboru (více virtuálních souborů v rámci jednoho fyzického) */
        static const char ALLOCATOR_SPEED = 1 << 0;

        /** zamezit míchání více bloků virtuálních souborů v rámci jednoho skutečného */
        static const char ALLOCATOR_SECURITY = 1 << 1;

        /** uložení jednoho bloku se provede několikanásobně do různých fyzických souborů */
        static const char ALLOCATOR_REDUNDANCY = 1 << 7;

        static void usage(std::ostream& s) {
            s << "Dostupné parametry:\n";
            s << "\t-c\tvytvoří nový systém souborů chráněný heslem\n";
            s << "\t-sPATH\tnastaví cestu k úložišti na PATH\n";
            s << "\n";
        }

        /**
         * Nastaví preferovanou strategii pro alokování nových bloků
         * @param strategy typ použité strategie (enum)
         * @param redundancy počet kopií stejného fragmentu
         * @todo přes bitové operace nastavit další vlastnosti (redundancy)
         */
        void allocatorSetStrategy(unsigned char strategy, unsigned int redundancy);

        /**
         * Vrací aktuálně použitou strategii alokace
         * @deprecated
         * @return hodnotu z enumu ALLOC_METHOD
         */
        //char allocatorGetStrategy();

        /**
         * Hlavní výkonná metoda (wrapper nad fuse_main)
         * @param argc počet argumentů z příkazové řádky
         * @param argv pole argumentů z příkazové řádky
         * @return návratová hodnota z FUSE
         */
        int run(int argc, char* argv[]);

        static std::string storagePath;

        enum {
            KEY_HELP,
            KEY_CREATE
        };

    protected:
        contentTable* CT;
        structTable* ST;
        hashTable* HT;
        superBlock* SB;
        unsigned char allocatorFlags;

        /** míra duplikace dat */
        unsigned int allocatorRedundancy;

        /** Seznam všech hash, kde se vykytují kopie superbloků */
        std::set<hash_ascii_t> superBlockLocations;

        /** instance objektu pro výpočet kontrolního součtu */
        CRC* checksum;

        IEncryption* encryption;

        struct optionsStruct {
            char* storagePath;
        };

        struct optionsStruct options;

        /**
         * Vyhledá (=naalokuje) vhodný počet bloků a zapíše do nich obsah jednotlivých fragmentů
         * @param inode inode ukládaného souboru
         * @param buffer obsah souboru
         * @param length délka bufferu
         * @return počet skutečně zapsaných bytů
         */
        size_t allocatorAllocate(inode_t inode, bytestream_t* buffer, size_t length);

        /**
         * Skrze parametr block vrací právě jeden volný blok na základě kritéra:
         * Blok se musí nacházet v souboru určeném hashem. Pokud takový blok neexistuje, vyhazuje se výjimka ExceptionBlockNotFound,
         * @param block metoda naplní údaje o nalezeném bloku
         * @param hash nový blok hledat v tomto souboru
         * @throw ExceptionBlockNotFound pokud v hashi už není žádný volný blok
         */
        void allocatorFindFreeBlock_sameHash(vBlock*& block, hash_ascii_t hash);

        /**
         * Skrze parametr block vrací právě jeden volný blok na základě kritéra:
         * Blok se nesmí nacházet v souboru určeném hashem. Pokud takový blok neexistuje, vyhazuje se výjimka ExceptionBlockNotFound,
         * @param block metoda naplní údaje o nalezeném bloku
         * @param excluded seznam fyzických souborů, které se nesmí použít
         */
        void allocatorFindFreeBlock_differentHash(vBlock*& block, std::set<hash_ascii_t>& excluded);

        /**
         * Skrze parametr block vrací právě jeden volný blok (jakýkoli volný blok).
         * Pokud takový blok neexistuje, vyhazuje se výjimka ExceptionDiscFull.
         * @param block metoda naplní údaje o nalezeném bloku
         */
        void allocatorFindFreeBlock_any(vBlock*& block);

        /**
         * Skrze parametr block vrací právě jeden volný blok. Pokusí se nejprve
         * vyhledat v preferovaném souboru (hash) a pokud neuspěje, vyhledá jakýkoli jiný volný blok
         * @param block metoda naplní údaje o nalezeném bloku
         * @param prefered hledání se spouští nejprve na souboru určeném tímto hash
         * @param excluded seznam fyzických souborů, které se nesmí použít
         */
        void allocatorFindFreeBlock_any(vBlock*& block, hash_ascii_t prefered, std::set<hash_ascii_t>& excluded);

        /**
         * Vyhledá volný blok v částečně obsazeném fyzickém souboru.
         * @param block metoda naplní údaje o nalezeném bloku
         * @param excluded seznam fyzických souborů, které se nesmí použít
         * @throw ExceptionBlockNotFound pokud takový blok neexistuje
         */
        void allocatorFindFreeBlock_partUsed(vBlock*& block, std::set<hash_ascii_t>& excluded);

        /**
         * Vyhledá volný blok v částečně obsazeném fyzickém souboru.
         * Pokud takový blok neexistuje, zkouší se vyhledat v dosud neobsazených fyzických souborech.
         * @param block metoda naplní údaje o nalezeném bloku
         * @param excluded seznam fyzických souborů, které se nesmí použít
         * @throw ExceptionDiscFull pokud není k dispozici ani částečně obsazený blok, ani žádný ze zcela volných
         */
        void allocatorFindFreeBlock_partUsedPreferred(vBlock*& block, std::set<hash_ascii_t>& excluded);

        /**
         * Vyhledá volný blok ve fyzickém souboru, který ještě neobsahuje žádný
         * obsazený blok a naplní tak parametr, který metoda sama alokuje.
         * @param block metoda naplní údaje o nalezeném bloku do tohoto parametru
         * @throw ExceptionBlockNotFound pokud takový blok neexistuje
         */
        void allocatorFindFreeBlock_unused(vBlock*& block);

        /**
         * Vyhledá (podle potřeby i naalokuje) blok, který dosud není použitý.
         * Pokud žádný takový neexistuje, vrací jakýkoli ještě dostupný blok.
         * @param block metoda naplní údaje o nalezeném bloku do tohoto parametru
         * @throw ExceptionDiscFull pokud není k dispozici vůbec žádný volný blok
         */
        void allocatorFindFreeBlock_unusedPreferred(vBlock*& block);

        /**
         * Prozkoumá volné bloky v rámci hash a naplní parametr block prvním dostupným blokem.
         * Naplní složky: hash, block, used. Zbylé (fragment a length) inicializuje
         * (obvykle na 0, pro další použití je nutné nastavit na smysluplné hodnoty).
         * @param hash kde se mají hledat volné bloky
         * @param block skrze tento parametr metoda vrací info o volném bloku,
         * metoda sama alokuje tento ukazatel
         */
        void allocatorFillFreeBlockByHash(hash_ascii_t hash, vBlock*& block);

        /**
         * Vyhledá a vrací první rezervovaný blok nehledě na to, kterému inode patří
         * @param inode identifikace okradeného souboru
         * @param block lokalizace rezervovaného bloku
         * @throw ExceptionDiscFull pokud se v systému nenalézá žádný rezervovaný blok
         */
        void allocatorStealReservedBlock(inode_t& inode, vBlock*& block);

        /**
         * Naalokuje zdroje pro práci se skutečným souborem - otevření souboru, různé inicializace...
         * @param filename adresa ke skutečnému souboru
         * @return ukazatel na kontext, který může být libovolného typu
         */
        virtual context_t* createContext(std::string filename) = 0;

        /**
         * Pokusí se zcela odstranit zadaný blok z fyzického souboru
         * @param hash jednoznačná identifikace souboru, ve kterém se nachází
         * blok pro mazání
         * @param block číslo bloku ke smazání
         */
        void removeBlock(hash_ascii_t hash, block_number_t block);

        /**
         * Pokusí se zcela odstranit zadaný blok z fyzického souboru
         * @param context ukazatel na kontext, v rámci kterého se pracuje s právě jedním skutečným souborem
         * @param block číslo bloku ke smazání
         */
        virtual void removeBlock(context_t* context, block_number_t block) = 0;

        /**
         * Uvolní naalokované prostředky pro skutečný soubor
         * @param context ukazatel na kontext, v rámci kterého se pracuje s právě jedním skutečným souborem
         */
        virtual void freeContext(context_t* context) = 0;

        /**
         * Zapíše změny do skutečného souboru
         * @param context ukazatel na kontext, v rámci kterého se pracuje s právě jedním skutečným souborem
         */
        virtual void flushContext(context_t* context) = 0;

        /**
         * Zabezpečuje přečtení bloku obsahující fragment souboru s kontrolou
         * kontrolního součtu a načtení identifikačního byte.
         * @param hash identifikace souboru
         * @param block pořadové číslo bloku v rámci skutečného souboru
         * @param buff užitný obsah bloku, metoda sama alokuje dostatečnou délku
         * @param length maximální délka bloku, která se má načíst
         * @param idByte metoda naplní tento parametr hodnotou rozlišovacího byte
         * @return počet skutečně naštených dat
         */
        size_t readBlock(hash_ascii_t hash, block_number_t block, bytestream_t** buff, size_t length, id_byte_t* idByte);

        /**
         * Provádí zápis bloku do skutečného souboru,
         * jeho implementace je na uživateli knihovny
         * @param context ukazatel na kontext, v rámci kterého se pracuje s právě jedním skutečným souborem
         * @param block pořadové číslo bloku v rámci souboru
         * @param buff obsah bloku
         * @param length maximální délka bloku
         * @return skutečně načtená délka bloku
         */
        virtual size_t readBlock(context_t* context, block_number_t block, bytestream_t* buff, size_t length) const = 0;

        /**
         * Provádí zápis bloku do skutečného souboru
         * @param hash hash skutečného souboru, do kterého se má blok schovat
         * @param block pořadové číslo bloku v rámci souboru
         * @param buff obsah bloku
         * @param length délka zapisovaného bloku
         * @throw ExceptionRuntimeError pokud je buffer příliš dlouhý
         */
        void writeBlock(hash_ascii_t hash, block_number_t block, bytestream_t* buff, size_t length, id_byte_t idByte);

        /**
         * Provádí zápis bloku do skutečného souboru,
         * jeho implementace je na uživateli knihovny
         * @param context ukazatel na kontext, v rámci kterého se pracuje s právě jedním skutečným souborem
         * @param block pořadové číslo bloku v rámci souboru
         * @param buff obsah bloku
         * @param length délka zapisovaného bloku
         */
        virtual void writeBlock(context_t* context, block_number_t block, bytestream_t* buff, size_t length) = 0;

        /**
         * Naplní hash tabulku (mapování cesty skutečného souboru na jednoznačný řetězec)
         * @param filename umístění adresáře určeného jako úložiště (storage)
         */
        virtual void storageRefreshIndex(std::string filename) = 0;

        /**
         * Prochází hash table za účelem nalezení dostatečného počtu doposud
         * neobsazených skutečných souborů (konstanta HT_UNUSED_CHUNK)
         */
        void findUnusedHash();

        /**
         * Prochází hash table za účelem nalezení dostatečného počtu doposud
         * neobsazených skutečných souborů (konstanta HT_PARTUSED_CHUNK)
         */
        void findPartlyUsedHash();

        /**
         * Systematicky prohledává hash table za účelem naplnění některého ze seznamů
         * @param list seznam, který chceme naplnit
         * @param limit množství zaplnění parametru list
         */
        void findHashByAuxList(std::set<hash_ascii_t>& list, const unsigned int limit);

        /**
         * Prochází hash table za účelem nalezení dostatečného počtu doposud
         * neobsazených skutečných souborů (konstanta HT_PARTUSED_CHUNK)
         */
        //void findPartUsedHash();

        /**
         * Přečte kompletní obsah virtuálního souboru ze všech fragmentů
         * @param inode inode souboru
         * @param buffer obsah souboru (metoda sama alokuje obsah)
         * @param length délka souboru v bytech
         */
        void getContent(inode_t inode, bytestream_t** buffer, size_t* length);

        /**
         * Provede "zabalení" bloku dat a výpočet CRC
         * @param input vstupní buffer
         * @param inputSize délka vstupního bufferu
         * @param output výstupní buffer, metoda sama alokuje potřebnou délku
         * @param outputSize délka výstupního bufferu
         * @param id_byte identifikační byte (pro odlišení superbloku od běžného)
         * @throw ExceptionRuntimeError pokud se snažíme zapsat více dat,
         * než kolik se vejde do bloku (inputSize je menší BLOCK_USABLE_LENGTH)
         */
        void packData(bytestream_t* input, size_t inputSize, bytestream_t** output, size_t* outputSize, id_byte_t id_byte);

        /**
         * Vstupní data rozdělí na CRC část a zbytek, porovná kontrolní součty
         * (v případě nesrovnalostí vyhazuje výjimku XYZ) a výsledek předá
         * do výstupného bufferu
         * @param input vstupní buffer
         * @param inputSize délka vstupního bufferu
         * @param output výstupní buffer, metoda sama alokuje potřebnou délku
         * @param outputSize délka výstupního bufferu
         * @param id_byte metoda naplní tento parametr hodnotou identifikačního byte
         * @throw ExceptionRuntimeError
         */
        void unpackData(bytestream_t* input, size_t inputSize, bytestream_t** output, size_t* outputSize, id_byte_t* id_byte);

        /**
         * Provádí zápis chainListu, je vlastně trochu jiným druhem alokátoru
         * @param chain vstupní seznam entit určených pro zápis
         * @param firstChain pokusit se zapsat od tohoto článku a pokud jsou dostupné
         * i následující, zapisovat do nich namísto nového výběru bloku. Pokud je NULL,
         * automaticky se vybere vhodný blok.
         * @return ukazatel na vBlock, který obsahuje první článek řetězu
         */
        vBlock* chainListCompleteSave(const chainList_t& chain, vBlock* firstChain = NULL);

        /**
         * Načte část řetězu entit do 'chain', která se nachází v 'location'
         * @param location umístění jednoho článku řetězu pro načtení
         * @param next naplní ukazatel dalšího článku řetězu
         * @param chain metoda rozšíří řetěz entitami o další obsah, nemaže původní obsah
         * @throw ExceptionRuntimeError pokud čtení z bloku 'location' nebylo
         * z nějakého důvodu korektní
         */
        void chainListRestore(vBlock* location, vBlock*& next, chainList_t& chain);

        /**
         * Pokusí se načíst kompletní řetěz entit
         * @param first blok s prvním článkem řetězu
         * @param chain metoda naplní řetěz entitami
         * @throw ExceptionRuntimeError pokud nebylo možné načíst řetěz kompletní
         */
        void chainListCompleteRestore(vBlock* first, chainList_t& chain);

        /**
         * Uloží řetěz do bloku určeném parametrem location. Výsledný bytestream
         * nesmí být delší, než je využitelný obsah bloku.
         * @param location cílový soubor pro zápis
         * @param next ukazatel na následující blok (může být i NULL)
         * @param chain vstupní řetěz k uložení
         */
        void chainListSave(vBlock* location, vBlock* next, const chainList_t& chain);

        /**
         * Vyhledá volný blok podle trochu jiných kritérií, než se používá v allocatorAllocate
         * @param hash identifikace fyzického souboru, který se má upřednostnit při hledání
         * @param output metoda naplní tento ukazatel volným blokem
         * @throw ExceptionDiscFull pokud není v systému absolutně žádný další volný blok
         */
        void chainListAllocate(const hash_ascii_t& hash, vBlock*& output);

        /**
         * Serializuje všechny tabulky (CT, ST), uloží všechny jejich kopie
         * a vloží začátky řetězců do superbloku.
         */
        void tablesSaveToSB();

        /**
         * Uloží řetěz entit do několika různých kopií (počet podle konstanty
         * TABLES_REDUNDANCY_AMOUNT) a pokud už jsou známy dříve obsazené bloky
         * tímto obsahem, upřednostnit je při ukládání.
         * @param content kompletní řetězec pro uložení
         * @param copies seznam vBlock(ů) s prvními články řetězu; pokud je prázdný,
         * metoda sama alokuje první články a naplní jimi tento parametr
         */
        void tableSave(chainList_t& content, std::set<vBlock*>& copies);

        /**
         * Komplexní metoda pro uložení superbloku: provádí serializaci,
         * zápis do stávajících kopií superbloků a podle potřeby doalokování
         * chybějících kopií.
         */
        void superBlockSave();

        // ---------------------------------------------------------------------
        // -----------------------     FUSE     --------------------------------
        // ---------------------------------------------------------------------

        static int fuse_getattr(const char* path, struct stat* stbuf);
        static int fuse_open(const char* path, struct fuse_file_info* file_i);
        static int fuse_read(const char* path, char* buffer, size_t size, off_t length, struct fuse_file_info* file_i);
        static int fuse_write(const char* path, const char* buffer, size_t size, off_t offset, struct fuse_file_info* file_i);
        // možná ještě fuse_release
        static int fuse_create(const char* path, mode_t mode, struct fuse_file_info* file_i);
        static int fuse_rename(const char* from, const char* to);
        static int fuse_mkdir(const char* path, mode_t mode);
        static int fuse_rmdir(const char* path);
        static int fuse_readdir(const char* path, void* buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* file_i);
        static int fuse_truncate(const char* path, off_t length);
    };
}
