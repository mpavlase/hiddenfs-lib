/**
 * @author Martin Pavlásek (xpavla07@stud.fit.vutbr.cz)
 *
 * Created on 14. březen 2012
 */

#include <iostream>
#include <fstream>
#include "hiddenfs-lib.h"
//#include "taglib-1.7/include/taglib.h"
#include <dirent.h>
#include <cryptopp/sha.h>
#include <cryptopp/filters.h>
#include <cryptopp/hex.h>

using namespace CryptoPP;

class mp3fs : public hiddenFs {
public:
    typedef vector<string> dirlist_t;
    static const char PATH_DELIMITER = '/';

protected:
    virtual size_t readBlock(string filename, T_BLOCK_NUMBER block, char* buff, size_t length) throw (ExceptionBlockOutOfRange);
    virtual void writeBlock(string filename, T_BLOCK_NUMBER block, char* buff, size_t length);
    virtual void storageRefreshIndex(string filename);
private:
    /**
     * Convert all chars of string to lowercase
     * @param input input strint
     * @return lowercase string
     */
    string toLowerString(string input);

    /**
     * Recursive pass through directory and searches files by filter
     * @param path input directory
     * @param filterParam find files only matching for this extensions
     * @param output list of found files
     */
    void scanDir(string path, vector<string> &filter, dirlist_t &output);

};

string mp3fs::toLowerString(string input) {
    string output = input;

    for(unsigned int i = 0; i < output.length(); i++) {
        output[i] = tolower(output[i]);
    }

    return output;
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

size_t mp3fs::readBlock(string filename, T_BLOCK_NUMBER block, char* buff, size_t length) throw (ExceptionBlockOutOfRange) {
    return 0;
}
void mp3fs::writeBlock(string filename, T_BLOCK_NUMBER block, char* buff, size_t length) {}

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
    SHA1 hash;

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
            StringSource(source, true, new HashFilter(hash, new HexEncoder(new StringSink(value))));

            this->HT->add(source, value);
        }

        f.close();
    }

    delete [] blok;
}

int main(int argc, char* argv[]) {
    mp3fs* fs = new mp3fs;
    int ret = fs->run(argc, argv);

    delete fs;

    return ret;
}
