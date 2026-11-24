#include "Json.h"

#include <cmath>
#include <cstdlib>

namespace json {

Value Value::makeBool(bool b) {
    Value v;
    v.type_ = Type::Bool;
    v.bool_ = b;
    return v;
}

Value Value::makeNumber(double n) {
    Value v;
    v.type_ = Type::Number;
    v.number_ = n;
    return v;
}

Value Value::makeString(std::string s) {
    Value v;
    v.type_ = Type::String;
    v.string_ = std::move(s);
    return v;
}

Value Value::makeArray(Array a) {
    Value v;
    v.type_ = Type::Array;
    v.array_ = std::move(a);
    return v;
}

Value Value::makeObject(Object o) {
    Value v;
    v.type_ = Type::Object;
    v.object_ = std::move(o);
    return v;
}

static void expectType(const Value& v, Value::Type t, const char* name) {
    if (v.type() != t) throw std::runtime_error(std::string("JSON value is not ") + name);
}

bool Value::asBool() const {
    expectType(*this, Type::Bool, "a boolean");
    return bool_;
}

double Value::asNumber() const {
    expectType(*this, Type::Number, "a number");
    return number_;
}

int Value::asInt() const {
    double n = asNumber();
    if (n != std::floor(n)) throw std::runtime_error("JSON number is not an integer");
    return static_cast<int>(n);
}

const std::string& Value::asString() const {
    expectType(*this, Type::String, "a string");
    return string_;
}

const Value::Array& Value::asArray() const {
    expectType(*this, Type::Array, "an array");
    return array_;
}

const Value::Object& Value::asObject() const {
    expectType(*this, Type::Object, "an object");
    return object_;
}

const Value* Value::find(const std::string& key) const {
    for (const auto& kv : asObject())
        if (kv.first == key) return &kv.second;
    return nullptr;
}

const Value& Value::at(const std::string& key) const {
    const Value* v = find(key);
    if (!v) throw std::runtime_error("missing key \"" + key + "\"");
    return *v;
}

namespace {

class Parser {
public:
    explicit Parser(const std::string& text) : s_(text) {
        // Skip UTF-8 BOM if present.
        if (s_.size() >= 3 && static_cast<unsigned char>(s_[0]) == 0xEF &&
            static_cast<unsigned char>(s_[1]) == 0xBB && static_cast<unsigned char>(s_[2]) == 0xBF)
            pos_ = 3;
    }

    Value parseDocument() {
        Value v = parseValue();
        skipWs();
        if (pos_ != s_.size()) fail("unexpected trailing characters");
        return v;
    }

private:
    const std::string& s_;
    size_t pos_ = 0;

    [[noreturn]] void fail(const std::string& msg) const {
        size_t line = 1, col = 1;
        for (size_t i = 0; i < pos_ && i < s_.size(); ++i) {
            if (s_[i] == '\n') {
                ++line;
                col = 1;
            } else {
                ++col;
            }
        }
        throw ParseError("JSON parse error at line " + std::to_string(line) + ", column " +
                         std::to_string(col) + ": " + msg);
    }

    void skipWs() {
        while (pos_ < s_.size() && (s_[pos_] == ' ' || s_[pos_] == '\t' || s_[pos_] == '\n' || s_[pos_] == '\r'))
            ++pos_;
    }

    char peek() {
        skipWs();
        if (pos_ >= s_.size()) fail("unexpected end of input");
        return s_[pos_];
    }

    void expect(char c) {
        if (peek() != c) fail(std::string("expected '") + c + "'");
        ++pos_;
    }

    bool consumeLiteral(const char* lit) {
        size_t n = std::char_traits<char>::length(lit);
        if (s_.compare(pos_, n, lit) == 0) {
            pos_ += n;
            return true;
        }
        return false;
    }

    Value parseValue() {
        char c = peek();
        if (c == '{') return parseObject();
        if (c == '[') return parseArray();
        if (c == '"') return Value::makeString(parseString());
        if (c == '-' || (c >= '0' && c <= '9')) return parseNumber();
        if (consumeLiteral("true")) return Value::makeBool(true);
        if (consumeLiteral("false")) return Value::makeBool(false);
        if (consumeLiteral("null")) return Value();
        fail("unexpected character");
    }

    Value parseObject() {
        expect('{');
        Value::Object obj;
        if (peek() == '}') {
            ++pos_;
            return Value::makeObject(std::move(obj));
        }
        while (true) {
            if (peek() != '"') fail("expected string key");
            std::string key = parseString();
            expect(':');
            obj.emplace_back(std::move(key), parseValue());
            char c = peek();
            ++pos_;
            if (c == '}') break;
            if (c != ',') fail("expected ',' or '}'");
        }
        return Value::makeObject(std::move(obj));
    }

    Value parseArray() {
        expect('[');
        Value::Array arr;
        if (peek() == ']') {
            ++pos_;
            return Value::makeArray(std::move(arr));
        }
        while (true) {
            arr.push_back(parseValue());
            char c = peek();
            ++pos_;
            if (c == ']') break;
            if (c != ',') fail("expected ',' or ']'");
        }
        return Value::makeArray(std::move(arr));
    }

    Value parseNumber() {
        size_t start = pos_;
        if (s_[pos_] == '-') ++pos_;
        while (pos_ < s_.size() && std::string("0123456789.eE+-").find(s_[pos_]) != std::string::npos) ++pos_;
        std::string token = s_.substr(start, pos_ - start);
        char* end = nullptr;
        double d = std::strtod(token.c_str(), &end);
        if (end == token.c_str() || *end != '\0') fail("invalid number");
        return Value::makeNumber(d);
    }

    unsigned parseHex4() {
        if (pos_ + 4 > s_.size()) fail("truncated \\u escape");
        unsigned v = 0;
        for (int i = 0; i < 4; ++i) {
            char c = s_[pos_++];
            v <<= 4;
            if (c >= '0' && c <= '9') v |= static_cast<unsigned>(c - '0');
            else if (c >= 'a' && c <= 'f') v |= static_cast<unsigned>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') v |= static_cast<unsigned>(c - 'A' + 10);
            else fail("invalid \\u escape");
        }
        return v;
    }

    static void appendUtf8(std::string& out, unsigned cp) {
        if (cp < 0x80) {
            out += static_cast<char>(cp);
        } else if (cp < 0x800) {
            out += static_cast<char>(0xC0 | (cp >> 6));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            out += static_cast<char>(0xE0 | (cp >> 12));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (cp >> 18));
            out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }

    std::string parseString() {
        expect('"');
        std::string out;
        while (true) {
            if (pos_ >= s_.size()) fail("unterminated string");
            char c = s_[pos_++];
            if (c == '"') break;
            if (c != '\\') {
                out += c;
                continue;
            }
            if (pos_ >= s_.size()) fail("unterminated escape");
            char e = s_[pos_++];
            switch (e) {
            case '"': out += '"'; break;
            case '\\': out += '\\'; break;
            case '/': out += '/'; break;
            case 'b': out += '\b'; break;
            case 'f': out += '\f'; break;
            case 'n': out += '\n'; break;
            case 'r': out += '\r'; break;
            case 't': out += '\t'; break;
            case 'u': {
                unsigned cp = parseHex4();
                if (cp >= 0xD800 && cp <= 0xDBFF) {
                    if (!consumeLiteral("\\u")) fail("missing low surrogate");
                    unsigned lo = parseHex4();
                    if (lo < 0xDC00 || lo > 0xDFFF) fail("invalid low surrogate");
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                }
                appendUtf8(out, cp);
                break;
            }
            default: fail("invalid escape");
            }
        }
        return out;
    }
};

}  // namespace

Value parse(const std::string& text) {
    return Parser(text).parseDocument();
}

}  // namespace json
