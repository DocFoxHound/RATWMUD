// A value kept out of line (Docs/Design/31-responsiveness.md, "Entity size"): an entity's large, seldom-read parts (its
// appearance, a player's practice) live on the heap behind this box, so the per-tick sweeps over every entity touch less
// memory. Measured 2026-10-07: 352 bytes more in each of 1,500 entities made movement, views and streaming cost 40% more.
//
// It behaves as the value: copied deeply, never empty, assignable from a value, and it converts to a reference to the
// value, so code that passes or assigns the whole thing is unchanged. Only reading a member needs `->`.
#pragma once

#include <memory>
#include <utility>

namespace ratw
{
template <class T>
class Cold
{
  public:
    Cold() : value_(std::make_unique<T>()) {}
    explicit Cold(T value) : value_(std::make_unique<T>(std::move(value))) {}
    Cold(const Cold& other) : value_(std::make_unique<T>(*other.value_)) {}
    Cold(Cold&& other) : value_(std::make_unique<T>(std::move(*other.value_))) {}   // (The moved-from keeps a value.)
    Cold& operator=(const Cold& other)
    {
        *value_ = *other.value_;
        return *this;
    }
    Cold& operator=(Cold&& other)
    {
        *value_ = std::move(*other.value_);
        return *this;
    }
    Cold& operator=(const T& value)
    {
        *value_ = value;
        return *this;
    }
    Cold& operator=(T&& value)
    {
        *value_ = std::move(value);
        return *this;
    }

    T* operator->() { return value_.get(); }
    const T* operator->() const { return value_.get(); }
    T& operator*() { return *value_; }
    const T& operator*() const { return *value_; }
    operator T&() { return *value_; }
    operator const T&() const { return *value_; }

    friend bool operator==(const Cold& a, const Cold& b) { return *a.value_ == *b.value_; }
    friend bool operator!=(const Cold& a, const Cold& b) { return !(*a.value_ == *b.value_); }

  private:
    std::unique_ptr<T> value_;
};
} // namespace ratw
