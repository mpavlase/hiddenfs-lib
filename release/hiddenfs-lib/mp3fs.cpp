/**
 * @author Martin Pavlásek (xpavla07@stud.fit.vutbr.cz)
 *
 * Created on 14. březen 2012
 */

#include <dirent.h>
#include <fstream>
#include <iostream>
#include <math.h>
#include <stdexcept>
#include <sys/stat.h>

#include <cryptopp/sha.h>
#include <cryptopp/filters.h>
#include <cryptopp/aes.h>
#include <cryptopp/modes.h>
#include <cryptopp/hex.h>

#include <id3/id3lib_frame.h>
#include <id3/tag.h>
#include <id3/field.h>
#include <id3.h>

#include "hiddenfs-lib.h"

class mp3fs : public HiddenFS::hiddenFs {
public:
    typedef std::vector<std::string> dirlist_t;

    void init() {
    }

    mp3fs(HiddenFS::IEncryption* i) : HiddenFS::hiddenFs(i) {
        this->init();
    };
    mp3fs() : HiddenFS::hiddenFs(NULL) {
        this->init();
    };

    /** množství skrytých dat na jeden fyzický soubor */
    static const float STEGO_RATIO = 0.1;
    static const char PATH_DELIMITER = '/';
    typedef struct {
        ID3_Tag* tag;
    } mp3context;

protected:
    size_t readBlock(HiddenFS::context_t* context, HiddenFS::block_number_t block, HiddenFS::bytestream_t* buff, size_t length) const;
    void writeBlock(HiddenFS::context_t* context, HiddenFS::block_number_t block, HiddenFS::bytestream_t* buff, size_t length);
    void storageRefreshIndex(std::string filename);
    HiddenFS::context_t* createContext(std::string filename);
    void freeContext(HiddenFS::context_t* context);
    void removeBlock(HiddenFS::context_t* context, HiddenFS::block_number_t block);
    void flushContext(HiddenFS::context_t* context) {
        ((mp3context*) context->userContext)->tag->Update();
    }

private:
    /**
     * Převádí vstupní řetězec na malá písmena
     * @param input vstupní řetězec
     * @return
     */
    std::string toLowerString(std::string input) const {
        std::string output = input;

        for(unsigned int i = 0; i < output.length(); i++) {
            output[i] = tolower(output[i]);
        }

        return output;
    }

    /**
     * Rekurzivně prochází adresář a ukládá do výstupu cesty k souborům vyhovující filtru
     * @param path vstupní adresáč
     * @param filterParam seznam filtrů přípon souborů
     * @param output metoda naplní tento seznam vyhovujícími cestami
     */
    void scanDir(std::string path, std::vector<std::string> &filter, dirlist_t &output);

};

HiddenFS::context_t* mp3fs::createContext(std::string filename) {
    // ----------------------------
    HiddenFS::context_t* context = new HiddenFS::context_t;

    std::string tagFilename;
    int blok;
    struct stat stBuff;
    int counter = 0;
    ID3_Frame* first = NULL;
    HiddenFS::block_number_t lastBlock;
    HiddenFS::block_number_t actBlock, diffBlock, iterBlock;
    ID3_Frame* frame;
    ID3_Frame* frameX;
    unsigned int usedSize;

    // výpočet objem rámců, které je možno použít pro uložení datových bloků
    int stat_ret = stat(filename.c_str(), &stBuff);
    if(stat_ret == -1) {
        char* errMsg = strerror(errno);
        std::cerr << errMsg;
        throw HiddenFS::ExceptionRuntimeError(errMsg);
    }

    context->userContext = new mp3context;
    ((mp3context*) context->userContext)->tag = new ID3_Tag((const char*)filename.c_str());

    ID3_Tag::Iterator* itt = ((mp3context*) context->userContext)->tag->CreateIterator();
    size_t sumGEOB = 0;

    while((frameX = itt->GetNext()) != NULL) {
        if(frameX->GetID() == ID3FID_GENERALOBJECT) {
            sumGEOB += frameX->GetField(ID3FN_DATA)->BinSize();
        }
    }

    unsigned int pureContentLength = stBuff.st_size - sumGEOB;
    context->maxBlocks = floor(pureContentLength * STEGO_RATIO / BLOCK_MAX_LENGTH);

    // průchod přes všechny ID3 tagy typu Global Enculapsed Object pro nalezení
    // rámců obsahující fragmenty tohoto FS
    if(sumGEOB > 0) {
        while(true) {
            frame = ((mp3context*) context->userContext)->tag->Find(ID3FID_GENERALOBJECT);

            if(counter == 0) {
                first = frame;
            } else if(first == frame) {
                break;
            }

            counter++;

            // patří nalezený rámec do tohoto FS?
            tagFilename = frame->GetField(ID3FN_FILENAME)->GetRawText();
            if(tagFilename == "") {
                continue;
            }

            std::istringstream(tagFilename) >> blok;
            context->usedBlocks.insert(blok);
        }
    }

    usedSize = context->usedBlocks.size();

    if(!context->usedBlocks.empty()) {
        lastBlock = HiddenFS::FIRST_BLOCK_NO;

        // vyhledání "děr" v číslování, začínat až od dalšího prvku, protože první jsme načetli ručně
        for(std::set<HiddenFS::block_number_t>::iterator setIt = context->usedBlocks.begin(); setIt != context->usedBlocks.end(); setIt++) {
            actBlock = *setIt;
            diffBlock = actBlock - lastBlock;

            if(diffBlock > 1) {
                // dopočítání čísel bloků v mezeře (jsou volnými bloky)
                for(iterBlock = lastBlock; iterBlock != actBlock; iterBlock++) {
                    // nesmíme překročit maximální počet bloků na soubor
                    if(context->avaliableBlocks.size() + context->usedBlocks.size() >= context->maxBlocks) {
                        break;
                    }

                    context->avaliableBlocks.insert(iterBlock);
                }

                // nesmíme překročit maximální počet bloků na soubor
                if(context->avaliableBlocks.size() + context->usedBlocks.size() >= context->maxBlocks) {
                    break;
                }
            }

            lastBlock = actBlock;
        }
    } else {
        lastBlock = HiddenFS::FIRST_BLOCK_NO;

        for(iterBlock = lastBlock; !(context->avaliableBlocks.size() + context->usedBlocks.size() >= context->maxBlocks); iterBlock++) {

            context->avaliableBlocks.insert(iterBlock);
        }
    }

    // máme k dispozici více čísel bloků, než obsahují "díry", takže je dopočítáme ručně
    if(!(context->avaliableBlocks.size() + context->usedBlocks.size() >= context->maxBlocks)) {
        for(iterBlock = lastBlock; !(context->avaliableBlocks.size() + context->usedBlocks.size() >= context->maxBlocks); iterBlock++) {
            if(context->usedBlocks.find(iterBlock) == context->usedBlocks.end()) {
                context->avaliableBlocks.insert(iterBlock);
            }
        }
    }

    return context;
}

void mp3fs::freeContext(HiddenFS::context_t* context) {
    this->flushContext(context);

    delete ((mp3context*) context->userContext)->tag;
    delete (mp3context*) context->userContext;
    delete context;
}

void mp3fs::scanDir(std::string path, std::vector<std::string> &filter, dirlist_t &output) {
    // cesta musí vždky končit lomítkem
    if(path[path.length() - 1] != PATH_DELIMITER) {
        path += PATH_DELIMITER;
    }

    DIR* d = opendir(path.c_str());
    struct dirent* item;
    size_t pos;
    std::string filename, filepath;
    std::string filterLower, filenameLower;

    if(d == NULL) {
        throw HiddenFS::ExceptionRuntimeError("Adresář \"" + path + "\" nelze otevřít při indexaci pro čtení.");
    }

    while((item = readdir(d))) {
        filename = std::string(item->d_name);
        filepath = path + filename;

        if(item->d_type == DT_DIR) {
            if(filename == "." || filename == "..") {
                continue;
            }

            this->scanDir(filepath, filter, output);

            continue;
        }

        if(filter.empty()) {
            std::cout << "nejsou filtry, beru vše.\n";
            filter.push_back("");
        }

        for(std::vector<std::string>::iterator i = filter.begin(); i != filter.end(); i++) {
            filterLower = toLowerString(*i);
            filenameLower = toLowerString(filename);

            pos = filenameLower.rfind(filterLower);

            if(pos != std::string::npos && pos == (filename.length() - filterLower.length())) {
                output.push_back(filepath);

                break;
            }
        }
    }

    closedir(d);
}

void mp3fs::removeBlock(HiddenFS::context_t* context, HiddenFS::block_number_t block) {
    std::stringstream ss;

    ss << block;

    if(context->usedBlocks.find(block) == context->usedBlocks.end()) {
        /// @todo opravdu to je až tak významná chyba?
        throw HiddenFS::ExceptionBlockNotFound("Blok " + ss.str() + " nelze smazat, není obsazen.");
    }

    ID3_Frame* frame = ((mp3context*) context->userContext)->tag->Find(ID3FID_GENERALOBJECT, ID3FN_FILENAME, (const char*) (ss.str().c_str()));
    if(frame == NULL) {
        throw HiddenFS::ExceptionBlockNotFound("Blok " + ss.str() + " nelze smazat - nebyl nalezen v úložišti.");
    }

    ((mp3context*) context->userContext)->tag->RemoveFrame(frame);

    // v případě, že se v průběhu času změnila délka souboru a tím maximální
    // počet bloků, tento blok pouze uvolníme, ale už nebude vložen mezi bloky,
    // které je zase možné použít (tak by došlo k ještě většímu překročení tohoto limitu)
    if(context->usedBlocks.size() <= context->maxBlocks) {
        context->avaliableBlocks.insert(block);
    }
    context->usedBlocks.erase(block);
}

size_t mp3fs::readBlock(HiddenFS::context_t* context, HiddenFS::block_number_t block, HiddenFS::bytestream_t* buff, size_t length) const {
    std::stringstream ss;
    size_t size = 0;

    ss << block;

    assert(buff != NULL);

    if(context->usedBlocks.find(block) == context->usedBlocks.end()) {
        if(context->avaliableBlocks.find(block) != context->avaliableBlocks.end()) {
            throw HiddenFS::ExceptionBlockNotUsed("Do bloku " + ss.str() + " nebylo dosud nic zapsáno, nelze jej proto přečíst.");
        }

        throw HiddenFS::ExceptionBlockNotUsed("Blok " + ss.str() + " nelze přečíst, není obsazen (jiný FS?).");
    }

    ID3_Frame* frame = ((mp3context*) context->userContext)->tag->Find(ID3FID_GENERALOBJECT, ID3FN_FILENAME, (const char*) (ss.str().c_str()));
    if(frame == NULL) {
        throw HiddenFS::ExceptionBlockNotFound("Blok " + ss.str() + " nelze přečíst, nebyl ve fyzickém souboru nalezen.");
    }

    size = (frame->Field(ID3FN_DATA).BinSize() > length) ? length : frame->Field(ID3FN_DATA).BinSize();
    memcpy(buff, frame->Field(ID3FN_DATA).GetRawBinary(), size);

    return size;
}

void mp3fs::writeBlock(HiddenFS::context_t* context, HiddenFS::block_number_t block, HiddenFS::bytestream_t* buff, size_t length) {
    std::stringstream ss;

    ss << block;

    // blok není dosud obsazen (což není není vždy chybou)
    if(context->usedBlocks.find(block) == context->usedBlocks.end()) {
        // ale protože se nenachází mezi dostupnými bloky, jedná se o chybu
        if(context->avaliableBlocks.find(block) == context->avaliableBlocks.end()) {
            throw HiddenFS::ExceptionBlockNotFound("Do bloku " + ss.str() + " nelze zapsat, není označen jako dostupný.");
        }
    }

    ID3_Frame* frame = ((mp3context*) context->userContext)->tag->Find(ID3FID_GENERALOBJECT, ID3FN_FILENAME, (const char*) (ss.str().c_str()));

    // blok s tímto číslem dosud neexistuje, takže jej založíme
    if(frame == NULL) {
        frame = new ID3_Frame();

        frame->SetID(ID3FID_GENERALOBJECT);
        frame->Field(ID3FN_FILENAME).Set((const char*) (ss.str().c_str()));

        // zápis obsahu bloku
        frame->Field(ID3FN_DATA).Set((const unsigned char*)buff, length);

        ((mp3context*) context->userContext)->tag->AddFrame(frame);
    }

    // zapíšeme obsah bloku
    frame->Field(ID3FN_DATA).Set((const unsigned char*)buff, length);

    // odstranit číslo tohoto bloku ze seznamu volných
    context->avaliableBlocks.erase(block);
    context->usedBlocks.insert(block);
}

void mp3fs::storageRefreshIndex(std::string path) {
    dirlist_t listOfFiles;
    std::ifstream f;
    unsigned int fileLength;

    // pro blok pro výpočet hash použít posledních 'blockOffset' bytů od konce MP3 souboru
    const unsigned int blockOffset = 300*1024;

    // počet bytů, ze kterých se následně počítá hash
    const unsigned int blockAmount = 1*1024;
    //const unsigned int blockAmount = 10;

    // = 2MB
    const unsigned int fileMinLength = 2*1024*1024 + blockOffset;

    // konstanty pro detekci a následné přeskočení ID3v1 tagů
    const unsigned int ID3v1_offset = 128;
    const char* ID3v1_identification = "TAG";
    const size_t ID3v1_identificationLen = strlen(ID3v1_identification);
    char id3v1_buffer[3];

    // při nalezení ID3v1 tagů se rovná 128, jinak 0
    unsigned int offsetFromEnd;

    // vyhledání všech souborů s požadovanou příponou
    std::vector<std::string> filter;
    filter.push_back(".mp3");

    // scandir může vyhazovat výjimku při problému se čtením obsahu adresáře
    this->scanDir(path, filter, listOfFiles);

    char* blok = new char[blockAmount+1];

    std::string hashOutput, encoded;
    CryptoPP::SHA1 hash;

    for(dirlist_t::iterator i = listOfFiles.begin(); i != listOfFiles.end(); i++) {
        f.open(i->c_str(), ifstream::binary);
        offsetFromEnd = 0;

        if(f.fail()) {
            printf(" - problém!!\n");
            continue;
        }

        // přečtení celkové délky souboru
        f.seekg(0, ifstream::end);
        fileLength = f.tellg();
        f.seekg(0, ifstream::beg);


        f.seekg(fileLength - ID3v1_offset);
        f.read(id3v1_buffer, ID3v1_identificationLen);

        if(memcmp(id3v1_buffer, ID3v1_identification, ID3v1_identificationLen) == 0) {
            offsetFromEnd = ID3v1_offset;
        }


        if(fileLength > fileMinLength) {
            hashOutput.clear();

            f.seekg(0, ifstream::beg);
            f.seekg(fileLength - blockOffset - offsetFromEnd);
            memset(blok, '\0', blockAmount);
            f.read(blok, blockAmount);

            encoded.clear();
            CryptoPP::StringSource(
                (const byte*)blok,
                blockAmount,
                true,
                new CryptoPP::HexEncoder(
                    new CryptoPP::StringSink(encoded)
                ) // HexEncoder
            ); // StringSource

            CryptoPP::StringSource(
                (const byte*)blok,
                blockAmount,
                true,
                new CryptoPP::HashFilter(
                    hash,
                    new CryptoPP::StringSink(hashOutput)
                )
            );

            HiddenFS::hash_ascii_t h_string;
            HiddenFS::convertHashToAscii(h_string, (HiddenFS::hash_raw_t*) (hashOutput.data()), HiddenFS::hash_raw_t_sizeof);

            this->HT->add(h_string, *i);
         } else {
            std::cout << "Soubor " << *i << " je moc krátký (délka < " << fileMinLength << " Bytes)!\n";
         }

        f.close();
    }

    delete [] blok;
}

int main(int argc, char* argv[]) {
    int ret;
    mp3fs* fs = new mp3fs();

    ret = fs->run(argc, argv);

    delete fs;

    return ret;
}
