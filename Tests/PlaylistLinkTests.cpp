#include "../Source/PlaylistLink.h"
#include <iostream>
#include <cstdlib>
#define CHECK(x) do { if(!(x)) {std::cerr<<"FAILED line "<<__LINE__<<'\n';std::exit(1);} } while(false)
int main() {
    using lsl::parsePlaylistLink;
    const std::string token(64,'a');
    const auto canonical="https://lindell-streams.com/?share="+token;
    CHECK(parsePlaylistLink(canonical).canonical==canonical);
    CHECK(parsePlaylistLink(" \n"+canonical+"\t ").canonical==canonical);
    CHECK(parsePlaylistLink("HTTPS://LINDELL-STREAMS.COM/index.php?share="+std::string(64,'A')).canonical==canonical);
    CHECK(parsePlaylistLink("https://lindell-streams.com?share="+token));
    for(const std::string& bad : {std::string(""),std::string("http://lindell-streams.com/?share=")+token,
        "https://evil.com/?share="+token,"https://lindell-streams.com.evil.com/?share="+token,
        "https://lindell-streams.com@evil.com/?share="+token,"https://evil.com@lindell-streams.com/?share="+token,
        "https://lindell-streams.com:444/?share="+token,"https://lindell-streams.com/?song="+token,
        canonical+"&song=x",canonical+"&share="+token,canonical+"#fragment",
        "https://lindell-streams.com/path?share="+token,std::string("https://lindell-streams.com/?share=short"),
        "https://lindell-streams.com/?share="+std::string(64,'g'),canonical+"\nX"})
        CHECK(!parsePlaylistLink(bad));
    std::cout<<"PASS: canonical playlist links and invalid/song/foreign/ambiguous link rejection\n";
}
