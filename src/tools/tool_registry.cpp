#include "tools/tool_registry.h"

#include <cstdint>
#include <utility>

// Start with 16 empty buckets.
ToolRegistry::ToolRegistry() : buckets_(16) {}

// The unique_ptrs delete the tools automatically.
ToolRegistry::~ToolRegistry() = default;

std::size_t ToolRegistry::hash(std::string_view key) noexcept {
    // Use the string hash given in Appendix A.
    std::uint64_t h = 1469598103934665603ULL;

    // Include every character, in order.
    for (unsigned char c : key) {
        h ^= c;
        h *= 1099511628211ULL;
    }

    return static_cast<std::size_t>(h);
}

void ToolRegistry::rehash(std::size_t new_bucket_count) {
    std::vector<Bucket> new_buckets(new_bucket_count);

    for (auto& bucket : buckets_) {
        for (auto& tool : bucket) {
            // The bucket index changes when the bucket count changes.
            std::size_t index = hash(tool->name()) % new_bucket_count;

            // Move ownership without moving the tool itself.
            new_buckets[index].push_back(std::move(tool));
        }
    }

    buckets_ = std::move(new_buckets);
}

bool ToolRegistry::add(std::unique_ptr<Tool> tool) {
    if (tool == nullptr) {
        return false;
    }

    std::string name = tool->name();

    // Keep the original tool if this name is already registered.
    if (find(name) != nullptr) {
        return false;
    }

    // Double the table before the load factor would go above 1.
    if (size_ + 1 > buckets_.size()) {
        rehash(buckets_.size() * 2);
    }

    std::size_t index = hash(name) % buckets_.size();

    buckets_[index].push_back(std::move(tool));
    ++size_;

    return true;
}

Tool* ToolRegistry::find(std::string_view name) const {
    std::size_t index = hash(name) % buckets_.size();

    // Only search the bucket where this name belongs.
    for (const auto& tool : buckets_[index]) {
        if (tool->name() == name) {
            return tool.get();
        }
    }

    return nullptr;
}

std::size_t ToolRegistry::size() const noexcept {
    return size_;
}

void ToolRegistry::for_each(
    const std::function<void(const Tool&)>& fn) const {
    // Visit every tool in every bucket.
    for (const auto& bucket : buckets_) {
        for (const auto& tool : bucket) {
            fn(*tool);
        }
    }
}

std::size_t ToolRegistry::bucket_count() const noexcept {
    return buckets_.size();
}

double ToolRegistry::load_factor() const noexcept {
    // Convert to double so this does not use integer division.
    return static_cast<double>(size_) /
           static_cast<double>(buckets_.size());
}

std::size_t ToolRegistry::max_probe_length() const {
    std::size_t longest = 0;

    // For chaining, the probe length is the longest bucket.
    for (const auto& bucket : buckets_) {
        if (bucket.size() > longest) {
            longest = bucket.size();
        }
    }

    return longest;
}