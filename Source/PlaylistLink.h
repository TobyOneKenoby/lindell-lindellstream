#pragma once
#include <string>
#include <algorithm>

namespace lsl {
struct PlaylistLink {
    std::string canonical;
    std::string error;
    explicit operator bool() const { return !canonical.empty(); }
};
// Deliberately accepts only the current website's playlist-share format.
// No network request is made. This is NOT ownership verification.
inline PlaylistLink parsePlaylistLink(std::string text) {
    const auto first=text.find_first_not_of(" \t\r\n");
    if(first==std::string::npos) return {{},"Paste a playlist link."};
    text=text.substr(first,text.find_last_not_of(" \t\r\n")-first+1);
    if(text.size()>2048) return {{},"Playlist link is too long."};
    const auto scheme=text.find("://");
    if(scheme==std::string::npos) return {{},"Use an HTTPS playlist link."};
    auto protocol=text.substr(0,scheme);
    auto lower=[](unsigned char c) { return c>='A' && c<='Z'?char(c+32):char(c); };
    std::transform(protocol.begin(),protocol.end(),protocol.begin(),lower);
    if(protocol!="https") return {{},"Use an HTTPS playlist link."};
    auto end=text.find_first_of("/?#",scheme+3);
    if(end==std::string::npos) return {{},"The link must include ?share=..."};
    auto host=text.substr(scheme+3,end-scheme-3);
    std::transform(host.begin(),host.end(),host.begin(),lower);
    if(host!="lindell-streams.com") return {{},"Use a lindell-streams.com playlist link."};
    auto tail=text.substr(end);
    auto query=tail.find('?');
    if(query==std::string::npos) return {{},"The link must include ?share=..."};
    const auto path=tail.substr(0,query);
    if(path!="" && path!="/" && path!="/index.php")
        return {{},"Use the playlist's Share link."};
    const auto params=tail.substr(query+1);
    if(params.rfind("share=",0)!=0) return {{},"Use a playlist link, not an individual song link."};
    auto token=params.substr(6);
    if(token.size()!=64 || !std::all_of(token.begin(),token.end(),[](unsigned char c) {
        return (c>='0' && c<='9') || (c>='a' && c<='f') || (c>='A' && c<='F');
    })) return {{},"Copy the complete playlist Share link (without extra parameters)."};
    std::transform(token.begin(),token.end(),token.begin(),lower);
    return {"https://lindell-streams.com/?share="+token,{}};
}
}
