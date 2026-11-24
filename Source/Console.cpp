#include "Console.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <thread>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace console {

namespace {

int g_typingDelayMs = 20;

std::string trim(const std::string& s) {
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

// Byte length of the UTF-8 sequence starting with lead byte c.
size_t utf8Length(unsigned char c) {
    if (c < 0x80) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;  // invalid lead byte: emit as-is
}

}  // namespace

void init() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}

void setTypingDelay(int ms) {
    g_typingDelayMs = ms < 0 ? 0 : ms;
}

void clear() {
    std::fflush(stdout);
#ifdef _WIN32
    std::system("cls");
#else
    std::fputs("\x1b[2J\x1b[H", stdout);
#endif
}

void print(const std::string& text) {
    std::fwrite(text.data(), 1, text.size(), stdout);
    std::fflush(stdout);
}

void typewrite(const std::string& text) {
    if (g_typingDelayMs == 0) {
        print(text);
        return;
    }
    // Write whole UTF-8 characters so multi-byte (Korean) text is never split mid-character.
    size_t i = 0;
    while (i < text.size()) {
        size_t len = utf8Length(static_cast<unsigned char>(text[i]));
        if (i + len > text.size()) len = text.size() - i;
        std::fwrite(text.data() + i, 1, len, stdout);
        std::fflush(stdout);
        i += len;
        std::this_thread::sleep_for(std::chrono::milliseconds(g_typingDelayMs));
    }
}

std::string readLine() {
    std::fflush(stdout);
#ifdef _WIN32
    // Interactive console: read UTF-16 so Korean input survives regardless of the code page.
    HANDLE in = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode = 0;
    if (in != INVALID_HANDLE_VALUE && GetConsoleMode(in, &mode)) {
        std::wstring wide;
        wchar_t buf[256];
        while (true) {
            DWORD read = 0;
            if (!ReadConsoleW(in, buf, 256, &read, nullptr) || read == 0) throw InputClosed();
            wide.append(buf, read);
            if (!wide.empty() && wide.back() == L'\n') break;
        }
        while (!wide.empty() && (wide.back() == L'\n' || wide.back() == L'\r')) wide.pop_back();
        if (!wide.empty() && wide[0] == 0x1A) throw InputClosed();  // Ctrl+Z
        if (wide.empty()) return "";
        int size = WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0,
                                       nullptr, nullptr);
        std::string out(static_cast<size_t>(size), '\0');
        WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), &out[0], size, nullptr,
                            nullptr);
        return out;
    }
#endif
    std::string line;
    if (!std::getline(std::cin, line)) throw InputClosed();
    if (!line.empty() && line.back() == '\r') line.pop_back();
    return line;
}

void waitEnter(const std::string& prompt) {
    print(prompt);
    readLine();
}

bool askYesNo(const std::string& prompt, const std::string& invalidMessage) {
    while (true) {
        print(prompt);
        std::string answer = trim(readLine());
        if (answer == "y" || answer == "Y") return true;
        if (answer == "n" || answer == "N") return false;
        print(invalidMessage + "\n");
    }
}

int askNumber(const std::string& prompt, int min, int max,
              const std::string& notNumberMessage, const std::string& outOfRangeMessage) {
    while (true) {
        print(prompt);
        std::string answer = trim(readLine());
        char* end = nullptr;
        long value = std::strtol(answer.c_str(), &end, 10);
        if (answer.empty() || *end != '\0') {
            print(notNumberMessage + "\n");
            continue;
        }
        if (value < min || value > max) {
            print(outOfRangeMessage + "\n");
            continue;
        }
        return static_cast<int>(value);
    }
}

}  // namespace console
