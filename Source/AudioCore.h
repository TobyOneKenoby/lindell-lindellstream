#pragma once
#include <array>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace lsl {
static_assert(std::atomic<std::uint32_t>::is_always_lock_free);
static_assert(std::atomic<float>::is_always_lock_free);
static_assert(std::atomic<bool>::is_always_lock_free);
constexpr std::size_t packetFrames = 512;
struct Packet {
    std::array<float, packetFrames * 2> interleaved{};
    std::uint32_t frames = 0;
    double sampleRate = 0;
    std::uint64_t firstFrame = 0; // Monotonic within a prepare epoch; includes dropped frames.
    std::uint32_t epoch = 0, generation = 0;
};
// Exactly one audio producer and one worker consumer. No concurrent reset operation.
// A full queue drops NEW packets, preserving the consumer's ownership of old slots.
template<std::size_t Slots> class SpscQueue {
    static_assert(Slots > 1 && Slots < 65536);
public:
    bool push(const Packet& packet) noexcept {
        const auto w = write.load(std::memory_order_relaxed);
        const auto next = (w + 1) % Slots;
        if (next == read.load(std::memory_order_acquire)) return false;
        slots[w] = packet;
        write.store(static_cast<std::uint32_t>(next), std::memory_order_release);
        return true;
    }
    bool pop(Packet& packet) noexcept {
        const auto r = read.load(std::memory_order_relaxed);
        if (r == write.load(std::memory_order_acquire)) return false;
        packet = slots[r];
        read.store(static_cast<std::uint32_t>((r + 1) % Slots), std::memory_order_release);
        return true;
    }
private:
    std::array<Packet, Slots> slots{};
    alignas(64) std::atomic<std::uint32_t> write{0};
    alignas(64) std::atomic<std::uint32_t> read{0};
};
class AudioTap {
public:
    // Called only when the host has stopped audio callbacks. Worker may still drain.
    void prepare(double rate) noexcept {
        sampleRate = rate; frame = 0; ++epoch;
        envelope = {}; power = {};
        for (int c=0;c<2;++c) { peak[c].store(0); rms[c].store(0); clipped[c].store(false); }
    }
    // READ ONLY audio pointers: local bus is never modified, even when queue is full.
    // The host must serialize prepare/process, as required for processor lifecycle.
    void process(const float* left, const float* right, int frames, bool capture, std::uint32_t generation = 0) noexcept {
        if (frames <= 0 || sampleRate <= 0) return;
        const float decay = static_cast<float>(std::exp(-frames / (sampleRate * 0.3)));
        const float* channels[] = {left,right};
        for (int c=0;c<2;++c) {
            float maximum = 0; double sum = 0;
            for (int i=0;i<frames;++i) {
                const float raw = channels[c][i];
                const float x = std::isfinite(raw) ? raw : 0.0f;
                maximum = std::max(maximum, std::abs(x)); sum += double(x)*x;
            }
            envelope[c] = std::max(maximum, envelope[c]*decay);
            power[c] = decay*power[c] + (1-decay)*sum/frames;
            peak[c].store(envelope[c],std::memory_order_relaxed);
            rms[c].store(static_cast<float>(std::sqrt(power[c])),std::memory_order_relaxed);
            if (maximum >= 1.0f) clipped[c].store(true,std::memory_order_relaxed);
        }
        if (capture) {
            for (int start=0; start<frames; start+=static_cast<int>(packetFrames)) {
                Packet packet; // Fixed stack allocation; never heap allocation.
                packet.frames = static_cast<std::uint32_t>(std::min(frames-start, static_cast<int>(packetFrames)));
                packet.generation=generation; packet.sampleRate=sampleRate; packet.epoch=epoch; packet.firstFrame=frame+start;
                for (std::uint32_t i=0;i<packet.frames;++i) {
                    packet.interleaved[2*i]=left[start+i];
                    packet.interleaved[2*i+1]=right[start+i];
                }
                if (!queue.push(packet)) dropped.fetch_add(1,std::memory_order_relaxed);
            }
        }
        frame += static_cast<std::uint64_t>(frames);
        heartbeat.fetch_add(1,std::memory_order_release);
    }
    SpscQueue<33> queue; // Bounded to 32 packets. Worker must drain promptly.
    std::array<std::atomic<float>,2> peak{}, rms{};
    std::array<std::atomic<bool>,2> clipped{};
    std::atomic<std::uint32_t> dropped{0}, heartbeat{0};
private:
    double sampleRate=0;
    std::uint64_t frame=0;
    std::uint32_t epoch=0;
    std::array<float,2> envelope{};
    std::array<double,2> power{};
};
} // namespace lsl
