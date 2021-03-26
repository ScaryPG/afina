#include "StripedLRU.h"

namespace Afina {
namespace Backend {

std::unique_ptr<StripedLRU> StripedLRU::BuildStripedLRU(size_t memory_limit, size_t stripe_count) {
    const size_t stripe_limit = memory_limit / stripe_count;
    if (stripe_limit < 1024 * 1024) {
        throw std::runtime_error("Shard memory size is too small!");
    }
    return std::unique_ptr<StripedLRU>
        (new StripedLRU(memory_limit / stripe_count, stripe_count));
}

} // namespace Backend
} // namespace Afina
