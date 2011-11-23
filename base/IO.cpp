#include "IO.h"

//static method
FileReader* FileReader::open(const char* fileName){
    FileReader * fr = NULL;
    switch(FileReader::checkFileType(fileName)) {
    case PLAIN:
        fr = new PlainFileReader(fileName);
        break;
    case GZIP:
        fr = new GzipFileReader(fileName);
        break;
    case BZIP2:
        fr = new Bzip2FileReader(fileName);
        break;
    default:
        fprintf(stderr, "Cannot detect file type (even not text file?!\n");
        break;
    }
    return fr;
}
// static method
void FileReader::close(FileReader** f) {
    assert(f && *f);
    (*f)->close();
    delete (*f);
    *f = NULL;
};

// check header for known file type
FileReader::FileType FileReader::checkFileType(const char* fileName){
    // treat stdin as plain text file
    if (strncmp(fileName, "-", 1) == 0) {
        return PLAIN;
    }
    // magic numbers
    const int gz_magic[2] = {0x1f, 0x8b}; /* gzip magic header */
    const int bzip2_magic[2] = {'B', 'Z'}; /* bzip2 magic header */
    // read file header    
    FILE* fp = fopen(fileName, "rb");
    if (!fp) return UNKNOWN;
    unsigned char header[2]={0,0};
    int n = fread(header, sizeof(char), 2, fp);
    fclose(fp);
    // check file types
    if ( n >= 2 && header[0] == gz_magic[0] && header[1] == gz_magic[1]) {
        return GZIP;
    }
    if ( n >= 2 && header[0] == bzip2_magic[0] && header[1] == bzip2_magic[1]) {
        return BZIP2;
    }
    return PLAIN;
    /* // check the characters fall into visible ASCII range */
    /* if ( header[0] >= 0x20 /\* space *\/ && */
    /*      header[0] <  0x7f /\* DEL *\/   && */
    /*      header[1] >= 0x20 /\* space *\/ && */
    /*      header[1] <  0x7f /\* DEL *\/) { */
    /*     return PLAIN; */
    /* }  */
    /* return UNKNOWN; */
};

AbstractFileWriter::~AbstractFileWriter() {
#ifdef IO_DEBUG
    fprintf(stderr, "AbstractFileWriter desc()\n");
#endif
};
