#pragma once
#include "link_devices.hpp"
#include <ctime>

namespace collection {
struct Game {u32 hash=0;std::string path,title,cover,note;bool favorite=false;u64 cycles=0;};
class Library {
    std::filesystem::path file;
public:
    std::vector<Game> games;
    void open(const std::filesystem::path& path){file=path;games.clear();const auto bytes=Memory::loadBytes(file,4*1024*1024);std::istringstream input(std::string(bytes.begin(),bytes.end()));
        Game g;while(input>>g.hash>>g.favorite>>g.cycles>>std::quoted(g.path)>>std::quoted(g.title)>>std::quoted(g.cover)>>std::quoted(g.note)){
            if(games.size()>=10000)break;games.push_back(g);}}
    Game& add(u32 hash,const std::filesystem::path& path,std::string title){for(auto& g:games)if(g.hash==hash){g.path=studioPath(path);return g;}
        games.push_back({hash,studioPath(path),std::move(title),{},{},false,0});return games.back();}
    static std::string studioPath(const std::filesystem::path& path){const auto value=path.u8string();return {value.begin(),value.end()};}
    bool save()const{if(file.empty())return false;std::ostringstream out;for(const auto& g:games)out<<g.hash<<' '<<g.favorite<<' '<<g.cycles<<' '<<std::quoted(g.path)<<' '<<std::quoted(g.title)<<' '<<std::quoted(g.cover)<<' '<<std::quoted(g.note)<<'\n';return Memory::writeAtomic(file,out.str(),true);}
    void played(u32 hash,u64 cycles){for(auto& g:games)if(g.hash==hash){g.cycles+=cycles;return;}}
    std::vector<std::size_t> search(std::string query,bool favorites=false)const{auto lower=[](std::string text){for(char& c:text)if(c>='A'&&c<='Z')c=(char)(c+32);return text;};query=lower(query);std::vector<std::size_t> result;
        for(std::size_t i=0;i<games.size();++i){const auto& g=games[i];if((!favorites||g.favorite)&&(query.empty()||lower(g.title+" "+g.path+" "+g.note).find(query)!=std::string::npos))result.push_back(i);}return result;}
};
struct Checkpoint {std::string id,parent,comment;u64 cycles=0,created=0;};
class Timeline {
    std::filesystem::path folder;std::string current;
    bool saveIndex()const{std::ostringstream out;for(const auto& point:points)out<<std::quoted(point.id)<<' '<<std::quoted(point.parent)<<' '<<point.cycles<<' '<<point.created<<' '<<std::quoted(point.comment)<<'\n';return Memory::writeAtomic(folder/"timeline.tsv",out.str(),true);}
    static bool validID(const std::string& id){return !id.empty()&&id.size()<80&&std::all_of(id.begin(),id.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f')||c=='-';});}
public:
    std::vector<Checkpoint> points;
    void open(const std::filesystem::path& path){folder=path;current.clear();points.clear();const auto bytes=Memory::loadBytes(folder/"timeline.tsv",4*1024*1024);
        std::istringstream in(std::string(bytes.begin(),bytes.end()));Checkpoint point;while(in>>std::quoted(point.id)>>std::quoted(point.parent)>>point.cycles>>point.created>>std::quoted(point.comment)){
            if(!validID(point.id)||(!point.parent.empty()&&!validID(point.parent))||points.size()>=10000)break;points.push_back(point);}if(!points.empty())current=points.back().id;const auto head=Memory::loadBytes(folder/"branch.head",80);const std::string saved(head.begin(),head.end());if(validID(saved)&&std::any_of(points.begin(),points.end(),[&](const Checkpoint& p){return p.id==saved;}))current=saved;}
    bool capture(GameBoy& gb,std::string comment){std::error_code ec;std::filesystem::create_directories(folder,ec);if(ec||points.size()>=10000)return false;
        std::ostringstream name;name<<std::hex<<std::chrono::steady_clock::now().time_since_epoch().count()<<'-'<<points.size();const std::string id=name.str();
        if(!saves::save(gb,folder/(id+".bess"))||!devices::bitmap(folder/(id+".bmp"),gb.ppu.pixels,GB_W,GB_H))return false;
        points.push_back({id,current,std::move(comment),gb.totalCycles,(u64)std::time(nullptr)});
        if(!saveIndex()){points.pop_back();return false;}current=id;Memory::writeAtomic(folder/"branch.head",current,true);return true;}
    bool restore(GameBoy& gb,std::size_t index){if(index>=points.size()||!saves::load(gb,folder/(points[index].id+".bess")))return false;current=points[index].id;Memory::writeAtomic(folder/"branch.head",current,true);return true;}
    std::filesystem::path thumbnail(std::size_t index)const{return index<points.size()?folder/(points[index].id+".bmp"):std::filesystem::path();}
    const std::string& branchHead()const{return current;}
};
} // namespace collection
