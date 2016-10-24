
#pragma once

#include <stcp/util.h>
#include <stddef.h>
#include <stcp/util.h>

namespace slank {

// uint16_t checksum(uint16_t* buf, size_t bufsz);
uint16_t checksum(const void* data, size_t len) noexcept;


} /* namespace slank */
