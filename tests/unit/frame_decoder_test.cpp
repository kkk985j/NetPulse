#include "netpulse/protocol/frame.hpp"
#include "netpulse/protocol/frame_decoder.hpp"

#include <arpa/inet.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <span>
#include <string_view>
#include <vector>

using netpulse::protocol::FrameDecodeStatus;
using netpulse::protocol::FrameDecoder;
using netpulse::protocol::max_payload_size;

namespace {

int fail(const char* message)
{
    std::cerr << "FAILED: " << message << '\n';
    return 1;
}

std::vector<std::byte> makeFrame(
    std::string_view payload)
{
    const auto payload_size{
        static_cast<std::uint32_t>(payload.size())
    };

    const std::uint32_t network_size{
        htonl(payload_size)
    };

    std::vector<std::byte> frame(
        sizeof(network_size) + payload.size());

    std::memcpy(
        frame.data(),
        &network_size,
        sizeof(network_size));

    if (!payload.empty()) {
        std::memcpy(
            frame.data() + sizeof(network_size),
            payload.data(),
            payload.size());
    }

    return frame;
}

bool payloadMatches(
    const std::vector<std::byte>& actual,
    std::string_view expected)
{
    const auto expected_bytes = std::as_bytes(
        std::span{
            expected.data(),
            expected.size()
        });

    return actual.size() == expected_bytes.size()
        && std::equal(
            actual.begin(),
            actual.end(),
            expected_bytes.begin());
}

}  // namespace

int main()
{
    FrameDecoder decoder{};

    const auto first_frame = makeFrame("first");
    const auto second_frame = makeFrame("second");

    decoder.append(
        std::span<const std::byte>{
            first_frame.data(),
            2
        });

    const auto partial_header = decoder.decodeNext();

    if (partial_header.status
        != FrameDecodeStatus::need_more_data) {
        return fail("partial header was accepted");
    }

    std::vector<std::byte> remaining_data{};

    remaining_data.insert(
        remaining_data.end(),
        first_frame.begin() + 2,
        first_frame.end());

    remaining_data.insert(
        remaining_data.end(),
        second_frame.begin(),
        second_frame.end());

    decoder.append(
        std::span<const std::byte>{remaining_data});

    const auto first_result = decoder.decodeNext();

    if (first_result.status
        != FrameDecodeStatus::frame_ready) {
        return fail("first frame was not decoded");
    }

    if (!payloadMatches(first_result.payload, "first")) {
        return fail("first payload does not match");
    }

    const auto second_result = decoder.decodeNext();

    if (second_result.status
        != FrameDecodeStatus::frame_ready) {
        return fail("second frame was not decoded");
    }

    if (!payloadMatches(second_result.payload, "second")) {
        return fail("second payload does not match");
    }

    if (decoder.bufferedBytes() != 0) {
        return fail("decoded bytes were not consumed");
    }

    const auto fragmented_frame{
        makeFrame("fragmented-payload")
    };

    constexpr std::size_t first_fragment_size{
        sizeof(std::uint32_t) + 3
    };

    decoder.append(
        std::span<const std::byte>{
            fragmented_frame.data(),
            first_fragment_size
        });

    const auto partial_payload = decoder.decodeNext();

    if (partial_payload.status
        != FrameDecodeStatus::need_more_data) {
        return fail("partial payload was accepted");
    }

    decoder.append(
        std::span<const std::byte>{
            fragmented_frame.data() + first_fragment_size,
            fragmented_frame.size() - first_fragment_size
        });

    const auto fragmented_result = decoder.decodeNext();

    if (fragmented_result.status
        != FrameDecodeStatus::frame_ready) {
        return fail("fragmented frame was not decoded");
    }

    if (!payloadMatches(
            fragmented_result.payload,
            "fragmented-payload")) {
        return fail("fragmented payload does not match");
    }

    const auto empty_frame = makeFrame("");

    decoder.append(
        std::span<const std::byte>{empty_frame});

    const auto empty_result = decoder.decodeNext();

    if (empty_result.status
        != FrameDecodeStatus::frame_ready) {
        return fail("empty frame was not decoded");
    }

    if (!empty_result.payload.empty()) {
        return fail("empty frame contains payload");
    }

    const std::uint32_t oversized_length{
        htonl(max_payload_size + 1U)
    };

    std::array<std::byte, sizeof(std::uint32_t)>
        oversized_header{};

    std::memcpy(
        oversized_header.data(),
        &oversized_length,
        sizeof(oversized_length));

    decoder.append(
        std::span<const std::byte>{oversized_header});

    const auto oversized_result = decoder.decodeNext();

    if (oversized_result.status
        != FrameDecodeStatus::payload_too_large) {
        return fail("oversized frame was not rejected");
    }

    decoder.clear();

    if (decoder.bufferedBytes() != 0) {
        return fail("clear did not reset the decoder");
    }

    std::cout << "Frame decoder tests passed.\n";
    return 0;
}