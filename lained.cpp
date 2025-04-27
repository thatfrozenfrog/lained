/*** includes ***/
#include <bits/stdc++.h>
#include <unistd.h>
#include <windows.h>

using namespace std;

/*** defines ***/
#define VERSION "0.0.1"
#define CTRL_KEY(k) ((k) & 0x1f)

enum editorKey
{
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    DEL_KEY,
    HOME_KEY,
    END_KEY,
    PAGE_UP,
    PAGE_DOWN,
    ENTER_KEY,
    ESC_KEY,
};

/*** data ***/
struct editorConfig {
    int cx, cy;
    int screenrows, screencols;
};

struct editorConfig E;
/*** terminal ***/
void die(const char *s)
{
    write(STDOUT_FILENO, "\x1b[2J", 4); // Clear screen
    write(STDOUT_FILENO, "\x1b[H", 3);
    perror(s);
    exit(1);
}

void setTerminalRawMode()
{
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode;
    GetConsoleMode(hStdin, &mode);
    mode &= ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT); // Disable line input and echo
    mode &= ~ENABLE_PROCESSED_INPUT;                  // Disable system processing (e.g. ^C, ^S, ^Q, ^Z)
    SetConsoleMode(hStdin, mode);
}

void resetTerminalMode()
{
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode;
    GetConsoleMode(hStdin, &mode);
    mode |= (ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT); // Enable line input and echo
    SetConsoleMode(hStdin, mode);
}

int editorReadKey()
{
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    INPUT_RECORD record;
    DWORD numRead;

    while (true)
    {
        if (!ReadConsoleInput(hStdin, &record, 1, &numRead))
            continue;
        if (record.EventType == KEY_EVENT && record.Event.KeyEvent.bKeyDown)
        {
            switch (record.Event.KeyEvent.wVirtualKeyCode)
            {
                case VK_UP:    return ARROW_UP;
                case VK_DOWN:  return ARROW_DOWN;
                case VK_LEFT:  return ARROW_LEFT;
                case VK_RIGHT: return ARROW_RIGHT;
                default:
                    if (record.Event.KeyEvent.uChar.AsciiChar != 0)
                        return record.Event.KeyEvent.uChar.AsciiChar;
                    break;
            }
        }
    }
}

int getCursorPosition(int *rows, int *cols)
{
    char buf[32];
    unsigned int i = 0;
    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4)
        return -1; // Failed to write escape sequence
    
    while (i < sizeof(buf) - 1)
    {
        if(read(STDIN_FILENO, &buf[i], 1) != 1)
            break; 
        if (buf[i] == 'R')
            break;
        i++;
    }
    buf[i] = '\0'; 
    
    if (buf[0] != '\x1b' || buf[1] != '[')
        return -1;
    
    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2)
        return -1;
    
    return 0;
}

int getWindowSize(int *rows, int *cols)
{
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    
    // Primary method: Get window size directly using Windows API
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi))
    {
        *rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
        *cols = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        return 0;
    }
    
    // Fallback method: Move cursor to bottom-right and check position
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
        return -1;
    
    return getCursorPosition(rows, cols);
}

/*** append buffer ***/
struct abuf
{
    char *b;
    int len;
};
#define ABUF_INIT {NULL, 0}
void abAppend(struct abuf *ab, const char *s, int len)
{
    char *newb = (char *)realloc(ab->b, ab->len + len);
    if (newb == NULL)
        return;
    memcpy(&newb[ab->len], s, len);
    ab->b = newb;
    ab->len += len;
}
void abFree(struct abuf *ab)
{
    free(ab->b);
    ab->len = 0;
}
/*** output ***/

void editorDrawRows(struct abuf *ab) {
    int y;
    for (y = 0; y < E.screenrows; y++) {
      if (y == E.screenrows / 3) {
        char welcome[80];
        int welcomelen = snprintf(welcome, sizeof(welcome),
          "LainEditor -- version %s", VERSION);
        if (welcomelen > E.screencols) welcomelen = E.screencols;
        int padding = (E.screencols - welcomelen) / 2;
        if (padding) {
          abAppend(ab, "~", 1);
          padding--;
        }
        while (padding--) abAppend(ab, " ", 1);
        abAppend(ab, welcome, welcomelen);
      } else {
        abAppend(ab, "~", 1);
      }
      abAppend(ab, "\x1b[K", 3);
      if (y < E.screenrows - 1) {
        abAppend(ab, "\r\n", 2);
      }
    }
  }

void editorDrawStatusBar()
{
    write(STDOUT_FILENO, "\x1b[7m", 4); // Invert colors
    write(STDOUT_FILENO, "LainEditor", 10);
    write(STDOUT_FILENO, "\x1b[m", 3); // Reset colors
}

void editorRefereshScreen()
{
    struct abuf ab = ABUF_INIT;

    abAppend(&ab, "\x1b[?25l", 6); // Hide cursor
    //abAppend(&ab, "\x1b[2J", 4); // Clear screen
    abAppend(&ab, "\x1b[H", 3);  // Move cursor to the top left corner
    editorDrawRows(&ab);

    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy + 1, E.cx + 1); // Move cursor to (E.cy, E.cx)
    abAppend(&ab, buf, strlen(buf)); // Move cursor to (E.cy, E.cx)

    //abAppend(&ab, "\x1b[H", 3); // Move cursor to the top left corner
    abAppend(&ab, "\x1b[?25h", 6); // Show cursor


    write(STDOUT_FILENO, ab.b, ab.len); // Write the buffer to stdout

    abFree(&ab); // Free the buffer
}

/*** input ***/
void editorMoveCursor(int key)
{
    switch (key)
    {
    case ARROW_LEFT:
        E.cx--;
        break;
    case ARROW_RIGHT:
        E.cx++;
        break;
    case ARROW_UP:
        E.cy--;
        break;
    case ARROW_DOWN:
        E.cy++;
        break;
    }
}

void editorProcessKeypress()
{
    int c = editorReadKey();
    switch (c)
    {
    case CTRL_KEY('q'):
        write(STDOUT_FILENO, "\x1b[2J", 4); // Clear screen
        write(STDOUT_FILENO, "\x1b[H", 3);
        exit(0);
        break;
    
    case ARROW_LEFT:
    case ARROW_RIGHT:
    case ARROW_UP:
    case ARROW_DOWN:
        editorMoveCursor(c);
        break;
    }
}


/*** init ***/

void initEditor()
{
    E.cx = 0;
    E.cy = 0;
    E.screenrows = 24;
    E.screencols = 80;
    if (getWindowSize(&E.screenrows, &E.screencols) == -1)
        die("getWindowSize failed");
}

int main()
{
    setTerminalRawMode();
    initEditor();
    while (1)
    {
        editorRefereshScreen();
        editorProcessKeypress();
    }
    resetTerminalMode();
    return 0;
}
