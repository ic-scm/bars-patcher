//BARS patcher for updating BWAV headers
//Copyright (C) 2020 Extrasklep
//This code is public domain

#include <iostream>
#include <fstream>
#include <cstring>

unsigned char* slice;
char* slicestring;

long clamp(long value, long min, long max) {
  return value <= min ? min : value >= max ? max : value;
}

//Get slice of data
unsigned char* getSlice(const unsigned char* data,unsigned long start,unsigned long length) {
    delete[] slice;
    slice = new unsigned char[length];
    for(unsigned int i=0;i<length;i++) {
        slice[i]=data[i+start];
    }
    return slice;
}

//Get slice and convert it to a number
unsigned long getSliceAsNumber(const unsigned char* data,unsigned long start,unsigned long length,bool endian) {
    if(length>4) {length=4;}
    unsigned long number=0;
    unsigned char* bytes=getSlice(data,start,length);
    unsigned char pos;
    if(endian) {
        pos=length-1; //Read as big endian
    } else {
        pos = 0; //Read as little endian
    }
    unsigned long pw=1; //Multiply by 1,256,65536...
    for(unsigned int i=0;i<length;i++) {
        if(i>0) {pw*=256;}
        number+=bytes[pos]*pw;
        if(endian) {pos--;} else {pos++;}
    }
    return number;
}

//Get slice as signed 16 bit number
signed int getSliceAsInt16Sample(const unsigned char * data,unsigned long start,bool endian) {
    unsigned int length=2;
    unsigned long number=0;
    unsigned char bytes[2]={data[start],data[start+1]};
    unsigned char little=bytes[endian];
    signed   char big=bytes[!endian];
    number=little+big*256;
    return number;
}

//Get slice as a null terminated string
char* getSliceAsString(const unsigned char* data,unsigned long start,unsigned long length) {
    unsigned char slicestr[length+1];
    unsigned char* bytes=getSlice(data,start,length);
    for(unsigned int i=0;i<length;i++) {
        slicestr[i]=bytes[i];
        if(slicestr[i]=='\0') {slicestr[i]=' ';}
    }
    slicestr[length]='\0';
    delete[] slice;
    slicestring = new char[length+1];
    for(unsigned int i=0;i<length+1;i++) {
        slicestring[i]=slicestr[i];
    }
    return slicestring;
}

//------------------ Command line arguments
const char* opts[] = {"-i","-o","-og","-patch"};
const char* opts_alt[] = {"-i","-o","-og","-patch"};
const unsigned int optcount = 4;
const bool optrequiredarg[optcount] = {1,1,1,1};
bool  optused  [optcount];
char* optargstr[optcount];

int main(int argc, char** args) {
    if(argc<2) {
        std::cout << "Usage: bars_patcher [options]\nOptions:\n-i [filename] - Input BARS file\n-o [filename] - Output BARS file\n-og [filename] - Original BWAV file\n-patch [filename] - Patch BWAV file\n";
        return 0;
    }
    //Parse command line args
    for(unsigned int a=1;a<argc;a++) {
        int vOpt = -1;
        //Compare cmd arg against each known option
        for(unsigned int o=0;o<optcount;o++) {
            if( strcmp(args[a], opts[o]) == 0 || strcmp(args[a], opts_alt[o]) == 0 ) {
                //Matched
                vOpt = o;
                break;
            }
        }
        //No match
        if(vOpt < 0) {std::cout << "Unknown option '" << args[a] << "'.\n"; exit(255);}
        //Mark the options as used
        optused[vOpt] = 1;
        //Read the argument for the option if it requires it
        if(optrequiredarg[vOpt]) {
            if(a+1 < argc) {
                optargstr[vOpt] = args[++a];
            } else {
                std::cout << "Option " << opts[vOpt] << " requires an argument\n";
                exit(255);
            }
        }
    }
    //Check options
    if(!(optused[0] && optused[1] && optused[2] && optused[3])) {
        std::cout << "All input and output file options need to be used.\n";
        exit(255);
    }
    //Open files
    std::ifstream ibars,ogbwav,pabwav;
    std::streampos ibarsSize;
    unsigned char* ibarsMemblock;
    std::ofstream obars;
    ibars.open(optargstr[0],std::ios::in | std::ios::binary | std::ios::ate);
    if(!ibars.is_open()) {
        perror("Unable to open input BARS file");
        exit(255);
    }
    ogbwav.open(optargstr[2],std::ios::in | std::ios::binary);
    if(!ogbwav.is_open()) {
        perror("Unable to open original BWAV file");
        exit(255);
    }
    pabwav.open(optargstr[3],std::ios::in | std::ios::binary);
    if(!pabwav.is_open()) {
        perror("Unable to open patch BWAV file");
        exit(255);
    }
    //Read BARS file into memory and get information from BWAV files
    ibarsSize = ibars.tellg();
    ibars.seekg(0);
    ogbwav.seekg(0);
    pabwav.seekg(0);
    
    
    //BWAV info
    bool ogBOM, paBOM;
    //CRC32 hash of original file, used to find this file in BARS later
    unsigned char oghashbytes[4];
    uint32_t oghash;
    uint16_t chnum;
    unsigned char ogtmpptr[65536];
    unsigned char patmpptr[65536];
    unsigned int paLen;
    //Check valid magic words
    ogbwav.read((char*)ogtmpptr,0x20);
    if(strcmp("BWAV",getSliceAsString(ogtmpptr,0,4)) != 0) {perror("Bad OG BWAV file"); exit(255);}
    pabwav.read((char*)patmpptr,0x20);
    if(strcmp("BWAV",getSliceAsString(patmpptr,0,4)) != 0) {perror("Bad Patch BWAV file"); exit(255);}
    //Get byte order marks
    if(getSliceAsInt16Sample(ogtmpptr,0x04,1) == -257) {
        ogBOM = 1; //Big endian
    } else {
        ogBOM = 0; //Little endian
    }
    if(getSliceAsInt16Sample(patmpptr,0x04,1) == -257) {
        paBOM = 1; //Big endian
    } else {
        paBOM = 0; //Little endian
    }
    //Compare channel nums
    chnum = getSliceAsNumber(ogtmpptr,0x0E,2,ogBOM);
    if(chnum != getSliceAsNumber(patmpptr,0x0E,2,paBOM)) {
        std::cout << "BWAV channel counts don't match.\n";
        exit(255);
    }
    //Read hash from OG file
    {
        unsigned char* tmp = getSlice(ogtmpptr,0x08,4);
        memcpy(oghashbytes,tmp,4);
        oghash = getSliceAsNumber(oghashbytes,0,4,ogBOM);
    }
    //Read full header from patch BWAV
    paLen = 0x10 + 0x4C*chnum;
    pabwav.seekg(0);
    pabwav.read((char*)patmpptr,paLen);
    std::cout << "Channel count: " << chnum << " Patch length: " << paLen << '\n';
    //Close files
    ogbwav.close();
    pabwav.close();
    
    
    //Read BARS
    ibarsMemblock = new unsigned char[ibarsSize];
    ibars.read((char*)ibarsMemblock,ibarsSize);
    ibars.close();
    
    std::cout << "OG File hash: " << std::hex << oghash << ", searching in BARS...\n";
    unsigned long barsbwavoffset = 0;
    unsigned int b;
    bool nomatch = 0;
    for(unsigned long i=0;i<ibarsSize;i++) {
        //Compare
        for(b=0;b<4;b++) {
            if(ibarsMemblock[i+b] != oghashbytes[b]) {
                nomatch = 1;
                break;
            }
        }
        if(nomatch == 0) {
            //Found
            barsbwavoffset = i - 0x08;
            break;
        }
        nomatch = 0;
    }
    if(barsbwavoffset != 0) {
        std::cout << "File found at offset " << std::hex << barsbwavoffset << '\n';
        if((unsigned long)ibarsSize - barsbwavoffset < paLen) {
            std::cout << "Fatal error: Not enough space for header in BARS file. Is the BARS file valid?\n";
            exit(255);
        }
        std::cout << "Writing patch...\n";
        for(unsigned int i=0;i<paLen;i++) {
            ibarsMemblock[barsbwavoffset+i] = patmpptr[i];
        }
        obars.open(optargstr[1],std::ios::out | std::ios::binary | std::ios::trunc);
        if(!obars.is_open()) {
            perror("Unable to open output BARS file");
            exit(255);
        }
        obars.write((char*)ibarsMemblock,ibarsSize);
        std::cout << "Done\n";
    } else {
        std::cout << "Unable to find.\n";
        exit(255);
    }
    
    delete[] ibarsMemblock;
    return 0;
}
