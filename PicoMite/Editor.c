/***********************************************************************************************************************
PicoMite MMBasic

Editor.c

<COPYRIGHT HOLDERS>  Geoff Graham, Peter Mather
Copyright (c) 2021, <COPYRIGHT HOLDERS> All rights reserved. 
Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met: 
1.	Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
2.	Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer
    in the documentation and/or other materials provided with the distribution.
3.	The name MMBasic be used when referring to the interpreter in any documentation and promotional material and the original copyright message be displayed 
    on the console at startup (additional copyright messages may be added).
4.	All advertising materials mentioning features or use of this software must display the following acknowledgement: This product includes software developed 
    by the <copyright holder>.
5.	Neither the name of the <copyright holder> nor the names of its contributors may be used to endorse or promote products derived from this software 
    without specific prior written permission.
THIS SOFTWARE IS PROVIDED BY <COPYRIGHT HOLDERS> AS IS AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDERS> BE LIABLE FOR ANY DIRECT, 
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; 
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, 
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 

************************************************************************************************************************/

#include "MMBasic_Includes.h"
#include "Hardware_Includes.h"


#define CTRLKEY(a) (a & 0x1f)


#define DISPLAY_CLS             1
#define REVERSE_VIDEO           3
#define CLEAR_TO_EOL            4
#define CLEAR_TO_EOS            5
#define SCROLL_DOWN             6
#define DRAW_LINE               7

#define GUI_C_NORMAL            WHITE
#define GUI_C_BCOLOUR           BLACK
#define GUI_C_COMMENT           YELLOW
#define GUI_C_KEYWORD           CYAN
#define GUI_C_QUOTE             MAGENTA
#define GUI_C_NUMBER            GREEN
#define GUI_C_LINE              MAGENTA
#define GUI_C_STATUS            WHITE

//======================================================================================
//      Attribute               VT100 Code      VT 100 Colour       LCD Screen Colour
//======================================================================================
#define VT100_C_NORMAL          "\033[37m"      // White            Foreground Colour
#define VT100_C_COMMENT         "\033[33m"      // Yellow               Yellow
#define VT100_C_KEYWORD         "\033[36m"      // Cyan                 Cyan
#define VT100_C_QUOTE           "\033[35m"      // Green                Green
#define VT100_C_NUMBER          "\033[32m"      // Red                  Red
#define VT100_C_LINE            "\033[37m"      // White                Grey
#define VT100_C_STATUS          "\033[37m"      // Black                Brown
#define VT100_C_ERROR           "\033[31m"      // Red                  Red

// these two are the only global variables, the default place for the cursor when the editor opens
unsigned char *StartEditPoint = NULL;
int StartEditChar = 0;
static bool markmode=false;
extern void routinechecks(void);
int optioncolourcodesave;
#if !defined(LITE)
#ifdef PICOMITEVGA
int editactive=0;
static int r_on=0;
void DisplayPutClever(char c){
    if(DISPLAY_TYPE==MONOVGA && markmode && Option.ColourCode && ytilecount==12 && gui_font==1){
    if(c >= FontTable[gui_font >> 4][2] && c < FontTable[gui_font >> 4][2] + FontTable[gui_font >> 4][3]) {
        if(CurrentX + gui_font_width > HRes) {
            DisplayPutClever('\r');
            DisplayPutClever('\n');
        }
    }

    // handle the standard control chars
    switch(c) {
        case '\b':  CurrentX -= gui_font_width;
            if (CurrentX < 0) CurrentX = 0;
            return;
        case '\r':  CurrentX = 0;
                    return;
        case '\n':  CurrentY += gui_font_height;
                    if(CurrentY + gui_font_height >= VRes) {
                        ScrollLCD(CurrentY + gui_font_height - VRes);
                        CurrentY -= (CurrentY + gui_font_height - VRes);
                    }
                    return;
        case '\t':  do {
                        DisplayPutClever(' ');
                    } while((CurrentX/gui_font_width) % 4 /******Option.Tab*/);
                    return;
    }
    if(r_on)tilebcols[CurrentY/gui_font_height*X_TILE+CurrentX/gui_font_width]=0x1111;
    else tilebcols[CurrentY/gui_font_height*X_TILE+CurrentX/gui_font_width]=Option.VGABC;
    CurrentX += gui_font_width;
    } else DisplayPutC(c);
}
#endif

void DisplayPutS(char *s) {
    while(*s) DisplayPutC(*s++);
}


    int OriginalFC, OriginalBC;                                     // the original fore/background colours used by MMBasic

    #define MMputchar           SerialConsolePutC
    #define MMPrintString       SSPrintString
#ifdef PICOMITEVGA
    #define MX470PutC(c)        DisplayPutClever(c)
#else
    #define MX470PutC(c)        DisplayPutC(c)
#endif
    #define MX470Scroll(n)      if(Option.DISPLAY_CONSOLE)ScrollLCD(n)

//    #define dx(...) {char s[140];sprintf(s,  __VA_ARGS__); SerUSBPutS(s); SerUSBPutS("\r\n");}

    void MX470PutS(char *s, int fc, int bc) {
        int tfc, tbc;
        tfc = gui_fcolour; tbc = gui_bcolour;
        gui_fcolour = fc; gui_bcolour = bc;
        DisplayPutS(s);
        gui_fcolour = tfc; gui_bcolour = tbc;
    }


    void MX470Cursor(int x, int y) {
        if(Option.DISPLAY_CONSOLE) {
            CurrentX = x;
            CurrentY = y;
        }
    }

    void MX470Display(int fn) {
        int t;
        if(!Option.DISPLAY_CONSOLE) return;
        switch(fn) {
            case DISPLAY_CLS:       ClearScreen(gui_bcolour);
                                    break;
#ifdef PICOMITEVGA
            case REVERSE_VIDEO:     if(DISPLAY_TYPE==MONOVGA && Option.ColourCode && ytilecount==12 && gui_font==1){
                                        r_on^=1;
                                    } else {
                                        t = gui_fcolour;
                                        gui_fcolour = gui_bcolour;
                                        gui_bcolour = t;
                                    }
                                    break;
            case CLEAR_TO_EOL:      DrawBox(CurrentX, CurrentY, HRes-1, CurrentY + gui_font_height-1, 0, 0, DISPLAY_TYPE==MONOVGA ? 0 : gui_bcolour);
                                    if(DISPLAY_TYPE==MONOVGA && Option.ColourCode && ytilecount==12 && gui_font==1){
                                        for(int x=CurrentX/gui_font_width;x<X_TILE;x++){
                                                tilefcols[CurrentY/gui_font_height*X_TILE+x]=Option.VGAFC;
                                                tilebcols[CurrentY/gui_font_height*X_TILE+x]=Option.VGABC;
                                        }
                                    }
                                    break;
#else
            case REVERSE_VIDEO:     t = gui_fcolour;
                                    gui_fcolour = gui_bcolour;
                                    gui_bcolour = t;
                                    break;
            case CLEAR_TO_EOL:      DrawBox(CurrentX, CurrentY, HRes-1, CurrentY + gui_font_height-1, 0, 0, gui_bcolour);
                                    break;
#endif
            case CLEAR_TO_EOS:      DrawBox(CurrentX, CurrentY, HRes-1, CurrentY + gui_font_height-1, 0, 0, gui_bcolour);
                                    DrawRectangle(0, CurrentY + gui_font_height, HRes-1, VRes-1, gui_bcolour);
                                    break;
            case SCROLL_DOWN:
                                    break;
            case DRAW_LINE:         DrawBox(0, gui_font_height * (Option.Height - 2), HRes - 1, VRes - 1, 0, 0, (DISPLAY_TYPE==MONOVGA ? 0 :gui_bcolour));
                                    DrawLine(0, VRes - gui_font_height - 6, HRes - 1, VRes - gui_font_height - 6, 1, GUI_C_LINE);
#ifdef PICOMITEVGA
                                    if(DISPLAY_TYPE==MONOVGA && Option.ColourCode && ytilecount==12 && gui_font==1)for(int i=0; i<80; i++)tilefcols[38*X_TILE+i]=Option.VGAFC;
#endif
                                    CurrentX = 0; CurrentY = VRes - gui_font_height;
                                    break;
        }
}



/********************************************************************************************************************************************
 THE EDIT COMMAND
********************************************************************************************************************************************/

unsigned char *EdBuff=NULL;                     // the buffer used for editing the text
int nbrlines;                     // size of the text held in the buffer (in lines)
int VWidth, VHeight;              // editing screen width and height in characters
int edx, edy;                     // column and row at the top left hand corner of the editing screen (in characters)
int curx, cury;                   // column and row of the current cursor (in characters) relative to the top left hand corner of the editing screen
unsigned char *txtp;                       // position of the current cursor in the text being edited
int drawstatusline;               // true if the status line needs to be redrawn on the next keystroke
int insert;                       // true if the editor is in insert text mode
int tempx;                        // used to track the prefered x position when up/down arrowing
int TextChanged;                  // true if the program has been modified and therefor a save might be required

#define EDIT  1                   // used to select the status line string
#define MARK  2

void FullScreenEditor(int x, int y, char *fname);
char *findLine(int ln, int *inmulti);
void printLine(int ln); 
void printScreen(void);
void SCursor(int x, int y);
int editInsertChar(unsigned char c, char *multi);
void PrintFunctKeys(int);
void PrintStatus(void);
void SaveToProgMemory(void);
void editDisplayMsg(unsigned char *msg);
void GetInputString(unsigned char *prompt);
void Scroll(void);
void ScrollDown(void);
void MarkMode(unsigned char *cb, unsigned char *buf);
void PositionCursor(unsigned char *curp);
extern void setterminal(void);

#define MAXCLIP 1024
// edit command:
//  EDIT              Will run the full screen editor on the current program memory, if run after an error will place the cursor on the error line
void cmd_edit(void) {
    unsigned char *fromp, *p=NULL;
    int y, x;
    char name[FF_MAX_LFN] ;   
    getargs(&cmdline,1,(unsigned char *)",");
    optioncolourcodesave=Option.ColourCode;
#ifdef PICOMITEVGA
    editactive=1;
    int mode = DISPLAY_TYPE;
    DISPLAY_TYPE=MONOVGA;
    memset(WriteBuf, 0, 38400);
    ResetDisplay();
#endif
    if(CurrentLinePtr) error("Invalid in a program");
    if(Option.ColourCode) {
        gui_fcolour = WHITE;
        gui_bcolour = BLACK;
    }
    if(Option.DISPLAY_CONSOLE == true && gui_font_width > 16) error("Font is too large");
    if(Option.DISPLAY_TYPE>=VIRTUAL && WriteBuf)FreeMemorySafe((void **)&WriteBuf);
    ClearVars(0);
    ClearRuntime();
#ifdef PICOMITEWEB
    cleanserver();
#endif
#ifdef PICOMITEVGA
    if(mode==COLOURVGA) SetFont(1) ;
#endif
    EdBuff = GetTempMemory(EDIT_BUFFER_SIZE);
    *EdBuff = 0;

    VHeight = Option.Height - 2;
    VWidth = Option.Width;
    edx = edy = curx = cury = y = x = tempx = 0;
    txtp = EdBuff;
    *tknbuf = 0;
    if(argc==0){
        fromp  = ProgMemory;
        p = EdBuff;
        nbrlines = 0;
        while(*fromp != 0xff) {
            if(*fromp == T_NEWLINE) {
                if(StartEditPoint >= ProgMemory) {
                    if(StartEditPoint == fromp) {
                        y = nbrlines;                                   // we will start editing at this line
                        tempx = x = StartEditChar;
                        txtp = p + StartEditChar;
                    }
                } else {
                    if(StartEditPoint == (unsigned char *)nbrlines) {
                        y = nbrlines;
                        tempx = x = StartEditChar;
                        txtp = p + StartEditChar;
                    }
                }
                nbrlines++;
                fromp = llist(p, fromp);                                // otherwise expand the line
                if(!(nbrlines==1 && p[0]=='\'' && p[1]=='#')){
                    p += strlen((char *)p);
                    *p++ = '\n'; *p = 0;
                }
            }
            // finally, is it the end of the program?
            if(fromp[0] == 0 || fromp[0] == 0xff) break;
        }
    } else {
        char *fname = (char *)getFstring(argv[0]);
        char c;
        strcpy(name,fname);
        if(!ExistsFile(name)){
            if (strchr(name, '.') == NULL) strcat(name, ".bas");
        }
        if(!fstrstr(name,".bas"))Option.ColourCode=0;
        if(ExistsFile(name)){
            int fnbr1;
            fnbr1 = FindFreeFileNbr();
            BasicFileOpen(name, fnbr1, FA_READ);
            p=EdBuff;
            do
            { // while waiting for the end of file
                c = FileGetChar(fnbr1);
                if(c=='\n')nbrlines++;
                if(c=='\r')continue;
                *p++=c;
            } while (!FileEOF(fnbr1));
            p++;
            FileClose(fnbr1);
        }
        txtp=EdBuff;
        tempx=x=0;
    }
    if(nbrlines == 0) nbrlines++;
    if(p > EdBuff) --p;
    *p = 0;                                                         // erase the last line terminator
    setterminal();

    MMPrintString("\033[?1000h");                                   // Tera Term turn on mouse click report in VT200 mode
    MMPrintString("\0337\033[2J\033[H");                            // vt100 clear screen and home cursor
    MX470Display(DISPLAY_CLS);                                      // clear screen on the MX470 display only
    SCursor(0, 0);
    PrintFunctKeys(EDIT);

    if(nbrlines > VHeight) {
        edy = y - VHeight/2;                                        // edy is the line displayed at the top
        if(edy < 0) edy = 0;                                        // compensate if we are near the start
        y = y - edy;                                                // y is the line on the screen
    }
    m_alloc(M_VAR);                                                 //clean up clipboard usage
    FullScreenEditor(x,y, argc==1 ? name: NULL);
    memset(tknbuf, 0, STRINGSIZE);                                  // zero this so that nextstmt is pointing to the end of program
    MMCharPos = 0;
}



void FullScreenEditor(int xx, int yy, char *fname) {
  int c, i;
  unsigned char buf[MAXCLIP+2], clipboard[MAXCLIP+2];
  unsigned char *p, *tp, BreakKeySave;
  static char currdel=0, nextdel=0, lastdel=0, multi=false;
#ifdef PICOMITEVGA
  int OptionY_TILESave;
  int ytilecountsave;
  ytilecountsave=ytilecount;
  OptionY_TILESave=Y_TILE;
  if(!Option.ColourCode)ytilecount=16;
  else {
    ytilecount=12;
    Y_TILE=40;
  }
#endif
  printScreen();                                                  // draw the screen
  SCursor(xx, yy);
  drawstatusline = true;
  unsigned char lastkey = 0;
  int y, statuscount;
  clipboard[0] = 0;
  buf[0]=0;
  insert = true;
  TextChanged = false;
    BreakKeySave = BreakKey;
    BreakKey = 0;
    while(1) {
      statuscount = 0;
      do {
          ShowCursor(true);
          c = MMInkey();
          if(statuscount++ == 5000) PrintStatus();
      } while(c == -1);
      ShowCursor(false);

      if(drawstatusline) PrintFunctKeys(EDIT);
      drawstatusline = false;
      if(c == TAB) {
            strcpy((char *)buf, "        ");
            buf[Option.Tab - ((edx + curx) % Option.Tab)] = 0;
      } else {
          buf[0] = c;
          buf[1] = 0;
      }
      do {
          if(buf[0] == BreakKeySave) buf[0] = ESC;                                // if the user tried to break turn it into an escape
          switch(buf[0]) {

              case '\r':
              case '\n':  // first count the spaces at the beginning of the line
                          if(txtp != EdBuff && (*txtp == '\n' || *txtp == 0)) {   // we only do this if we are at the end of the line
                              for(tp = txtp - 1, i = 0; *tp != '\n' && tp >= EdBuff; tp--)
                                  if(*tp != ' ')
                                      i = 0;                                      // not a space
                                  else
                                      i++;                                        // potential space at the start
                              if(tp == EdBuff && *tp == ' ') i++;                 // correct for a counting error at the start of the buffer
                              if(buf[1] != 0)
                                  i = 0;                                          // do not insert spaces if buffer too small or has something in it
                              else
                                  buf[i + 1] = 0;                                 // make sure that the end of the buffer is zeroed
                              while(i) buf[i--] = ' ';                            // now, place our spaces in the typeahead buffer
                          }
                          if(!editInsertChar('\n',&multi)) break;                        // insert the newline
                          TextChanged = true;
                          nbrlines++;
                          if(!(cury < VHeight - 1))                               // if we are NOT at the bottom
                              edy++;                                              // otherwise scroll
                          printScreen();                                          // redraw everything
                          PositionCursor(txtp);
                          break;

              case CTRLKEY('E'):
              case UP:    if(cury == 0 && edy == 0) break;
                          if(*txtp == '\n') txtp--;                               // step back over the terminator if we are right at the end of the line
                          while(txtp != EdBuff && *txtp != '\n') txtp--;          // move to the beginning of the line
                          if(txtp != EdBuff) {
                              txtp--;                                             // step over the terminator to the end of the previous line
                              while(txtp != EdBuff && *txtp != '\n') txtp--;      // move to the beginning of that line
                              if(*txtp == '\n') txtp++;                           // and position at the start
                          }
                          for(i = 0; i < edx + tempx && *txtp != 0 && *txtp != '\n'; i++, txtp++);  // move the cursor to the column

                          if(cury > 2 || edy == 0) {                              // if we are more that two lines from the top
                              if(cury > 0) SCursor(i, cury - 1);                  // just move the cursor up
                          }
                          else if(edy > 0) {                                      // if we are two lines or less from the top
                              curx = i;
                              ScrollDown();
                          }
                          PositionCursor(txtp);
                          break;

              case CTRLKEY('X'):
              case DOWN:  p = txtp;
                          while(*p != 0 && *p != '\n') p++;                       // move to the end of this line
                          if(*p == 0) break;                                      // skip if it is at the end of the file
                          p++;                                                    // step over the line terminator to the start of the next line
                          for(i = 0; i < edx + tempx && *p != 0 && *p != '\n'; i++, p++);  // move the cursor to the column
                          txtp = p;

                          if(cury < VHeight - 3 || edy + VHeight == nbrlines) {
                              if(cury < VHeight - 1) SCursor(i, cury + 1);
                          }
                          else if(edy + VHeight < nbrlines) {
                              curx = i;
                              Scroll();
                          }
                          PositionCursor(txtp);
                          break;

              case CTRLKEY('S'):
              case LEFT:  if(txtp == EdBuff) break;
                          if(*(txtp - 1) == '\n') {                               // if at the beginning of the line wrap around
                              buf[1] = UP;
                              buf[2] = END;
                              buf[3] = 1;
                              buf[4] = 0;
                          } else {
                              txtp--;
                              PositionCursor(txtp);
                          }
                          break;

              case CTRLKEY('D'):
              case RIGHT: if(*txtp == '\n') {                                   // if at the end of the line wrap around
                              buf[1] = HOME;
                              buf[2] = DOWN;
                              buf[3] = 0;
                              break;
                          }
                          if(curx >= VWidth) {
                              editDisplayMsg((unsigned char *)" LINE IS TOO LONG ");
                              break;
                          }

                          if(*txtp != 0) txtp++;                                  // now we can move the cursor
                          PositionCursor(txtp);
                          break;

              // backspace
              case BKSP:  if(txtp == EdBuff) break;
                          if(*(txtp - 1) == '\n') {                               // if at the beginning of the line wrap around
                              buf[1] = UP;
                              buf[2] = END;
                              buf[3] = DEL;
                              buf[4] = 0;
                              break;
                          }
                          // find how many spaces are between the cursor and the start of the line
                          for(p = txtp - 1; *p == ' ' && p != EdBuff; p--);
                          if((p == EdBuff || *p == '\n') && txtp - p > 1) {
                              i = txtp - p - 1;
                              // we have have the number of continuous spaces between the cursor and the start of the line
                              // now figure out the number of backspaces to the nearest tab stop

                                i = (i % Option.Tab); if(i == 0) i = Option.Tab;
                                // load the corresponding number of deletes in the type ahead buffer
                                buf[i + 1] = 0;
                                while(i--) {
                                    buf[i + 1] = DEL;
                                    txtp--;
                                }
                                // and let the delete case take care of deleting the characters
                              PositionCursor(txtp);
                              break;
                          }
                          // this is just a normal backspace (not a tabbed backspace)
                          txtp--;
                          PositionCursor(txtp);
                          // fall through to delete the char

              case CTRLKEY(']'):
              case DEL: if(*txtp == 0) break;
                          p = txtp;
                          c = *p;
                          currdel=*p;
                          if(p!=EdBuff+EDIT_BUFFER_SIZE-1)nextdel=p[1];
                          else nextdel=0;
                          if(p!=EdBuff){
                          lastdel=*(--p);
                          p++;
                          } else lastdel=0;
                          while(*p) {
                              p[0] = p[1];
                              p++;
                          }
                          if(c == '\n') {
                              printScreen();
                              nbrlines--;
                          }
                          else
                              printLine(edy + cury);
                          TextChanged = true;
                          PositionCursor(txtp);
                          if(currdel=='/' && nextdel=='*' && Option.ColourCode)printScreen();
                          if(currdel=='*' && nextdel=='/' && Option.ColourCode)printScreen();
                          if(currdel=='/' && lastdel=='*' && Option.ColourCode)printScreen();
                          if(currdel=='*' && lastdel=='/' && Option.ColourCode)printScreen();
                          break;

              case CTRLKEY('N'):
              case INSERT:insert = !insert;
                          break;

              case CTRLKEY('U'):
              case HOME:  if(txtp == EdBuff) break;
                          if(lastkey == HOME || lastkey == CTRLKEY('U')) {
                              edx = edy = curx = cury = 0;
                              txtp = EdBuff;
                              MMPrintString("\033[2J\033[H");                     // vt100 clear screen and home cursor
                              MX470Display(DISPLAY_CLS);                          // clear screen on the MX470 display only
                              printScreen();
                              PrintFunctKeys(EDIT);
                              PositionCursor(txtp);
                              break;
                          }
                          if(*txtp == '\n') txtp--;                               // step back over the terminator if we are right at the end of the line
                          while(txtp != EdBuff && *txtp != '\n') txtp--;          // move to the beginning of the line
                          if(*txtp == '\n') txtp++;                               // skip if no more lines above this one
                          PositionCursor(txtp);
                          break;

              case CTRLKEY('K'):
              case END: if(*txtp == 0) break;                                     // already at the end
                          if(lastkey == END || lastkey == CTRLKEY('K')) {         // jump to the end of the file
                              i = 0; p = txtp = EdBuff;
                              while(*txtp != 0) {
                                  if(*txtp == '\n') { p = txtp + 1; i++; }
                                  txtp++;
                              }

                              if(i >= VHeight) {
                                  edy = i - VHeight + 1;
                                  printScreen();
                                  cury = VHeight - 1;
                              } else {
                                  cury = i;
                              }
                              txtp = p;
                              curx = 0;
                          }

                          while(curx < VWidth && *txtp != 0 && *txtp != '\n') {
                              txtp++;
                              PositionCursor(txtp);
                          }
                          if(curx > VWidth) editDisplayMsg((unsigned char *)" LINE IS TOO LONG ");
                          break;

              case CTRLKEY('P'):
              case PUP: if(edy == 0) {                                            // if we are already showing the top of the text
                              buf[1] = HOME;                                      // force the editing point to the start of the text
                              buf[2] = HOME;
                              buf[3] = 0;
                              break;
                          } else if(edy >= VHeight - 1) {                         // if we can scroll a full screenfull
                              i = VHeight + 1;
                              edy -= VHeight;
                          } else {                                                // if it is less than a full screenfull
                              i = edy + 1;
                              edy = 0;
                          }
                          while(i--) {
                              if(*txtp == '\n') txtp--;                           // step back over the terminator if we are right at the end of the line
                              while(txtp != EdBuff && *txtp != '\n') txtp--;      // move to the beginning of the line
                              if(txtp == EdBuff) break;                           // skip if no more lines above this one
                          }
                          if(txtp != EdBuff) txtp++;                              // and position at the start of the line
                          for(i = 0; i < edx + curx && *txtp != 0 && *txtp != '\n'; i++, txtp++);  // move the cursor to the column
                          printScreen();
                          PositionCursor(txtp);
                          break;

              case CTRLKEY('L'):
              case PDOWN: if(nbrlines <= edy + VHeight + 1) {                     // if we are already showing the end of the text
                              buf[1] = END;                                       // force the editing point to the end of the text
                              buf[2] = END;
                              buf[3] = 0;
                              break;                                              // cursor to the top line
                          } else if(nbrlines - edy - VHeight >= VHeight) {        // if we can scroll a full screenfull
                              edy += VHeight;
                              i = VHeight;
                          } else {                                                // if it is less than a full screenfull
                              i = nbrlines - VHeight - edy;
                              edy = nbrlines - VHeight;
                          }
                          if(*txtp == '\n') i--;                                  // compensate if we are right at the end of a line
                          while(i--) {
                              if(*txtp == '\n') txtp++;                           // step over the terminator if we are right at the start of the line
                              while(*txtp != 0 && *txtp != '\n') txtp++;          // move to the end of the line
                              if(*txtp == 0) break;                               // skip if no more lines after this one
                          }
                          if(txtp != EdBuff) txtp++;                              // and position at the start of the line
                          for(i = 0; i < edx + curx && *txtp != 0 && *txtp != '\n'; i++, txtp++);  // move the cursor to the column
                          //y = cury;
                          printScreen();
                          PositionCursor(txtp);
                          break;

              // Abort without saving
              case ESC:     uSec(50000);                                                                // wait 50ms to see if anything more is coming
                            routinechecks();
                            if(MMInkey() == '[' && MMInkey() == 'M') {
                                // received escape code for Tera Term reporting a mouse click or scroll wheel movement
                                int c, x, y;
                                c = MMInkey();  x = MMInkey() - '!';  y  = MMInkey() - '!';
                                if(c == 'e' || c == 'a') {                      // Tera Term - SHIFT + mouse-wheel-rotate-down
                                    buf[1] = UP;
                                    buf[2] = 0;
                                } else if(c == 'd' || c == '`') {               // Tera Term - SHIFT + mouse-wheel-rotate-up
                                    buf[1] = DOWN;
                                    buf[2] = 0;
                                } else if(c == ' ' && x >= 0 && x < VWidth && y >= 0 && y < VHeight) {  // c == ' ' means mouse down and no shift, ctrl, etc
                                    // first position on the y axis
                                    while(*txtp != 0 && y > cury)                                       // assume we have to move down the screen
                                        if(*txtp++ == '\n') cury++;
                                    while(txtp != EdBuff && y < cury)                                   // assume we have to move up the screen
                                        if(*--txtp == '\n') cury--;
                                    while(txtp != EdBuff && *(txtp - 1) != '\n') txtp--;                // move to the beginning of the line
                                    for(curx = 0; curx < x && *txtp && *txtp != '\n'; curx++) txtp++;   // now position on the x axis
                                    PositionCursor(txtp);
                                }
                                break;
                            }
                            // this must be an ordinary escape (not part of an escape code)
                            if(TextChanged) {
                                GetInputString((unsigned char *)"Exit and discard all changes (Y/N): ");
                                if(toupper(*inpbuf) != 'Y') break;
                            }
                            // fall through to the normal exit

              case CTRLKEY('Q'):   // Save and exit
              case F1:             // Save and exit
              case CTRLKEY('W'):   // Save, exit and run
              case F2:             // Save, exit and run
                            c=buf[0];
                            MMPrintString("\033[?1000l");                         // Tera Term turn off mouse click report in vt200 mode
                            MMPrintString("\0338\033[2J\033[H");                  // vt100 clear screen and home cursor
                            gui_fcolour = GUI_C_NORMAL;
                            MMPrintString(VT100_C_NORMAL);
                            gui_fcolour = PromptFC;
                            gui_bcolour = PromptBC;
                            MX470Cursor(0, 0);                                // home the cursor
                            MX470Display(DISPLAY_CLS);                        // clear screen on the MX470 display only
                            MMPrintString("\033[0m");
                            BreakKey = BreakKeySave;
                            Option.ColourCode=optioncolourcodesave;
#ifdef PICOMITEVGA
                            editactive=0;
                            Y_TILE=OptionY_TILESave;
                            ytilecount=ytilecountsave;
#endif                          
                            if(c != ESC && TextChanged && fname==NULL) SaveToProgMemory();
                            if(c != ESC && TextChanged && fname) {
                                int fnbr1;
                                if(ExistsFile(fname)){
                                    char backup[FF_MAX_LFN];
                                    strcpy(backup,fname);
                                    strcat(backup,".bak");
                                    fnbr1 = FindFreeFileNbr();
                                    BasicFileOpen(fname, fnbr1, FA_READ);
                                    int fnbr2 = FindFreeFileNbr();
                                    BasicFileOpen(backup, fnbr2, FA_WRITE | FA_CREATE_ALWAYS);
                                    while (!FileEOF(fnbr1))
                                    { // while waiting for the end of file
                                        FilePutChar(FileGetChar(fnbr1),fnbr2);
                                    }
                                    FileClose(fnbr1);
                                    FileClose(fnbr2);
                                }
                                fnbr1=FindFreeFileNbr();
                                BasicFileOpen(fname, fnbr1, FA_WRITE | FA_CREATE_ALWAYS);
                                p=EdBuff;
                                do
                                { // while waiting for the end of file
                                    c = *p++;
                                    if(c=='\n')FilePutChar('\r',fnbr1);
                                    FilePutChar(c,fnbr1);
                                } while (*p);
                                FileClose(fnbr1);
                            }
                            if(c == ESC || c == CTRLKEY('Q') || c == F1 || fname) cmd_end();
                            // this must be save, exit and run.  We have done the first two, now do the run part.
                            ClearRuntime();
//                            WatchdogSet = false;
                            PrepareProgram(true);
                            // Create a global constant MM.CMDLINE$ containing the empty string.
                            (void) findvar((unsigned char *)"MM.CMDLINE$", V_FIND | V_DIM_VAR | T_CONST);
                            if(Option.LIBRARY_FLASH_SIZE == MAX_PROG_SIZE) ExecuteProgram(LibMemory );       // run anything that might be in the library
                            if(*ProgMemory != T_NEWLINE) return;                             // no program to run
                        #ifdef PICOMITEWEB
                            cleanserver();
                        #endif
                            nextstmt = ProgMemory;
                            return;

              // Search
              case CTRLKEY('R'):
              case F3:    GetInputString((unsigned char *)"Find (Use SHIFT-F3 to repeat): ");
                          if(*inpbuf == 0 || *inpbuf == ESC) break;
                          if(!(*inpbuf == 0xb3 || *inpbuf == F3)) strcpy((char *)tknbuf, (char *)inpbuf);
                          // fall through

              case CTRLKEY('G'):
              case 0xB3:  // SHIFT-F3
                          p = txtp;
                          if(*p == 0) p = EdBuff - 1;
                          i = strlen((char *)tknbuf);
                          while(1) {
                              p++;
                              if(p == txtp) break;
                              if(*p == 0) p = EdBuff;
                              if(p == txtp) break;
                              if(mem_equal(p, tknbuf, i)) break;
                          }
                          if(p == txtp) {
                              editDisplayMsg((unsigned char *)" NOT FOUND ");
                              break;
                          }
                          for(y = 0, txtp = EdBuff; txtp != p; txtp++) {          // find the line and column of the string
                              if(*txtp == '\n') {
                                  y++;                                            // y is the line
                              }
                          }
                          edy = y - VHeight/2;                                    // edy is the line displayed at the top
                          if(edy < 0) edy = 0;                                    // compensate if we are near the start
                          printScreen();
                          PositionCursor(txtp);
                          // SCursor(x, y);
                          break;

              // Mark
              case CTRLKEY('T'):
              case F4:    MarkMode(clipboard, &buf[1]);
                          printScreen();
                          PrintFunctKeys(EDIT);
                          PositionCursor(txtp);
                          break;

              case CTRLKEY('Y'):
              case CTRLKEY('V'):
              case F5:    if(*clipboard == 0) {
                              editDisplayMsg((unsigned char *)" CLIPBOARD IS EMPTY ");
                              break;
                          }
                          for(i = 0; clipboard[i]; i++) buf[i + 1] = clipboard[i];
                          buf[i + 1] = 0;
                          break;

              // F6 to F12 - Normal function keys
              case F6:
              case F7:
              case F8:
              case F9:
              case F10:
              case F11:
              case F12:
                          break;

              // a normal character
              default:    c = buf[0];
                          if(c < ' ' || c > '~') break;                           // make sure that this is valid
                          if(curx >= VWidth) {
                              editDisplayMsg((unsigned char *)" LINE IS TOO LONG ");
                              break;
                          }
                          TextChanged = true;
                          if(insert || *txtp == '\n' || *txtp == 0) {
                              if(!editInsertChar(c, &multi)) break;                       // insert it
                          } else
                              *txtp++ = c;                                        // or just overtype
                                printLine(edy + cury);                            // redraw the whole line so that colour coding will occur
                          PositionCursor(txtp);
                          // SCursor(x, cury);
                          tempx = cury;                                           // used to track the preferred cursor position
                          if(multi && Option.ColourCode)printScreen();
                          break;

            }
            lastkey = buf[0];
            if(buf[0] != UP && buf[0] != DOWN && buf[0] != CTRLKEY('E') && buf[0] != CTRLKEY('X')) tempx = curx;
            buf[MAXCLIP + 1] = 0;
            for(i = 0; i < MAXCLIP + 1; i++) buf[i] = buf[i + 1];                // suffle down the buffer to get the next char
        } while(*buf);
    }
}


/*******************************************************************************************************************
  UTILITY FUNCTIONS USED BY THE FULL SCREEN EDITOR
*******************************************************************************************************************/


void PositionCursor(unsigned char *curp) {
    int ln, col;
    unsigned char *p;

    for(p = EdBuff, ln = col = 0; p < curp; p++) {
       if(*p == '\n') {
            ln++;
            col = 0;
       } else
          col++;
    }
    if(ln < edy || ln >= edy + VHeight) return;
    SCursor(col, ln - edy);
}



// mark mode
// implement the mark mode (when the user presses F4)
void MarkMode(unsigned char *cb, unsigned char *buf) {
    unsigned char *p, *mark, *oldmark;
    int c, x, y, i, oldx, oldy, txtpx, txtpy, errmsg = false;

    PrintFunctKeys(MARK);
    oldmark = mark = txtp;
    txtpx = oldx = curx; txtpy = oldy = cury;
    while(1) {
        c = MMInkey();
        if(c != -1 && errmsg) {
            PrintFunctKeys(MARK);
            errmsg = false;
        }
        switch(c) {
            case ESC: uSec(50000);                                           // wait 50ms to see if anything more is coming
                      if(MMInkey() == '[' && MMInkey() == 'M') {
                          // received escape code for Tera Term reporting a mouse click.  in mark mode we ignore it
                          MMInkey();  MMInkey();  MMInkey();
                          break;
                      }
                      curx = txtpx; cury = txtpy;                             // just an escape key
                      return;

            case CTRLKEY('E'):
            case UP:  if(cury <= 0) continue;
                      p = mark;
                      if(*p == '\n') p--;                                     // step back over the terminator if we are right at the end of the line
                      while(p != EdBuff && *p != '\n') p--;                   // move to the beginning of the line
                      if(p != EdBuff) {
                          p--;                                                // step over the terminator to the end of the previous line
                          for(i = 0; p != EdBuff && *p != '\n'; p--, i++);    // move to the beginning of that line
                          if(*p == '\n') p++;                                 // and position at the start
                          if(i >= VWidth) {
                              editDisplayMsg((unsigned char *)" LINE IS TOO LONG ");
                              errmsg = true;
                              continue;
                          }
                      }
                      mark = p;
                      for(i = 0; i < edx + curx && *mark != 0 && *mark != '\n'; i++, mark++);  // move the cursor to the column
                      curx = i; cury--;
                      break;

            case CTRLKEY('X'):
            case DOWN:if(cury == VHeight -1) continue;
                      for(p = mark, i = curx; *p != 0 && *p != '\n'; p++, i++);// move to the end of this line
                      if(*p == 0) continue;                                    // skip if it is at the end of the file
                      if(i >= VWidth) {
                          editDisplayMsg((unsigned char *)" LINE IS TOO LONG ");
                          errmsg = true;
                          continue;
                      }
                      mark = p + 1;                                 // step over the line terminator to the start of the next line
                      for(i = 0; i < edx + curx && *mark != 0 && *mark != '\n'; i++, mark++);  // move the cursor to the column
                      curx = i; cury++;
                      break;

            case CTRLKEY('S'):
            case LEFT:if(curx == edx) continue;
                      mark--;
                      curx--;
                      break;

            case CTRLKEY('D'):
            case RIGHT:if(curx >= VWidth || *mark == 0 || *mark == '\n') continue;
                      mark++;
                      curx++;
                      break;

            case CTRLKEY('U'):
            case HOME:if(mark == EdBuff) break;
                      if(*mark == '\n') mark--;                     // step back over the terminator if we are right at the end of the line
                      while(mark != EdBuff && *mark != '\n') mark--;// move to the beginning of the line
                      if(*mark == '\n') mark++;                     // skip if no more lines above this one
                      break;

            case CTRLKEY('K'):
            case END: if(*mark == 0) break;
                      for(p = mark, i = curx; *p != 0 && *p != '\n'; p++, i++);// move to the end of this line
                      if(i >= VWidth) {
                          editDisplayMsg((unsigned char *)" LINE IS TOO LONG ");
                          errmsg = true;
                          continue;
                      }
                      mark = p;
                      break;

            case CTRLKEY('Y'):
            case CTRLKEY('T'):
            case F5:
            case F4:  if(txtp - mark > MAXCLIP || mark - txtp > MAXCLIP) {
                          editDisplayMsg((unsigned char *)" MARKED TEXT EXCEEDS CLIPBOARD BUFFER SIZE");
                          errmsg = true;
                          break;
                      }
                      if(mark <= txtp) {
                          p = mark;
                          while(p < txtp) *cb++ = *p++;
                      } else {
                          p = txtp;
                          while(p <= mark - 1) *cb++ = *p++;
                      }
                      *cb = 0;
                      if(c == F5 || c == CTRLKEY('Y')) {
                          PositionCursor(txtp);
                          return;
                      }
                      // fall through

            case CTRLKEY(']'):
            case DEL: if(mark < txtp) {
                          p = txtp;  txtp = mark; mark = p;         // swap txtp and mark
                      }
                      for(p = txtp; p < mark; p++) if(*p == '\n') nbrlines--;
                      for(p = txtp; *mark; ) *p++ = *mark++;
                      *p++ = 0; *p++ = 0;
                      TextChanged = true;
                      PositionCursor(txtp);
                      return;

            default:    continue;
        }

        x = curx; y = cury;
        markmode=true;
        // first unmark the area not marked as a result of the keystroke
        if(oldmark < mark) {
            PositionCursor(oldmark);
            p = oldmark;
            while(p < mark) {
                if(*p == '\n') { MMputchar('\r',0); MX470PutC('\r'); }// also print on the MX470 display
                MX470PutC(*p);                                      // print on the MX470 display
                MMputchar(*p++,0);
            }
        } else if(oldmark > mark) {
            PositionCursor(mark);
            p = mark;
            while(oldmark > p) {
                if(*p == '\n') { MMputchar('\r',0); MX470PutC('\r'); }// also print on the MX470 display
                MX470PutC(*p);                                      // print on the MX470 display
                MMputchar(*p++,0);
            }
        }
        fflush(stdout);

        oldmark = mark; oldx = x; oldy = y;

        // now draw the marked area
        if(mark < txtp) {
            PositionCursor(mark);
            MMPrintString("\033[7m");
            MX470Display(REVERSE_VIDEO);                            // reverse video on the MX470 display only
            p = mark;
            while(p < txtp) {
                if(*p == '\n') {
                    MMputchar('\r',0); MX470PutC('\r');               // also print on the MX470 display
                }
                MX470PutC(*p);                                      // print on the MX470 display
                MMputchar(*p++,0);
            }
            MX470Display(REVERSE_VIDEO);                            // reverse video back to normal on the MX470 display only
        } else if(mark > txtp) {
            PositionCursor(txtp);
            MMPrintString("\033[7m");
            MX470Display(REVERSE_VIDEO);                            // reverse video on the MX470 display only
            p = txtp;
            while(p < mark) {
                if(*p == '\n') {
                    MMputchar('\r',0); MX470PutC('\r');               // also print on the MX470 display
                }
                MX470PutC(*p);                                      // print on the MX470 display
                MMputchar(*p++,0);
            }
            MX470Display(REVERSE_VIDEO);                            // reverse video back to normal on the MX470 display only
        }
        markmode=false;
        MMPrintString("\033[0m");                                   // normal video

        oldx = x; oldy = y; oldmark = mark;
        PositionCursor(mark);
    }
}




// search through the text in the editing buffer looking for a specific line
// enters with ln = the line required
// exits pointing to the start of the line or pointing to a zero char if not that many lines in the buffer
char *findLine(int ln, int *inmulti) {
    unsigned char *p, *q;
    *inmulti=false;
    p = q = EdBuff;
    skipspace(q);
    if(q[0]=='/' && q[1]=='*') *inmulti=true;
    if(q[0]=='*' && q[1]=='/') *inmulti=false;
    while(ln && *p) {
        if(*p == '\n') { 
            if(*inmulti==2)*inmulti=false;
            ln--;
            q=&p[1];
            skipspace(q);
            if(q[0]=='/' && q[1]=='*') *inmulti=true;
            if(q[0]=='*' && q[1]=='/') *inmulti=2;
        }
        p++;
    }
    return (char *)p;
}


int EditCompStr(char *p, char *tkn) {
    while(*tkn && (toupper(*tkn) == toupper(*p))) {
        if(*tkn == '(' && *p == '(') return true;
        if(*tkn == '$' && *p == '$') return true;
        tkn++; p++;
    }
    if(*tkn == 0 && !isnamechar(*p)) return true;                   // return the string if successful

    return false;                                                   // or NULL if not
}


// this function does the syntax colour coding
// p = pointer to the current character to be printed
//     or NULL if the colour coding is to be reset to normal
//
// it keeps track of where it is in the line using static variables
// so it must be fed all chars from the start of the line
void SetColour(unsigned char *p, int DoVT100) {
    int i;
    unsigned char **pp;
    static int intext = false;
    static int incomment = false;
    static int multilinecomment = false;
    static int inkeyword = false;
    static unsigned char *twokeyword = NULL;
    static int inquote = false;
    static int innumber = false;

    if(!Option.ColourCode) return;

    // this is a list of keywords that can come after the OPTION and GUI commands
    // the list must be terminated with a NULL
    char *twokeywordtbl[] = {
        "BASE", "EXPLICIT", "DEFAULT", "BREAK", "AUTORUN", "BAUDRATE", "DISPLAY",
#if defined(PICOMITE)
        "BUTTON", "SWITCH", "CHECKBOX", "RADIO", "LED", "FRAME", "NUMBERBOX", "SPINBOX", "TEXTBOX", "DISPLAYBOX", "CAPTION", "DELETE",
        "DISABLE", "HIDE", "ENABLE", "SHOW", "FCOLOUR", "BCOLOUR", "REDRAW", "BEEP", "INTERRUPT",
#endif
        NULL
    };

    // this is a list of common keywords that should be highlighted as such
    // the list must be terminated with a NULL
    char *specialkeywords[] = {
        "SELECT", "INTEGER", "FLOAT", "STRING", "DISPLAY", "SDCARD", "OUTPUT", "APPEND", "WRITE", "SLAVE",
        NULL
    };


    // reset everything back to normal
    if(p == NULL) {
        innumber = inquote = inkeyword = incomment = intext = false;
        twokeyword = NULL;
        if(!multilinecomment){
            gui_fcolour = GUI_C_NORMAL;
            if(DoVT100) MMPrintString(VT100_C_NORMAL);
        }
        return;
    }
    
    if(*p == '*' && p[1]=='/' && !inquote) {
        multilinecomment = 2;
        return;
    }

    if(*p == '/' && !inquote && multilinecomment==2) {
        multilinecomment = false;
        return;
    }

    // check for a comment char
    if(*p == '\'' && !inquote) {
        gui_fcolour = GUI_C_COMMENT;
        if(DoVT100) MMPrintString(VT100_C_COMMENT);
        incomment = true;
        return;
    }
    if(*p == '/' && p[1]=='*' && !inquote) {
        gui_fcolour = GUI_C_COMMENT;
        if(DoVT100) MMPrintString(VT100_C_COMMENT);
        multilinecomment = true;
        return;
    }

    // once in a comment all following chars must be comments also
    if(incomment || multilinecomment) return;

    // check for a quoted string
    if(*p == '\"') {
        if(!inquote) {
            inquote = true;
            gui_fcolour = GUI_C_QUOTE;
            if(DoVT100) MMPrintString(VT100_C_QUOTE);
            return;
        } else {
            inquote = false;
            return;
        }
    }

    if(inquote) return;

    // if we are displaying a keyword check that it is still actually in the keyword and reset if not
    if(inkeyword) {
        if(isnamechar(*p) || *p == '$') return;
        gui_fcolour = GUI_C_NORMAL;
        if(DoVT100) MMPrintString(VT100_C_NORMAL);
        inkeyword = false;
        return;
    }

    // if we are displaying a number check that we are still actually in it and reset if not
    // this is complicated because numbers can be in hex or scientific notation
    if(innumber) {
        if(!isdigit(*p) && !(toupper(*p) >= 'A' && toupper(*p) <= 'F') && toupper(*p) != 'O' && toupper(*p) != 'H' && *p != '.') {
            gui_fcolour = GUI_C_NORMAL;
            if(DoVT100) MMPrintString(VT100_C_NORMAL);
            innumber = false;
            return;
        } else {
            return;
        }
    // check if we are starting a number
    } else if(!intext){
        if(isdigit(*p) || *p == '&' || ((*p == '-' || *p == '+' || *p == '.') && isdigit(p[1]))) {
            gui_fcolour = GUI_C_NUMBER;
            if(DoVT100) MMPrintString(VT100_C_NUMBER);
            innumber = true;
            return;
        }
        // check if this is an 8 digit hex number as used in CFunctions
        for(i = 0; i < 8; i++) if(!isxdigit(p[i])) break;
        if(i == 8 && (p[8] == ' ' || p[8] == '\'' || p[8] == 0)) {
            gui_fcolour = GUI_C_NUMBER;
            if(DoVT100) MMPrintString(VT100_C_NUMBER);
            innumber = true;
            return;
        }
    }

    // check if this is the start of a keyword
    if(isnamestart(*p) && !intext) {
        for(i = 0; i < CommandTableSize - 1; i++) {                 // check the command table for a match
            if(EditCompStr((char *)p, (char *)commandtbl[i].name) != 0) {
                if(EditCompStr((char *)p, "REM") != 0) {                 // special case, REM is a comment
                    gui_fcolour = GUI_C_COMMENT;
                    if(DoVT100) MMPrintString(VT100_C_COMMENT);
                    incomment = true;
                } else {
                    gui_fcolour = GUI_C_KEYWORD;
                    if(DoVT100) MMPrintString(VT100_C_KEYWORD);
                    inkeyword = true;
                    if(EditCompStr((char *)p, "GUI") || EditCompStr((char *)p, "OPTION")) {
                        twokeyword = p;
                        while(isalnum(*twokeyword)) twokeyword++;
                        while(*twokeyword == ' ') twokeyword++;
                    }
                return;
                }
            }
        }
        for(i = 0; i < TokenTableSize - 1; i++) {                   // check the token table for a match
            if(EditCompStr((char *)p, (char *)tokentbl[i].name) != 0) {
                gui_fcolour = GUI_C_KEYWORD;
                if(DoVT100) MMPrintString(VT100_C_KEYWORD);
                inkeyword = true;
                return;
            }
        }

        // check for the second keyword in two keyword commands
        if(p == twokeyword) {
            for(pp = (unsigned char **)twokeywordtbl; *pp; pp++)
                if(EditCompStr((char *)p, (char *)*pp)) break;
            if(*pp) {
                gui_fcolour = GUI_C_KEYWORD;
                if(DoVT100) MMPrintString(VT100_C_KEYWORD);
                inkeyword = true;
                return;
            }
        }
        if(p >= twokeyword) twokeyword = NULL;

        // check for a range of common keywords
        for(pp = (unsigned char **)specialkeywords; *pp; pp++)
            if(EditCompStr((char *)p, (char *)*pp)) break;
        if(*pp) {
            gui_fcolour = GUI_C_KEYWORD;
            if(DoVT100) MMPrintString(VT100_C_KEYWORD);
            inkeyword = true;
            return;
        }
    }

    // try to keep track of if we are in general text or not
    // this is to avoid recognising keywords or numbers inside variables
    if(isnamechar(*p)) {
        intext = true;
    } else {
        intext = false;
        gui_fcolour = GUI_C_NORMAL;
        if(DoVT100) MMPrintString(VT100_C_NORMAL);
    }

}


// print a line starting at the current column (edx) at the current cursor.
// if the line is beyond the end of the text then just clear to the end of line
// enters with the line number to be printed
void printLine(int ln) {
    char *p;
    int i;
    int inmulti=false;
    // we always colour code the output to the LCD panel on the MX470 (when used as the console)
    if(Option.DISPLAY_CONSOLE) {
        MX470PutC('\r');                                            // print on the MX470 display
        p = findLine(ln, &inmulti);
        i = VWidth - 1;
        while(i && *p && *p != '\n') {
            if(!inmulti)SetColour((unsigned char *)p, false);                                    // set the colour for the LCD display only
            else gui_fcolour = GUI_C_COMMENT;
            MX470PutC(*p++);                                        // print on the MX470 display
            i--;
        }
        MX470Display(CLEAR_TO_EOL);                                 // clear to the end of line on the MX470 display only
    }
    SetColour(NULL, false);

    p = findLine(ln, &inmulti);
    if(Option.ColourCode) {
        // if we are colour coding we need to redraw the whole line
        MMputchar('\r',0);                                            // display the chars after the editing point
        i = VWidth - 1;
    } else {
        // if we are NOT colour coding we can start drawing at the current cursor position
        i = curx;
        while(i-- && *p && *p != '\n') p++;                         // find the editing point in the buffer
        i = VWidth - curx;
    }

    while(i && *p && *p != '\n') {
        if(Option.ColourCode) {
            if(!inmulti)SetColour((unsigned char *)p, true);                   // if colour coding is used set the colour for the VT100 emulator
            else {
                gui_fcolour = GUI_C_COMMENT;
                MMPrintString(VT100_C_COMMENT);
            }
        }
        MMputchar(*p++,0);                                            // display the chars after the editing point
        i--;
    }

    MMPrintString("\033[K");                                        // all done, clear to the end of the line on a vt100 emulator
    if(Option.ColourCode) SetColour(NULL, true);
    curx = VWidth - 1;
}



// print a full screen starting with the top left corner specified by edx, edy
// this draws the full screen including blank areas so there is no need to clear the screen first
// it then returns the cursor to its original position
void printScreen(void) {
    int i;

    SCursor(0, 0);
    for(i = 0; i <VHeight; i++) {
        printLine(i + edy);
        MMPrintString("\r\n");
        MX470PutS("\r\n", gui_fcolour, gui_bcolour);
        curx = 0;
        cury = i + 1;
    }
    while(MMInkey() != -1);                                           // consume any keystrokes accumulated while redrawing the screen
}



// position the cursor on the screen
void SCursor(int x, int y) {
    char s[12];

    MMPrintString("\033[");
    IntToStr(s, y + 1, 10); MMPrintString(s); MMPrintString(";");
    IntToStr(s, x + 1, 10); MMPrintString(s); MMPrintString("H");
    MX470Cursor(x * gui_font_width, y * gui_font_height);           // position the cursor on the MX470 display only
    curx = x; cury = y;
}



// move the text down by one char starting at the current position in the text
// and insert a character
int editInsertChar(unsigned char c, char *multi) {
    unsigned char *p;

    for(p = EdBuff; *p; p++);                                         // find the end of the text in memory
    if(p >= EdBuff + EDIT_BUFFER_SIZE - 10) {                         // and check that we have the space (allow 10 bytes for slack)
        editDisplayMsg((unsigned char *)" OUT OF MEMORY ");
        return false;
    }
    for(; p >= txtp; p--) *(p + 1) = *p;                              // shift everything down
    *multi=0;
    p=txtp-1;
    if((c=='/' && *p=='*') || (c=='*' && *p=='/') )*multi=1;
    p+=2;
    if((c=='/' && *p=='*') || (c=='*' && *p=='/') )*multi=1;
    *txtp++ = c;                                                      // and insert our char
    return true;
}



// print the function keys at the bottom of the screen
void PrintFunctKeys(int typ) {
    int i, x, y;
    char *p;

    if(typ == EDIT) {
        if(VWidth >= 78)
            p = "ESC:Exit  F1:Save  F2:Run  F3:Find  F4:Mark  F5:Paste";
        else if(VWidth >= 62)
            p = "F1:Save F2:Run F3:Find F4:Mark F5:Paste";
        else
            p = "EDIT MODE";
    } else {
        if(VWidth >= 49)
            p = "MARK MODE   ESC=Exit  DEL:Delete  F4:Cut  F5:Copy";
        else
            p = "MARK MODE";
    }

    MX470Display(DRAW_LINE);                                        // on the MX470 display draw the line
    MX470PutS(p, GUI_C_STATUS, gui_bcolour);                        // display the string on the display attached to the MX470
    MX470Display(CLEAR_TO_EOL);                                     // clear to the end of line on the MX470 display only

    x = curx; y = cury;
    SCursor(0, VHeight);
    if(Option.ColourCode) MMPrintString(VT100_C_LINE);
    MMPrintString("\033[4m");                                       // underline on
    for(i = 0; i < VWidth; i++) MMputchar(' ',0);
    MMPrintString("\033[0m\r\n");                                   // underline off
    if(Option.ColourCode) MMPrintString(VT100_C_STATUS);
    MMPrintString(p);
    if(Option.ColourCode) MMPrintString(VT100_C_NORMAL);
    MMPrintString("\033[K");                                        // clear to the end of the line on a vt100 emulator
    SCursor(x, y);
}



// print the current status
void PrintStatus(void) {
    int tx;
    char s[MAXSTRLEN];

    tx = edx + curx + 1;
    strcpy(s, "Ln: ");
    IntToStr(s + strlen(s), edy + cury + 1, 10);
    strcat(s + strlen(s), "  Col: ");
    IntToStr(s + strlen(s), tx, 10);
    strcat(s, "       ");
    strcpy(s + 19, insert?"INS":"OVR");

    MX470Cursor((VWidth - strlen(s)) * gui_font_width, VRes - gui_font_height);
    MX470PutS(s, GUI_C_STATUS, gui_bcolour);                        // display the string on the display attached to the MX470

    SCursor(VWidth - 25, VHeight + 1);
    if(Option.ColourCode) MMPrintString(VT100_C_STATUS);
    MMPrintString(s);
    if(Option.ColourCode) MMPrintString(VT100_C_NORMAL);

    PositionCursor(txtp);
}



// display a message in the status line
void editDisplayMsg(unsigned char *msg) {
    SCursor(0, VHeight + 1);
    if(Option.ColourCode) MMPrintString(VT100_C_ERROR);
    MMPrintString("\033[7m");
    MX470Cursor(0, VRes - gui_font_height);
    MMPrintString((char *)msg);
    MX470PutS((char *)msg, BLACK, RED);
    if(Option.ColourCode) MMPrintString(VT100_C_NORMAL);
    MMPrintString("\033[0m");
    MMPrintString("\033[K");                                        // clear to the end of the line on a vt100 emulator
    MX470Display(CLEAR_TO_EOL);                                     // clear to the end of line on the MX470 display only
    PositionCursor(txtp);
    drawstatusline = true;
}



// save the program in the editing buffer into the program memory
void SaveToProgMemory(void) {
    SaveProgramToFlash(EdBuff, true);
    ClearProgram();
    StartEditPoint = (unsigned char *)(edy + cury);                            // record out position in case the editor is invoked again
    StartEditChar = edx + curx;
    // bugfix for when the edit point is a space
    // the space could be at the end of a line which will be trimmed in SaveProgramToFlash() leaving StartEditChar referring to something not there
    // this is not a serious issue so fix the bug in the MX470 only because it has plenty of flash
    while(StartEditChar > 0 && txtp > EdBuff && *(--txtp) == ' ') {
        StartEditChar--;
    }
}



// get an input string from the user and save into inpbuf
void GetInputString(unsigned char *prompt) {
    int i;
    unsigned char *p;

    SCursor(0, VHeight + 1);
    MMPrintString((char *)prompt);
    MX470Cursor(0, VRes - gui_font_height);
    MX470PutS((char *)prompt, gui_fcolour, gui_bcolour);
    for(i = 0; i < VWidth - strlen((char *)prompt); i++) {
        MMputchar(' ',1);
        MX470PutC(' ');
    }
    SCursor(strlen((char *)prompt), VHeight + 1);
    MX470Cursor(strlen((char *)prompt) * gui_font_width, VRes - gui_font_height);
    for(p = inpbuf; (*p = MMgetchar()) != '\r'; p++) {              // get the input
        if(*p == 0xb3 || *p == F3 || *p == ESC) { p++; break; }     // return if it is SHIFT-F3, F3 or ESC
        if(isprint(*p)) {
            MMputchar(*p,1);                                          // echo the char
            MX470PutC(*p);                                          // echo the char on the MX470 display
        }
        if(*p == '\b') {
            p--;                                                    // backspace over a backspace
            if(p >= inpbuf){
                p--;                                                // and the char before
                MMPrintString("\b \b");                             // erase on the screen
                MX470PutS("\b \b", gui_fcolour, gui_bcolour);       // erase on the MX470 display
            }
        }
    }
    *p = 0;                                                         // terminate the input string
    PrintFunctKeys(EDIT);
    PositionCursor(txtp);
}


// scroll up the video screen
void Scroll(void) {
    edy++;
    SCursor(0, VHeight);
    MMPrintString("\033[J\033[99B\n");                              // clear to end of screen, move to the end of the screen and force a scroll of one line
    MX470Cursor(0, VHeight * gui_font_height);
    MX470Scroll(gui_font_height);
    SCursor(0, VHeight);
    curx = 0;
    cury = VHeight - 1;
    PrintFunctKeys(EDIT);
    printLine(VHeight - 1 + edy);
    PositionCursor(txtp);
    while(MMInkey() != -1);                                         // consume any keystrokes accumulated while redrawing the screen
}


// scroll down the video screen
void ScrollDown(void) {
    SCursor(0, VHeight);                                            // go to the end of the editing area
    MMPrintString("\033[J");                                        // clear to end of screen
    edy--;
    SCursor(0, 0);
    MMPrintString("\033M");                                         // scroll window down one line
    MX470Scroll(-gui_font_height);
    printLine(edy);
    PrintFunctKeys(EDIT);
    PositionCursor(txtp);
    while(MMInkey() != -1);                                         // consume any keystrokes accumulated while redrawing the screen
}

#endif
