/**
 * @author Martin Pavlásek (xpavla07@stud.fit.vutbr.cz)
 *
 * Created on 14. březen 2012
 */

#include <iostream>
#include <fstream>
#include <dirent.h>
#include <sys/stat.h>
#include <math.h>

#include <cryptopp/sha.h>
#include <cryptopp/filters.h>
#include <cryptopp/hex.h>

#include <taglib/mpegfile.h>
#include <taglib/id3v2tag.h>
#include <taglib/id3v2frame.h>
#include <taglib/generalencapsulatedobjectframe.h>
#include <taglib/urllinkframe.h>

#include "hiddenfs-lib.h"
#include <stdexcept>

class mp3fs : public hiddenFs {
public:
    typedef vector<string> dirlist_t;
    /** množství skrytých dat na jeden fyzický soubor */
    static const float STEGO_RATIO = 0.1;
    static const char PATH_DELIMITER = '/';
    typedef struct {
        TagLib::MPEG::File* file;
        TagLib::ID3v2::Tag* tag;
        int maxBlocks;
        std::set<T_BLOCK_NUMBER> avaliableBlocks;
    } context_t;

protected:
    virtual size_t readBlock(void* context, T_BLOCK_NUMBER block, char* buff, size_t length);
    virtual void writeBlock(void* context, T_BLOCK_NUMBER block, char* buff, size_t length);
    virtual void storageRefreshIndex(string filename);
    virtual void* createContext(string filename);
    virtual void listAvaliableBlocks(void* context, std::set<T_BLOCK_NUMBER>* blocks);
    virtual void freeContext(void* context);
    virtual void removeBlock(void* context, T_BLOCK_NUMBER block);
    virtual void flushContext(void* contextParam) {
        context_t* context = (context_t*) contextParam;
        context->file->save();
    }

    /**
     * Generuje název souboru podel čísla bloku
     * @param block identifikátor bloku
     * @return řetězec pro název souboru v mezi ID3 tagy
     */
    std::string genFilenameByBlock(T_BLOCK_NUMBER block) {
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
    string toLowerString(string input) {
        string output = input;

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
    void scanDir(string path, vector<string> &filter, dirlist_t &output);

};

void mp3fs::listAvaliableBlocks(void* contextParam, std::set<T_BLOCK_NUMBER>* blocks) {
    context_t* context = (context_t*) contextParam;
    *blocks = context->avaliableBlocks;
}

void* mp3fs::createContext(string filename) {
    context_t* context = new context_t;

    context->file = new TagLib::MPEG::File(filename.c_str());

    if(!context->file->isValid()) {
        throw ExceptionFileNotFound(filename);
    }

    context->tag = context->file->ID3v2Tag(true);
    TagLib::ID3v2::FrameList flm = context->tag->frameListMap()["GEOB"];

    struct stat stBuff;
    stat(filename.c_str(), &stBuff);
    int pureContentLength = stBuff.st_size - (flm.size() * BLOCK_MAX_LENGTH);
    context->maxBlocks = floor(pureContentLength * STEGO_RATIO / BLOCK_MAX_LENGTH);

    std::cout << "max blocks [" << filename << "] = " << context->maxBlocks;
    std::cout << ", stat.len=" << stBuff.st_size << ", pure len=" << pureContentLength << "poč. GEOB:" << flm.size() << std::endl;

    return (void*) context;
}

void mp3fs::freeContext(void* contextParam) {
    context_t* context = (context_t*) contextParam;

    this->flushContext(context);

    delete context->tag;
    //delete context->file->~File();
    delete context;
}

void mp3fs::scanDir(string path, vector<string> &filter, dirlist_t &output) {
    // cesta musí vždky končit lomítkem
    if(path[path.length() - 1] != PATH_DELIMITER) {
        path += PATH_DELIMITER;
    }

    DIR* d = opendir(path.c_str());
    struct dirent* item;
    size_t pos;
    string filename, filepath;
    string filterLower, filenameLower;

    //printf("aktualni dir: %s\n", path.c_str());

    while((item = readdir(d))) {
        filename = string(item->d_name);
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
            cout << "nejsou filtry, beru vše." << endl;
            filter.push_back("");
        }

        for(vector<string>::iterator i = filter.begin(); i != filter.end(); i++) {
            filterLower = toLowerString(*i);
            filenameLower = toLowerString(filename);

            pos = filenameLower.rfind(filterLower);

            if(pos != string::npos && pos == (filename.length() - filterLower.length())) {
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

void mp3fs::removeBlock(void* contextParam, T_BLOCK_NUMBER block) {
    context_t* context = (context_t*) contextParam;
    TagLib::ID3v2::GeneralEncapsulatedObjectFrame* geob;
    std::string filename = this->genFilenameByBlock(block);
    TagLib::ID3v2::FrameList flm = context->tag->frameListMap()["GEOB"];

    for(TagLib::ID3v2::FrameList::Iterator i = flm.begin(); i != flm.end(); i++) {
        geob = (TagLib::ID3v2::GeneralEncapsulatedObjectFrame*) (*i);

        if(geob->fileName() == filename) {
            context->tag->removeFrame(geob, true);
            break;
        }
    }

    context->avaliableBlocks.insert(block);
}

size_t mp3fs::readBlock(void* contextParam, T_BLOCK_NUMBER block, char* buff, size_t length) {
    context_t* context = (context_t*) contextParam;
    TagLib::ID3v2::GeneralEncapsulatedObjectFrame* geob;
    std::string filename = this->genFilenameByBlock(block);
    TagLib::ByteVector bv;

    TagLib::ID3v2::FrameList flm = context->tag->frameListMap()["GEOB"];
    /*
    TagLib::ID3v2::GeneralEncapsulatedObjectFrame* fr = new TagLib::ID3v2::GeneralEncapsulatedObjectFrame();
    TagLib::ID3v2::FrameList::Iterator i = flm.find(fr);
    if(flm.end() == i) {
        cout << "tento tag neexistuje" << endl;
        return 0;
    }
    */


    for(TagLib::ID3v2::FrameList::Iterator i = flm.begin(); i != flm.end(); i++) {
        geob = (TagLib::ID3v2::GeneralEncapsulatedObjectFrame*) (*i);

        if(geob->fileName() == filename) {
            bv = geob->object();
            memcpy(buff, bv.data(), bv.size());

            return bv.size();
        }
    }

    throw runtime_error("Block not found.");

    /*
    FrameList::ConstIterator it;
    for(it = tag->frameList().begin(); it != tag->frameList().end(); it++) {
        cout << (*it)->frameID() << " = " << (*it)->toString() << endl;
    }
    */

    //tag->removeFrame(flm.front(), true);

    /*
    TagLib::ID3v2::GeneralEncapsulatedObjectFrame* geob1 = new TagLib::ID3v2::GeneralEncapsulatedObjectFrame();
    geob1->setDescription("1");
    ByteVector bv;
    string s = "asfdasdfasfdasdf";
    bv.setData(s.c_str());
    geob1->setObject(bv);
    //geob1->setFileName("nn");
    //cout << geob1->toString() << endl;
    tag->addFrame(geob1);
    f.save();
    */

    return 0;
}

void mp3fs::writeBlock(void* contextParam, T_BLOCK_NUMBER block, char* buff, size_t length) {
    context_t* context = (context_t*) contextParam;

    TagLib::ID3v2::FrameList flm = context->tag->frameListMap()["GEOB"];
    TagLib::ID3v2::GeneralEncapsulatedObjectFrame* geob;
    TagLib::ID3v2::GeneralEncapsulatedObjectFrame* newGeob = new TagLib::ID3v2::GeneralEncapsulatedObjectFrame;
    TagLib::ByteVector bv;
    std::string filename = this->genFilenameByBlock(block);

    bv.setData(buff, length);

    /// @todo zkusit optimalizovat přes některý z asociativních kontejnerů
    for(TagLib::ID3v2::FrameList::Iterator i = flm.begin(); i != flm.end(); i++) {
        geob = (TagLib::ID3v2::GeneralEncapsulatedObjectFrame*) (*i);

        if(geob->fileName() == filename) {
            geob->setObject(bv);
            context->avaliableBlocks.erase(block);
            return;
        }
    }

    // blok s tímto číslem ještě neexistuje, proto je nutné jej založit ručně
    newGeob->setFileName(filename);
    newGeob->setObject(bv);

    context->avaliableBlocks.erase(block);
}

void mp3fs::storageRefreshIndex(string filename) {
    dirlist_t l;
    ifstream f;

    vector<string> filter;
    filter.push_back(".mp3");

    this->scanDir(filename, filter, l);
    const int N = 500;
    const int offset = 5000;

    int delka;
    long delka_sum = 0;
    char* blok = new char[N+1];

    string source, value;
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

    /*
    cout << "načítám:"; cout.flush();
    TagLib::MPEG::File f("funnyman.mp3");
    cout << "f() hotovo, načítám tagy..."; cout.flush();
    TagLib::ID3v2::Tag* tag = f.ID3v2Tag(true);
    cout << "hotovo..."; cout.flush();
    TagLib::ID3v2::FrameList::ConstIterator it;
    for(it = tag->frameList().begin(); it != tag->frameList().end(); it++) {
        cout << (*it)->frameID() << " = " << (*it)->toString() << endl;
    }
    */

    /*
    TagLib::ID3v2::FrameList flm = tag->frameListMap()["GEOB"];
    for(TagLib::ID3v2::FrameList::Iterator i = flm.begin(); i != flm.end(); i++) {
        tag->removeFrame(*i, true);
        cout << "#"; cout.flush();
    }
    */
    //tag->removeFrame(flm.front(), true);
    /*
    for(int i = 0; i < 0; i++) {
        if(i % 10 == 0) {
            cout << "#";
            cout.flush();
        }
        TagLib::ID3v2::GeneralEncapsulatedObjectFrame* geob1 = new TagLib::ID3v2::GeneralEncapsulatedObjectFrame();
        geob1->setDescription("1");
        TagLib::ByteVector bv;
        string s = "asfdasdfasfdasdf...asfdasdfasfdasdf...asfdasdfasfdasdf...asfdasdfasfdasdf...asfdasdfasfdasdf...asfdasdfasfdasdf...asfdasdfasfdasdf...asfdasdfasfdasdf...asfdasdfasfdasdf...asfdasdfasfdasdf...asfdasdfasfdasdf...asfdasdfasfdasdf...asfdasdfasfdasdf...asfdasdfasfdasdf...asfdasdfasfdasdf...asfdasdfasfdasdf...asfdasdfasfdasdf...asfdasdfasfdasdf...asfdasdfasfdasdf...asfdasdfasfdasdf...asfdasdfasfdasdf...asfdasdfasfdasdf...asfdasdfasfdasdf...asfdasdfasfdasdf...asfdasdfasfdasdf...asfdasdfasfdasdf...56789#asfdasdfasfdasdf...asfdasdfasfdasdf...asfdasdfasfdasdf...asfdasdfasfdasdf...asfdasdfasfdasdf...asfdasdfasfdasdf...asfdasdfasfdasdf...asfdasdfasfdasdf...asfdasdfasfdasdf...asfdasdfasfdasdf...asfdasdfasfdasdf...asfdasdfasfdasdf...asfdasdfasfdasdf...asfdasdfasfdasdf...asfdasdfasfdasdf...asfdasdfasfdasdf...asfdasdfasfdasdf...asfdasdfasfdasdf...asfdasdfasfdasdf...asfdasdfasfdasdf...asfdasdfasfdasdf...asfdasdfasfdasdf...asfdasdfasfdasdf...asfdasdfasfdasdf...asfdasdfasfdasdf...asfdasdfasfdasdf...56789#asdf...56789#..56789#234";
        //s.append(__TIME__);
        bv.setData(s.c_str());
        geob1->setObject(bv);
        geob1->setFileName("nn");
        tag->addFrame(geob1);
    }
    cout << "Mažu:"; cout.flush();
    f.save();
    //f.setID3v2FrameFactory()
    //cout << "destruktor"; cout.flush();
    f.~File();
    */

    int ret;
    mp3fs* fs = new mp3fs;

    if(false) {
        string a[2];
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
