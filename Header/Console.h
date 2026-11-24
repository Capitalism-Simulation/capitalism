#pragma once

// Console input/output helpers. All text is UTF-8.

#include <stdexcept>
#include <string>

namespace console {

// Thrown when standard input is closed (EOF), so the game can exit cleanly.
class InputClosed : public std::runtime_error {
public:
    InputClosed() : std::runtime_error("input closed") {}
};

void init();                       // Switch the Windows console to UTF-8.
void setTypingDelay(int ms);       // 0 disables the typewriter effect.
void clear();
void print(const std::string& text);
void typewrite(const std::string& text);  // Prints one character at a time.

std::string readLine();            // Without the trailing newline. Throws InputClosed.
void waitEnter(const std::string& prompt);
bool askYesNo(const std::string& prompt, const std::string& invalidMessage);
int askNumber(const std::string& prompt, int min, int max,
              const std::string& notNumberMessage, const std::string& outOfRangeMessage);

}  // namespace console
