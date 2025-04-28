/*** includes ***/
#include <bits/stdc++.h>
#include <unistd.h>
#include <windows.h>

using namespace std;

/*** defines ***/
#define VERSION "0.0.1"
#define TAB_STOP 4
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
typedef struct erow {
    int size;
    int rsize;
    char *chars;
    char *render; 

} erow;
struct editorConfig {
    int cx, cy;
    int rx;
    int rowoff;
    int coloff;
    int screenrows, screencols;
    int numrows;
    erow *row;
    char *filename;
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
        // Read console input
        if (!ReadConsoleInput(hStdin, &record, 1, &numRead))
            continue;
            
        // Only process key down events
        if (record.EventType == KEY_EVENT && record.Event.KeyEvent.bKeyDown)
        {
            // Check for special keys first
            switch (record.Event.KeyEvent.wVirtualKeyCode)
            {
                // Arrow keys
                case VK_UP:     return ARROW_UP;
                case VK_DOWN:   return ARROW_DOWN;
                case VK_LEFT:   return ARROW_LEFT;
                case VK_RIGHT:  return ARROW_RIGHT;
                
                // Editing keys
                case VK_DELETE: return DEL_KEY;
                case VK_HOME:   return HOME_KEY;
                case VK_END:    return END_KEY;
                case VK_PRIOR:  return PAGE_UP;    // Page Up
                case VK_NEXT:   return PAGE_DOWN;  // Page Down
                case VK_RETURN: return ENTER_KEY;
                case VK_ESCAPE: return ESC_KEY;
                
                default:
                    if (record.Event.KeyEvent.dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) {
                        char c = record.Event.KeyEvent.uChar.AsciiChar;
                        if (c >= 'a' && c <= 'z') {
                            return CTRL_KEY(c);
                        }
                    }
                    
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

/*** row operations ***/

int editorRowCxToRx(erow *row, int rx)
{
    int curx = 0;
    int j;
    for (j = 0; j < rx; j++)
    {
        if (row->chars[j] == '\t')
            curx += (TAB_STOP - 1) - (curx % TAB_STOP);
        curx++;
    }
    return curx;
}

void editorUpdateRow(erow *row)
{
    int tabs = 0;
    for (int j = 0; j < row->size; j++)
        if (row->chars[j] == '\t')
            tabs++;
    free(row->render);
    row->render = (char *)malloc(row->size + tabs * (TAB_STOP - 1) + 1);
    int idx = 0;
    for (int j = 0; j < row->size; j++)
    {
        if (row->chars[j] == '\t')
        {
            row->render[idx++] = ' ';
            while (idx % TAB_STOP != 0)
                row->render[idx++] = ' ';
        }
        else
            row->render[idx++] = row->chars[j];
    }
    row->render[idx] = '\0';
    row->rsize = idx;
}

   
void editorAppendRow(char *s, size_t len)
{
    E.row = (erow *)realloc(E.row, sizeof(erow) * (E.numrows + 1));
    int at = E.numrows;
    E.row[at].size = len;
    E.row[at].chars = (char *)malloc(len + 1);
    memcpy(E.row[at].chars, s, len);
    E.row[at].chars[len] = '\0';

    E.row[at].rsize = 0;
    E.row[at].render = NULL;
    editorUpdateRow(&E.row[at]);
    E.numrows++;
}

/*** file i/o ***/

// Custom getline implementation for Windows
ssize_t win_getline(char **lineptr, size_t *n, FILE *stream) {
    char c;
    size_t pos = 0;
    
    if (*lineptr == NULL || *n == 0) {
        *n = 128; // Initial allocation size
        *lineptr = (char *)malloc(*n);
        if (!*lineptr) return -1;
    }
    
    while ((c = fgetc(stream)) != EOF) {
        if (pos + 1 >= *n) {
            size_t new_size = *n * 2;
            char *new_ptr = (char *)realloc(*lineptr, new_size);
            if (!new_ptr) return -1;
            *lineptr = new_ptr;
            *n = new_size;
        }
        
        (*lineptr)[pos++] = c;
        
        if (c == '\n') break; // End of line
    }
    
    if (pos == 0) return -1; // No characters read

    (*lineptr)[pos] = '\0';
    return pos;
}

void editorOpen(char *filename)
{
    free(E.filename);
    size_t len = strlen(filename) + 1;
    E.filename = (char*)malloc(len);
    if (E.filename) {
        strcpy(E.filename, filename);
    }

    FILE *fp = fopen(filename, "r");
    if (!fp) die("fopen failed");

    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;
    
    while ((linelen = win_getline(&line, &linecap, fp)) != -1) {
        while (linelen > 0 && (line[linelen - 1] == '\n' ||
                               line[linelen - 1] == '\r'))
            linelen--;
        editorAppendRow(line, linelen);
    }
    
    free(line);
    fclose(fp);
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

void editorScroll()
{
    E.rx = 0;
    if (E.cy < E.numrows)
        E.rx = editorRowCxToRx(&E.row[E.cy], E.cx);
    if (E.cy < E.rowoff) {
        E.rowoff = E.cy;
    }
    if (E.cy >= E.rowoff + E.screenrows) {
        E.rowoff = E.cy - E.screenrows + 1;
    }
    if (E.rx < E.coloff) {
        E.coloff = E.rx;
    }
    if (E.rx >= E.coloff + E.screencols) {
        E.coloff = E.rx - E.screencols + 1;
    }
}

void editorDrawRows(struct abuf *ab) {
    int y;
    for (y = 0; y < E.screenrows; y++) {
        int filerow = y + E.rowoff;
        if (filerow >= E.numrows) {
            if (E.numrows == 0 && y == E.screenrows / 3) {
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
        } else {
            int len = E.row[filerow].rsize - E.coloff;
            if (len < 0) len = 0;
            if (len > E.screencols) len = E.screencols;
            abAppend(ab, &E.row[filerow].render[E.coloff], len);
        }
        

        
        abAppend(ab, "\x1b[K", 3);
        abAppend(ab, "\r\n", 2);
        
    }
}

void editorDrawStatusBar(struct abuf *ab) {
    abAppend(ab, "\x1b[7m", 4); 
    char status[80];
    int len = snprintf(status, sizeof(status), "%.20s - %d lines",
        E.filename ? E.filename : "[No Name]", E.numrows);
    if (len > E.screencols) len = E.screencols;
    abAppend(ab, status, len);
    while (len < E.screencols) {
        if (E.screencols - len == 1) {
            abAppend(ab, "\x1b[0m", 4); 
            break;
        } else {
            abAppend(ab, " ", 1);
            len++;
        }
    }
    abAppend(ab, "\x1b[m", 3);
}

void editorRefereshScreen()
{
    editorScroll();

    struct abuf ab = ABUF_INIT;
    abAppend(&ab, "\x1b[?25l", 6);
    //abAppend(&ab, "\x1b[2J", 4); // Clear screen
    abAppend(&ab, "\x1b[H", 3);
    editorDrawRows(&ab);
    editorDrawStatusBar(&ab); 

    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH",
                (E.cy - E.rowoff) + 1,
                (E.rx - E.coloff) + 1); 
    
    
    abAppend(&ab, buf, strlen(buf)); 

    //abAppend(&ab, "\x1b[H", 3); 
    abAppend(&ab, "\x1b[?25h", 6);


    write(STDOUT_FILENO, ab.b, ab.len);

    abFree(&ab); 
}

/*** input ***/
void editorMoveCursor(int key)
{
    erow *row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
    switch (key)
    {
    case ARROW_LEFT:
        if (E.cx != 0)
            E.cx--;
        else if (E.cy > 0)
        {
            E.cy--;
            E.cx = E.row[E.cy].size;
        }
        break;
    case ARROW_RIGHT:
        if (row && E.cx < row->size)
            E.cx++;
        else if (row && E.cx == row->size)
        {
            E.cy++;
            E.cx = 0;
        }
        break;
    case ARROW_UP:
        if (E.cy > 0)
            E.cy--;
        break;
    case ARROW_DOWN:
        if (E.cy < E.numrows)
            E.cy++;
        break;
    }

    row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
    int rowlen = row ? row->size : 0;
    if (E.cx > rowlen)
        E.cx = rowlen;
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
    case HOME_KEY:
        E.cx = 0;
        break;
    case END_KEY:
        if (E.cy < E.numrows)
            E.cx = E.row[E.cy].size;
        break;
    case PAGE_UP:
    case PAGE_DOWN:
        {
            if (c == PAGE_UP)
                E.cy = E.rowoff;
            else {
                E.cy = E.rowoff + E.screenrows - 1;
                if (E.cy > E.numrows)
                    E.cy = E.numrows;
            }
            int times = E.screenrows;
            while (times--)
            {
                if (c == PAGE_UP)
                    editorMoveCursor(ARROW_UP);
                else
                    editorMoveCursor(ARROW_DOWN);
            }
            break; // Add the missing break statement here
        }
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
    E.rx = 0;
    E.rowoff = 0;
    E.coloff = 0;
    E.numrows = 0;
    E.row = NULL;
    E.filename = NULL;
    if (getWindowSize(&E.screenrows, &E.screencols) == -1)
        die("getWindowSize failed");
        
    E.screenrows -= 1;

}

int main(int argc, char *argv[])
{
    setTerminalRawMode();
    initEditor();
    if (argc >= 2)
    {
        editorOpen(argv[1]);
    }

    while (1)
    {
        editorRefereshScreen();
        editorProcessKeypress();
    }
    resetTerminalMode();
    return 0;
}
