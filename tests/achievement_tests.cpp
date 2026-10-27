#include "../src/achievement_tools.hpp"
#include "../src/portable_state.hpp"
#include "../src/session.hpp"
#include <chrono>
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<#x<<" line "<<__LINE__<<'\n';return 1;}}while(false)
int main(int argc,char** argv){CHECK(argc==2);auto g=std::make_unique<GameBoy>();CHECK(g->load(argv[1]));
    std::atomic<unsigned> unlocks=0;
    achievements::Client client(*g,[&](const achievements::Request& r)->achievements::Response{
        if(r.post.find("r=login2")!=std::string::npos)return {R"({"Success":true,"User":"OfflineTest","Token":"FakeToken","Score":0,"SoftcoreScore":0,"Messages":0})",200};
        if(r.post.find("r=achievementsets")!=std::string::npos)return {R"({"Success":true,"GameId":1234,"Title":"Local synthetic set","ConsoleId":4,"ImageIconUrl":"https://server/test.png","RichPresenceGameId":1234,"RichPresencePatch":"","Sets":[{"AchievementSetId":1111,"GameId":1234,"Title":null,"Type":"core","ImageIconUrl":"https://server/test.png","Achievements":[{"ID":5501,"Title":"RAM trigger","Description":"C023 equals 42","Flags":3,"Points":5,"MemAddr":"0xHC023=42","Author":"Test","BadgeName":"00234","Created":1367266583,"Modified":1376929305}],"Leaderboards":[]}]})",200};
        if(r.post.find("r=awardachievement")!=std::string::npos){++unlocks;return {R"({"Success":true,"Score":5,"SoftcoreScore":5,"AchievementID":5501,"AchievementsRemaining":0})",200};}
        return {R"({"Success":true})",200};
    });
    g->auxiliaryState=[&](){return client.progress();};g->auxiliaryRestore=[&](std::span<const u8> bytes){return client.restore(bytes);};
    client.login("OfflineTest","FakeToken");const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while(client.rows().empty()&&std::chrono::steady_clock::now()<deadline){client.poll();std::this_thread::yield();}
    CHECK(client.signedIn()&&client.rows().size()==1&&client.hardcore());CHECK(client.takeReset());g->reset();client.reset();
    client.setHardcore(false);g->mem.write(0xC023,0);client.frame();const auto state=g->serializeState(),bess=saves::exportBess(*g);CHECK(!client.progress().empty()&&!bess.empty());
    g->mem.write(0xC023,42);client.frame();client.frame();
    while(!unlocks&&std::chrono::steady_clock::now()<deadline){client.poll();std::this_thread::yield();}CHECK(unlocks==1);
    CHECK(saves::importBess(*g,bess)&&g->mem.read(0xC023)==0);
    CHECK(g->deserializeState(state)&&g->mem.read(0xC023)==0);client.setHardcore(true);CHECK(client.takeReset());g->reset();client.reset();
    CHECK(client.hardcore());client.logout();CHECK(!client.signedIn()&&!client.hardcore());
    CHECK(g->deserializeState(state)&&g->mem.read(0xC023)==0);
    std::cout<<"Offline rc_client login, ROM identity, native memory trigger, unlock request, state progress and Hardcore reset PASS\n";
    return 0;
}
