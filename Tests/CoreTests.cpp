#include "../Source/AudioCore.h"
#include <iostream>
#include <thread>
#include <vector>
#include <cstring>
#include <cstdlib>
#define CHECK(x) do { if(!(x)) { std::cerr << "FAILED " << #x << " line " << __LINE__ << '\n'; std::exit(1); } } while(false)
int main() {
    for(double rate:{44100.,48000.,88200.,96000.}) {
        lsl::AudioTap tap; tap.prepare(rate);
        std::vector<float> l(1301),r(1301);
        for(int i=0;i<1301;++i) { l[i]=float(std::sin(i*.1)*1.1);r[i]=float(std::cos(i*.13)*.6); }
        const auto originalL=l, originalR=r;
        tap.process(l.data(),r.data(),1301,true);
        CHECK(std::memcmp(l.data(),originalL.data(),l.size()*sizeof(float))==0);
        CHECK(std::memcmp(r.data(),originalR.data(),r.size()*sizeof(float))==0);
        lsl::Packet packet;int count=0;
        while(tap.queue.pop(packet)) {
            CHECK(packet.sampleRate==rate);CHECK(packet.firstFrame==static_cast<unsigned>(count));
            for(unsigned i=0;i<packet.frames;++i) {
                CHECK(packet.interleaved[2*i]==l[count+i]);CHECK(packet.interleaved[2*i+1]==r[count+i]);
            }
            count+=packet.frames;
        }
        CHECK(count==1301);CHECK(tap.clipped[0].load());CHECK(!tap.clipped[1].load());
        CHECK(tap.peak[0].load()>1);CHECK(tap.rms[0].load()>0);
        tap.process(l.data(),r.data(),0,true); CHECK(!tap.queue.pop(packet));
        tap.process(l.data(),r.data(),1301,false);CHECK(!tap.queue.pop(packet));
        for(int i=0;i<40;++i) tap.process(l.data(),r.data(),512,true);
        CHECK(tap.dropped.load()==8);
        int packets=0;while(tap.queue.pop(packet))++packets;CHECK(packets==32);
        auto oldEpoch=packet.epoch;tap.prepare(48000);
        tap.process(l.data(),r.data(),1,true);CHECK(tap.queue.pop(packet));
        CHECK(packet.epoch!=oldEpoch);CHECK(packet.firstFrame==0);
    }
    // Concurrent wraparound: consumer must never see torn or out-of-order samples.
    lsl::SpscQueue<17> queue;
    constexpr std::uint64_t total=100000;
    std::thread producer([&] {
        for(std::uint64_t n=0;n<total;++n) {
            lsl::Packet p;p.firstFrame=n;p.frames=512;p.interleaved.fill(float(n));
            while(!queue.push(p))std::this_thread::yield();
        }
    });
    for(std::uint64_t n=0;n<total;++n) {
        lsl::Packet p;while(!queue.pop(p))std::this_thread::yield();
        CHECK(p.firstFrame==n);for(float v:p.interleaved)CHECK(v==float(n));
    }
    producer.join();
    std::cout << "PASS: unchanged samples, rates, packet boundaries, meters, overflow, epochs, 100000 concurrent packets\n";
}
