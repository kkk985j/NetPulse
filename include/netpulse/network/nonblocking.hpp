#pragma once

namespace netpulse::network{

    struct NonBlockingResult{
        bool success {false};
        int error_number {0};

        [[nodiscard]] explicit operator bool()const noexcept
        {
            return success;
        }
    };

    [[nodiscard]] NonBlockingResult setNonBlocking
    (
        int fd
    ) noexcept;
}