#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <conio.h>
#include <windows.h>
#include <algorithm>
#include <sstream>
#include <iomanip>

using namespace std;

enum Mode {
    NORMAL,
    INSERT,
    COMMAND
};

class Editor {
private:
    vector<string> buffer;
    size_t cursorX = 0;
    size_t cursorY = 0;
    Mode mode = NORMAL;
    string filename;
    string statusMessage;
    string commandBuffer;
    string yankBuffer;
    bool isRunning = true;
    HANDLE consoleHandle;
    COORD screenSize;

    void initializeScreen() {
        consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        GetConsoleScreenBufferInfo(consoleHandle, &csbi);
        screenSize.X = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        screenSize.Y = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
        buffer.push_back("");
    }

    void clearScreen() {
        system("cls");
    }

    void moveCursor(int x, int y) {
        COORD pos = {(SHORT)x, (SHORT)y};
        SetConsoleCursorPosition(consoleHandle, pos);
    }

    void setColor(int color) {
        SetConsoleTextAttribute(consoleHandle, color);
    }

    void drawStatusBar() {
        moveCursor(0, screenSize.Y - 2);
        setColor(0x70); // White background, black text
        string modeStr;
        switch (mode) {
        case NORMAL: modeStr = "NORMAL"; break;
        case INSERT: modeStr = "INSERT"; break;
        case COMMAND: modeStr = "COMMAND"; break;
        }

        string status = modeStr + " | " + filename + " | " +
                        "Line " + to_string(cursorY + 1) + "/" + to_string(buffer.size()) +
                        " Col " + to_string(cursorX + 1);
        cout << status;
        for (int i = status.length(); i < screenSize.X; i++) cout << " ";

        // Status message line
        moveCursor(0, screenSize.Y - 1);
        cout << statusMessage;
        for (int i = statusMessage.length(); i < screenSize.X; i++) cout << " ";

        setColor(0x07); // Reset to default colors
    }

    void drawBuffer() {
        clearScreen();
        for (size_t i = 0; i < buffer.size() && i < screenSize.Y - 2; i++) {
            moveCursor(0, i);
            cout << setw(4) << i + 1 << " " << buffer[i];
        }
        drawStatusBar();
        moveCursor(cursorX + 5, cursorY); // +5 for line numbers
    }

    void handleNormalMode(int ch) {
        switch (ch) {
        case 'h': if (cursorX > 0) cursorX--; break;
        case 'l': if (cursorX < buffer[cursorY].length()) cursorX++; break;
        case 'k': if (cursorY > 0) cursorY--; break;
        case 'j': if (cursorY < buffer.size() - 1) cursorY++; break;
        case 'i':
            mode = INSERT;
            statusMessage = "-- INSERT --";
            break;
        case ':':
            mode = COMMAND;
            commandBuffer = ":";
            statusMessage = commandBuffer;
            break;
        case 'x': // Delete character under cursor
            if (!buffer[cursorY].empty() && cursorX < buffer[cursorY].length()) {
                buffer[cursorY].erase(cursorX, 1);
            }
            break;
        case 'd': // Delete line
            if (ch == 'd') {
                yankBuffer = buffer[cursorY];
                buffer.erase(buffer.begin() + cursorY);
                if (buffer.empty()) buffer.push_back("");
                if (cursorY >= buffer.size()) cursorY = buffer.size() - 1;
            }
            break;
        case 'p': // Paste yanked text
            if (!yankBuffer.empty()) {
                buffer.insert(buffer.begin() + cursorY + 1, yankBuffer);
            }
            break;
        }

        // Ensure cursor stays within bounds
        if (cursorX >= buffer[cursorY].length())
            cursorX = buffer[cursorY].empty() ? 0 : buffer[cursorY].length() - 1;
    }

    void handleInsertMode(int ch) {
        if (ch == 27) { // ESC key
            mode = NORMAL;
            statusMessage = "";
            if (cursorX > 0) cursorX--;
            return;
        }

        if (ch == '\r') { // Enter key
            string rest = buffer[cursorY].substr(cursorX);
            buffer[cursorY].erase(cursorX);
            buffer.insert(buffer.begin() + cursorY + 1, rest);
            cursorX = 0;
            cursorY++;
        }
        else if (ch == '\b') { // Backspace
            if (cursorX > 0) {
                buffer[cursorY].erase(cursorX - 1, 1);
                cursorX--;
            }
            else if (cursorY > 0) {
                cursorX = buffer[cursorY - 1].length();
                buffer[cursorY - 1] += buffer[cursorY];
                buffer.erase(buffer.begin() + cursorY);
                cursorY--;
            }
        }
        else {
            buffer[cursorY].insert(cursorX, 1, (char)ch);
            cursorX++;
        }
    }

    void handleCommandMode(int ch) {
        if (ch == 27) { // ESC key
            mode = NORMAL;
            commandBuffer = "";
            statusMessage = "";
            return;
        }
        else if (ch == '\r') { // Enter key
            executeCommand(commandBuffer);
            mode = NORMAL;
            commandBuffer = "";
            return;
        }
        else if (ch == '\b' && !commandBuffer.empty()) { // Backspace
            commandBuffer.pop_back();
        }
        else {
            commandBuffer += (char)ch;
        }
        statusMessage = commandBuffer;
    }

    void executeCommand(const string& cmd) {
        if (cmd == ":q" || cmd == ":quit") {
            isRunning = false;
        }
        else if (cmd == ":w" || cmd.substr(0, 2) == ":w") {
            if (cmd.length() > 3) {
                filename = cmd.substr(3);
            }
            saveFile();
        }
        else if (cmd.substr(0, 2) == ":e") {
            if (cmd.length() > 3) {
                filename = cmd.substr(3);
                loadFile();
            }
        }
    }

    void loadFile() {
        buffer.clear();
        ifstream file(filename);
        if (file.is_open()) {
            string line;
            while (getline(file, line)) {
                buffer.push_back(line);
            }
            if (buffer.empty()) buffer.push_back("");
            file.close();
            statusMessage = "File loaded: " + filename;
        }
        else {
            buffer.push_back("");
            statusMessage = "New file: " + filename;
        }
        cursorX = cursorY = 0;
    }

    void saveFile() {
        if (filename.empty()) {
            statusMessage = "No filename specified";
            return;
        }

        ofstream file(filename);
        if (file.is_open()) {
            for (const auto& line : buffer) {
                file << line << '\n';
            }
            file.close();
            statusMessage = "File saved: " + filename;
        }
        else {
            statusMessage = "Error: Could not save file";
        }
    }

public:
    Editor() {
        initializeScreen();
    }

    void run() {
        while (isRunning) {
            drawBuffer();

            if (int ch = _getch()) {
                switch (mode) {
                case NORMAL:
                    handleNormalMode(ch);
                    break;
                case INSERT:
                    handleInsertMode(ch);
                    break;
                case COMMAND:
                    handleCommandMode(ch);
                    break;
                }
            }
        }
    }
};

int main(int argc, char* argv[]) {
    // Set console title
    SetConsoleTitle("Mini Vim Editor");

    // Enable virtual terminal processing for better console handling
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);

    Editor editor;
    editor.run();
    return 0;
}
