/**
 * @author Martin Pavlásek (xpavla07@stud.fit.vutbr.cz)
 *
 * Created on 14. březen 2012
 */

#include <dirent.h>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <math.h>
#include <dirent.h>
#include <sys/stat.h>

#include <cryptopp/sha.h>
#include <cryptopp/filters.h>
#include <cryptopp/hex.h>

/*
#include <taglib/mpegfile.h>
#include <taglib/id3v2tag.h>
#include <taglib/id3v2frame.h>
#include <taglib/generalencapsulatedobjectframe.h>
#include <taglib/urllinkframe.h>
*/

#include <id3/id3lib_frame.h>
#include <id3/tag.h>
#include <id3/field.h>
#include <id3.h>

#include "hiddenfs-lib.h"

class mp3fs : public HiddenFS::hiddenFs {
public:
    typedef std::vector<std::string> dirlist_t;

    /** množství skrytých dat na jeden fyzický soubor */
    static const float STEGO_RATIO = 0.1;
    static const char PATH_DELIMITER = '/';
    static const HiddenFS::T_BLOCK_NUMBER FIRST_BLOCK_NO = 1;
    typedef struct {
        ID3_Tag* tag;
        unsigned int maxBlocks;
        std::set<HiddenFS::T_BLOCK_NUMBER> avaliableBlocks;
        std::set<HiddenFS::T_BLOCK_NUMBER> usedBlocks;
    } context_t;

protected:
    virtual size_t readBlock(void* context, HiddenFS::T_BLOCK_NUMBER block, char* buff, size_t length) const;
    virtual void writeBlock(void* context, HiddenFS::T_BLOCK_NUMBER block, char* buff, size_t length);
    virtual void storageRefreshIndex(std::string filename);
    virtual void* createContext(std::string filename);
    virtual void listAvaliableBlocks(void* context, std::set<HiddenFS::T_BLOCK_NUMBER>* blocks) const;
    virtual void freeContext(void* context);
    virtual void removeBlock(void* context, HiddenFS::T_BLOCK_NUMBER block);
    virtual void flushContext(void* contextParam) {
        context_t* context = (context_t*) contextParam;
        context->tag->Update();
    }

    /**
     * Generuje název souboru podel čísla bloku
     * @param block identifikátor bloku
     * @return řetězec pro název souboru v mezi ID3 tagy
     */
    std::string genFilenameByBlock(HiddenFS::T_BLOCK_NUMBER block) {
        std::stringstream ss;
        ss << "B_";
        ss << block;

        return ss.str();
    }

private:
    /**
     * Convert all chars of string to lowercase
     * @param input input strint
     * @return lowercase string
     */
    std::string toLowerString(std::string input) const {
        std::string output = input;

        for(unsigned int i = 0; i < output.length(); i++) {
            output[i] = tolower(output[i]);
        }

        return output;
    }

    /**
     * Recursive pass through directory and searches files by filter
     * @param path input directory
     * @param filterParam find files only matching for this extensions
     * @param output list of found files
     */
    void scanDir(std::string path, std::vector<std::string> &filter, dirlist_t &output);

};

void mp3fs::listAvaliableBlocks(void* contextParam, std::set<HiddenFS::T_BLOCK_NUMBER>* blocks) const {
    context_t* context = (context_t*) contextParam;
    *blocks = context->avaliableBlocks;
}

void* mp3fs::createContext(std::string filename) {
    // ----------------------------
    context_t* context = new context_t;

    std::string tagFilename;
    int blok;
    struct stat stBuff;
    int counter = 0;
    ID3_Frame* first;
    HiddenFS::T_BLOCK_NUMBER lastBlock;
    HiddenFS::T_BLOCK_NUMBER actBlock, diffBlock, iterBlock;
    ID3_Frame* frame;
    ID3_Frame* frameX;
    unsigned int usedSize;

    context->tag = new ID3_Tag((const char*)filename.c_str());

    ID3_Tag::Iterator* itt = context->tag->CreateIterator();
    size_t sumGEOB = 0;

    while((frameX = itt->GetNext()) != NULL) {
        //std::cout << "rámec: " << frameX->GetTextID();
        if(frameX->GetID() == ID3FID_GENERALOBJECT) {
            sumGEOB += frameX->GetField(ID3FN_DATA)->BinSize();
            //std::cout << " obsah GEOBu: " << frameX->GetField(ID3FN_DATA)->GetRawBinary() << "\n ";
            //std::cout << "id=" << ", size=" << frameX->GetField(ID3FN_DATA)->BinSize() << ", blokNo=" << frameX->GetField(ID3FN_FILENAME)->GetRawText() << "\n";
        }
        //std::cout << "\n";
    }

    // výpočet objem rámců, které je možno použít pro uložení datových bloků
    stat(filename.c_str(), &stBuff);
    int pureContentLength = stBuff.st_size - sumGEOB;
    context->maxBlocks = floor(pureContentLength * STEGO_RATIO / BLOCK_MAX_LENGTH);

    // průchod přes všechny ID3 tagy typu Global Enculapsed Object pro nalezení
    // rámců obsahující fragmenty tohoto FS
    while(true) {
        frame = context->tag->Find(ID3FID_GENERALOBJECT);

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
        //std::cout << "GEOB[" << blok << "]: " << frame->GetField(ID3FN_FILENAME)->GetRawText() << ", " << frame->GetField(ID3FN_DATA)->GetRawBinary() << "\n";
        //std::cout << "a_" << tagFilename << "\n";
    }


    if(!context->usedBlocks.empty()) {
        lastBlock = FIRST_BLOCK_NO;
        usedSize = context->usedBlocks.size();

        // vyhledání "děr" v číslování, začínat až od dalšího prvku, protože první jsme načetli ručně
        for(std::set<HiddenFS::T_BLOCK_NUMBER>::iterator setIt = context->usedBlocks.begin(); setIt != context->usedBlocks.end(); setIt++) {
            actBlock = *setIt;
            diffBlock = actBlock - lastBlock;

            std::cout << "#[" << actBlock << "]\t";

            if(diffBlock != 1) {
                assert(diffBlock > 0);

                // dopočítání čísel bloků v mezeře (jsou volnými bloky)
                for(iterBlock = lastBlock; iterBlock != actBlock; iterBlock++) {
                    // nesmíme překročit maximální počet bloků na soubor
                    if(context->avaliableBlocks.size() + usedSize >= context->maxBlocks) {
                        break;
                    }

                   std::cout << ".[" << iterBlock << "]\t";
                    context->avaliableBlocks.insert(iterBlock);
                }

                // nesmíme překročit maximální počet bloků na soubor
                if(context->avaliableBlocks.size() + usedSize >= context->maxBlocks) {
                    break;
                }
            }

            lastBlock = actBlock;
        }
    } else {
        lastBlock = FIRST_BLOCK_NO;
    }

    // máme k dispozici více čísel bloků, než obsahují "díry", takže je dopočítáme ručně
    if(!(context->avaliableBlocks.size() + usedSize >= context->maxBlocks)) {
        for(iterBlock = lastBlock; !(context->avaliableBlocks.size() + usedSize >= context->maxBlocks); iterBlock++) {

            std::cout << ".[" << iterBlock << "]\t";
            context->avaliableBlocks.insert(iterBlock);
        }
    }

    std::cout << "\n\nfile size: " << stBuff.st_size << ", sum geob: " << sumGEOB << ", blockLen = " << BLOCK_MAX_LENGTH;
    std::cout << ", poč.avaliable: " << context->avaliableBlocks.size();
    std::cout << ", max bloku: " << context->maxBlocks << ", poč. volných bloků:" << context->avaliableBlocks.size() << " <----------\n";

    std::cout << "used = ";
    for(std::set<HiddenFS::T_BLOCK_NUMBER>::iterator setIt = context->usedBlocks.begin(); setIt != context->usedBlocks.end(); setIt++) {
        std::cout << *setIt << ", ";
    }

    std::cout << "\navaliable = ";

    for(std::set<HiddenFS::T_BLOCK_NUMBER>::iterator setIt = context->avaliableBlocks.begin(); setIt != context->avaliableBlocks.end(); setIt++) {
        std::cout << *setIt << ", ";
    }

    std::cout << "\n";

    return (void*) context;
}

void mp3fs::freeContext(void* contextParam) {
    context_t* context = (context_t*) contextParam;

    this->flushContext(context);

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

    //printf("aktualni dir: %s\n", path.c_str());

    while((item = readdir(d))) {
        filename = std::string(item->d_name);
        filepath = path + filename;

        if(item->d_type == DT_DIR) {
            if(filename == "." || filename == "..") {
                continue;
            }

            dirlist_t d;
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
                //cout << path  << filename << " ...ok" << endl;
                output.push_back(filepath);
                break;
            } else {
                //cout << path << filenameLower << " ## mě nezajímá" <<  endl;
            }
        }
    }

    closedir(d);
}

void mp3fs::removeBlock(void* contextParam, HiddenFS::T_BLOCK_NUMBER block) {
    context_t* context = (context_t*) contextParam;
    std::stringstream ss;

    ss << block;

    if(context->usedBlocks.find(block) == context->usedBlocks.end()) {
        /// @todo opravdu to je až tak významná chyba?
        throw HiddenFS::ExceptionBlockNotFound("Blok " + ss.str() + " nelze smazat, není obsazen.");
    }

    ID3_Frame* frame = context->tag->Find(ID3FID_GENERALOBJECT, ID3FN_FILENAME, (const char*) (ss.str().c_str()));
    if(frame == NULL) {
        throw HiddenFS::ExceptionBlockNotFound("Blok " + ss.str() + " nelze smazat - nebyl nalezen v úložišti.");
    }

    context->tag->RemoveFrame(frame);

    // v případě, že se v průběhu času změnila délka souboru a tím maximální
    // počet bloků, tento blok pouze uvolníme, ale už nebude vložen mezi bloky,
    // které je zase možné použít (tak by došlo k ještě většímu překročení tohoto limitu)
    if(context->usedBlocks.size() <= context->maxBlocks) {
        context->avaliableBlocks.insert(block);
    }
    context->usedBlocks.erase(block);
}

size_t mp3fs::readBlock(void* contextParam, HiddenFS::T_BLOCK_NUMBER block, char* buff, size_t length) const {
    context_t* context = (context_t*) contextParam;
    std::stringstream ss;
    size_t size = 0;

    ss << block;

    if(context->usedBlocks.find(block) == context->usedBlocks.end()) {
        /// @todo opravdu to je tak významná chyba?
        throw HiddenFS::ExceptionBlockNotFound("Blok " + ss.str() + " nelze přečíst, není obsazen.");
    }

    ID3_Frame* frame = context->tag->Find(ID3FID_GENERALOBJECT, ID3FN_FILENAME, (const char*) (ss.str().c_str()));
    if(frame == NULL) {
        throw HiddenFS::ExceptionBlockNotFound("Blok " + ss.str() + " nelze přečíst, nebyl nalezen.");
    }

    size = (frame->Field(ID3FN_DATA).BinSize() > length) ? length : frame->Field(ID3FN_DATA).BinSize();
    memcpy(buff, frame->Field(ID3FN_DATA).GetRawBinary(), size);

    return size;
}

void mp3fs::writeBlock(void* contextParam, HiddenFS::T_BLOCK_NUMBER block, char* buff, size_t length) {
    context_t* context = (context_t*) contextParam;
    std::stringstream ss;

    ss << block;

    // blok není dosud obsazen (což není není vždy chybou)
    if(context->usedBlocks.find(block) == context->usedBlocks.end()) {
        // ale protože se nenachází mezi dostupnými bloky, jedná se o chybu
        if(context->avaliableBlocks.find(block) == context->avaliableBlocks.end()) {
            throw HiddenFS::ExceptionBlockNotFound("Do bloku " + ss.str() + " nelze zapsat, není označen jako dostupný.");
        }
    }

    ID3_Frame* frame = context->tag->Find(ID3FID_GENERALOBJECT, ID3FN_FILENAME, (const char*) (ss.str().c_str()));

    // blok s tímto číslem dosud neexistuje, takže jej založíme
    if(frame == NULL) {
        frame = new ID3_Frame();

        frame->SetID(ID3FID_GENERALOBJECT);
        frame->Field(ID3FN_FILENAME).Set((const char*) (ss.str().c_str()));

        // zápis obsahu bloku
        frame->Field(ID3FN_DATA).Set((const unsigned char*)buff, length);

        context->tag->AddFrame(frame);
    }

    // zapíšeme obsah bloku
    frame->Field(ID3FN_DATA).Set((const unsigned char*)buff, length);

    // odstranit číslo tohoto bloku ze seznamu volných
    context->avaliableBlocks.erase(block);
    context->usedBlocks.insert(block);
}

void mp3fs::storageRefreshIndex(std::string filename) {
    dirlist_t l;
    std::ifstream f;

    std::vector<std::string> filter;
    filter.push_back(".mp3");

    this->scanDir(filename, filter, l);
    const int N = 500;
    const int offset = 5000;

    int delka;
    long delka_sum = 0;
    char* blok = new char[N+1];

    std::string source, value;
    CryptoPP::SHA1 hash;

    for(dirlist_t::iterator i = l.begin(); i != l.end(); i++) {
        f.open(i->c_str(), ifstream::binary);

        if(f.fail()) {
            printf(" - problém!!\n");
            continue;
        }
        //f.seekg(ifstream::end);
        f.seekg(0, ifstream::end);
        delka = f.tellg();

        delka_sum += delka;

        if(delka > (offset + N)) {
            //f.seekg(fstream::beg + offset);
            value.clear();
            f.seekg(offset);
            f.read(blok, N);

            source = *i;
            //StringSource(source, true, new HashFilter(hash, new HexEncoder(new StringSink(value))));

            this->HT->add(source, value);
         }

        f.close();
    }

    delete [] blok;
}

int main(int argc, char* argv[]) {
    int ret;
    mp3fs* fs = new mp3fs;

    if(false) {
        std::string a[2];
        a[0] = "-d";
        a[1] = "vfs/";
        const char* argv2[] = {a[0].c_str(), a[1].c_str()};
        int argc2 = 3;
        ret = fs->run(argc2, (char**)argv2);
    } else {
        ret = fs->run(argc, argv);
    }

    delete fs;

    return ret;
}
