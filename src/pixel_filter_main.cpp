// GPLv3 image-processing tool linked with xBRZ; see third_party/xbrz/COPYING.
#include "../third_party/xbrz/xbrz.h"
#include <cstdint>
#include <iostream>
#include <vector>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif
int main(){
#ifdef _WIN32
    _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
#endif
    std::ios::sync_with_stdio(false);
    for(;;){uint32_t header[4]{};if(!std::cin.read((char*)header,sizeof(header)))return std::cin.eof()?0:1;
        const unsigned width=header[1],height=header[2],factor=header[3];if(header[0]!=0x46504247||!width||!height||width>1024||height>1024||factor<2||factor>6)return 2;
        std::vector<uint32_t> source((std::size_t)width*height),output(source.size()*factor*factor);
        if(!std::cin.read((char*)source.data(),(std::streamsize)source.size()*4))return 3;
        xbrz::scale(factor,source.data(),output.data(),(int)width,(int)height,xbrz::ColorFormat::RGB);
        for(auto& color:output)color|=0xFF000000;
        const uint32_t reply[]={header[0],(uint32_t)output.size()};std::cout.write((const char*)reply,sizeof(reply));std::cout.write((const char*)output.data(),(std::streamsize)output.size()*4);std::cout.flush();if(!std::cout)return 4;
    }
}
