#pragma once

// Minimal JSON reader used to load game data (data/game.json).
// Supports the full JSON grammar; strings are kept as UTF-8.

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace json {

class ParseError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class Value {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };
    using Array = std::vector<Value>;
    using Object = std::vector<std::pair<std::string, Value>>;

    Value() = default;
    static Value makeBool(bool b);
    static Value makeNumber(double n);
    static Value makeString(std::string s);
    static Value makeArray(Array a);
    static Value makeObject(Object o);

    Type type() const { return type_; }
    bool isNull() const { return type_ == Type::Null; }
    bool isNumber() const { return type_ == Type::Number; }
    bool isString() const { return type_ == Type::String; }
    bool isArray() const { return type_ == Type::Array; }
    bool isObject() const { return type_ == Type::Object; }

    // Accessors throw std::runtime_error when the type does not match.
    bool asBool() const;
    double asNumber() const;
    int asInt() const;
    const std::string& asString() const;
    const Array& asArray() const;
    const Object& asObject() const;

    // Object lookup. find() returns nullptr when the key is missing.
    const Value* find(const std::string& key) const;
    const Value& at(const std::string& key) const;  // throws when missing

private:
    Type type_ = Type::Null;
    bool bool_ = false;
    double number_ = 0.0;
    std::string string_;
    Array array_;
    Object object_;
};

Value parse(const std::string& text);

}  // namespace json
