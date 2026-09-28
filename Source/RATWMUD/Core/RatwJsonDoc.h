#pragma once
// JSON for the portable server (Docs/Design/26-living-npcs.md, Phase 6): the save's document, snapshots, commands and
// events, the same text the Unreal runtime reads and writes with its own JSON types. A value is null, a bool, a
// number, a string, an array or an object; an object keeps its fields in the order they were set, as Unreal's does.
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace ratw::json
{
class Value;
using Array = std::vector<Value>;
using Field = std::pair<std::string, Value>;

class Value
{
  public:
    enum class Type { Null, Bool, Number, String, List, Object };
    Value() = default;
    Value(std::nullptr_t) {}
    Value(bool b) : type_(Type::Bool), bool_(b) {}
    Value(int n) : type_(Type::Number), number_(n) {}
    Value(unsigned n) : type_(Type::Number), number_(n) {}
    Value(long n) : type_(Type::Number), number_(double(n)) {}
    Value(long long n) : type_(Type::Number), number_(double(n)) {}
    Value(unsigned long n) : type_(Type::Number), number_(double(n)) {}
    Value(unsigned long long n) : type_(Type::Number), number_(double(n)) {}
    Value(double n) : type_(Type::Number), number_(n) {}
    Value(const char* s) : type_(Type::String), string_(s) {}
    Value(std::string s) : type_(Type::String), string_(std::move(s)) {}
    Value(Array a);
    static Value object();
    static Value array();

    Type type() const { return type_; }
    bool isNull() const { return type_ == Type::Null; }
    bool isBool() const { return type_ == Type::Bool; }
    bool isNumber() const { return type_ == Type::Number; }
    bool isString() const { return type_ == Type::String; }
    bool isArray() const { return type_ == Type::List; }
    bool isObject() const { return type_ == Type::Object; }

    // Reading, forgivingly: a value of another type gives the fallback.
    bool asBool(bool fallback = false) const { return type_ == Type::Bool ? bool_ : fallback; }
    double asNumber(double fallback = 0) const { return type_ == Type::Number ? number_ : fallback; }
    const std::string& asString() const;
    std::string asString(const std::string& fallback) const { return type_ == Type::String ? string_ : fallback; }
    const Array& items() const;                    // An array's items (empty for anything else).
    Array& items();                                // Makes it an array if it isn't.
    const std::vector<Field>& fields() const;      // An object's fields, in order (empty for anything else).

    // Objects. find() is null for a missing field (or a value that isn't an object); operator[] is a null value then.
    const Value* find(const std::string& key) const;
    Value* find(const std::string& key);
    const Value& operator[](const std::string& key) const;
    bool has(const std::string& key) const { return find(key) != nullptr; }
    // Sets a field (replacing one of the same name in place, else adding it at the end); makes this an object.
    Value& set(const std::string& key, Value v);
    // Adds a field without looking for one of the same name: for building objects whose keys are known distinct.
    Value& add(std::string key, Value v);
    bool erase(const std::string& key);
    std::size_t size() const;                      // Fields of an object, items of an array, else 0.
    Value& push(Value v);                          // Appends to an array; makes this an array.

    // Convenience readers for an object's fields.
    double number(const std::string& key, double fallback = 0) const { const auto* v = find(key); return v ? v->asNumber(fallback) : fallback; }
    std::string string(const std::string& key, const std::string& fallback = {}) const { const auto* v = find(key); return v ? v->asString(fallback) : fallback; }
    bool boolean(const std::string& key, bool fallback = false) const { const auto* v = find(key); return v ? v->asBool(fallback) : fallback; }
    const Array& array(const std::string& key) const;
    const Value& object(const std::string& key) const;   // A null value if missing or not an object.

    bool operator==(const Value& other) const;     // Same content; objects compare field sets, whatever their order.
    bool operator!=(const Value& other) const { return !(*this == other); }

  private:
    Type type_ = Type::Null;
    bool bool_ = false;
    double number_ = 0;
    std::string string_;
    std::shared_ptr<Array> array_;
    std::shared_ptr<std::vector<Field>> object_;
    std::shared_ptr<std::map<std::string, std::size_t>> index_;   // Field positions, once an object is large.
    void own();                                     // Copy on write: shared containers are copied before a change.
    void reindex();
};

// Condensed JSON text (no spaces), numbers in the shortest form that reads back exactly.
std::string dump(const Value& v);
// Strict JSON (RFC 8259): false with a message on anything else, or nesting deeper than 256. Duplicate keys keep the
// last value, as Unreal's reader does.
bool parse(const std::string& text, Value& out, std::string& error);
} // namespace ratw::json
