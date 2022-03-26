#include <stdio.h>
#include <stdlib.h>
#include <curses.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <sys/types.h>

/* program version */
float version = 0.1;

/* default snap length (maximum bytes per packet to capture) */
#define SNAP_LEN 1518

/* max char length for filter */
#define FILTER 96

/* Starting number of struct account pointers to hold before realloc() is called */
#define ACCOUNTS 2

/* Print Screen every number of seconds */
#define SECONDS 2

/* Definition for the ENTER key representing integer 10 */
#define ENTER 10

/* Macros for refreshing the pads */
#define PREFRESHP1HEAD prefresh(p1head, 0, 0, 0, 3, 0, columns);
#define PREFRESHP1INDEX prefresh(p1index, p1scrolltop, 0, 1, 0, part1rows, 1);
#define PREFRESHP1ACCOUNTS prefresh(p1accounts, p1scrolltop, 0, 1, 3, part1rows, columns);
#define PREFRESHP2HEAD prefresh(p2head, 0, 0, 0, 0, 0, columns);
#define PREFRESHP2IPS prefresh(p2ips, 0, 0, 2, 0, part2rows, 30);
#define PREFRESHP2REQUESTS prefresh(p2requests, 0, 0, 2, 32, part2rows, columns);

/* Declaring Ncurses Pads */
WINDOW *p1head, *p1index, *p1accounts, *p2head, *p2ips, *p2requests;

/* Declaring Ncurses Backup Pads to save state while paused */
WINDOW *p1accounts_backup, *p1index_backup, *p2ips_backup, *p2requests_backup;

/* Defining Ncurses rows and columns */
int rows, columns, part1rows, part2rows, totalrowsp1, totalrowsp2;

/* Defining Ncurses scrolling variables */
int p1scrollbottom, p1scrolltop, p2scrollbottom, p2scrolltop, selection, position, input;
#define PREFRESHP1ACCOUNTSSCROLL prefresh(p1accounts, p1scrolltop, 0, 1, 3, part1rows, columns);
#define PREFRESHP1INDEXSCROLL prefresh(p1index, p1scrolltop, 0, 1, 0, part1rows, 1);

/* Account string and boolean int for switching between part1 and part2 */
struct Account *part2account;
int usePart2;
int Pause;
int Shutdown;
int realtime;
int useFilter;
int Exit;

/* filter buffer */
char Filter[FILTER];

/*
 * main struct that is at the top of all the data structures
 */
struct Accounts {
  struct Account **dptr;  // array of pointers pointing to struct Account
  char accountNumber[13]; // account number
  int seconds;            // rate to print to screen
  u_int count;            // number of accounts
  u_int size;             // size of account pointer array
};

typedef struct Accounts Accounts;

/*
 * struct to store data on account
 */
struct Account {
    u_int GET, POST;
    u_int num_requests;     // number of distinct requests
    u_int total_requests;
    u_int num_ips;
    char name[64];
    u_int request_size;
    struct Request **requests;  // array of pointers pointing to struct Request
    u_int ip_size;
    struct IP **ips;      // array of pointers pointing to struct IP
};

typedef struct Account Account;

/*
 * struct to store requests about a account
 */
struct Request {
    u_int count;
    char url[128];
};

typedef struct Request Request;

/*
 * struct to store IPs
 */
struct IP {
  u_int count;
  char ip[16];
};

typedef struct IP IP;

/*
 * Initializes main pointer to data structures
 */
Accounts *Initialize()
{
  // allocate memory for Dptr
  Accounts *Dptr = calloc(1, sizeof(Accounts));
  Dptr->count = 0;
  Dptr->size = 0;
  Dptr->seconds = SECONDS;

  // allocate memory for array of pointers to struct account
  Dptr->dptr = (Account **) calloc(ACCOUNTS, sizeof(Account *));

  Dptr->size = ACCOUNTS;

  return Dptr;
}

/*
 * function to create the Ncurses pads
 */
void CreatePads()
{
  /* initialize pads
   * p1head = Header
   * p1index = User Input
   * p1accounts = Full summary of accounts and their requests
   * p2head = Header
   * p2ips = accounts specific stats IP
   * p2requests = account specific stats URLs
   */
  p1head = newpad(1, columns - 3);
  p1index = newpad(rows - 1, 2); // hold indexes
  p1accounts = newpad(rows - 1, columns - 3); // hold Summary of Accounts
  p2head = newpad(1, columns);
  p2ips = newpad(rows - 2, 31);
  p2requests = newpad(rows - 2, columns - 32);
  if (totalrowsp1 == 0)
    totalrowsp1 = rows;

  // set dynamic rows variable for scrolling and to resize rows in Part1Resize() and Part2Resize()
  part1rows = rows - 1;
  p1scrollbottom = part1rows;
  part2rows = rows - 1;
  if (totalrowsp2 == 0)
    totalrowsp2 = part2rows;

  return;
}

/*
 * function to display intro screen
 */
void DisplayIntro(Accounts *Dptr)
{
  clear();
  move(0,0);
  printw("rows = %d\ncolumns = %d\n", rows, columns);
  refresh();
  sleep(1);
  wmove(p1index, 0, 0);
  waddstr(p1index, "->");
}

/*
 * function to start up Ncurses
 */
int NcursesInit(Accounts *Dptr)
{

  // Start up Ncurses
  initscr();

  // turn off cursor
  curs_set(0);

  // enable colors
  if (has_colors() == TRUE)
    start_color();

  // create black and white pair
  init_pair(1, COLOR_BLACK, COLOR_WHITE);
  init_pair(2, COLOR_WHITE, COLOR_BLACK);

  // get number of rows and columns for current session
  getmaxyx(stdscr, rows, columns);

  totalrowsp1 = 0;
  totalrowsp2 = 0;

  CreatePads();
  DisplayIntro(Dptr);
}

/*
 * function to erase all data from the screen
 */
void EraseAll()
{
  werase(p1index);
  werase(p1accounts);
  werase(p2ips);
  werase(p2requests);
  clear();
}

/*
 * function to refresh all pads
 */
void RefreshAll()
{
  PREFRESHP1ACCOUNTS;
  PREFRESHP1INDEX;
  PREFRESHP2IPS;
  PREFRESHP2REQUESTS;
  refresh();
}

/*
 * function to refresh pads in Part1
 */
void Part1Refresh()
{
  PREFRESHP1INDEX;
  PREFRESHP1ACCOUNTS;
  clear();
  refresh();
}

/*
 * function to refresh pads in Part2
 */
void Part2Refresh()
{
  PREFRESHP2HEAD;
  PREFRESHP2IPS;
  PREFRESHP2REQUESTS;
  clear();
  refresh();
}

/*
 * function to resize Part1
 */
void Part1Resize()
{
/*
  if (p1head) delwin(p1head);
  if (p1index) delwin(p1index);
  if (p1accounts) delwin(p1accounts);
  p1head = newpad(1, columns - 3);
  p1index = newpad(totalrowsp1 * 2, 2);
  p1accounts = newpad(totalrowsp1 * 2, columns - 3);
*/
  wresize(p1index, totalrowsp1 * 2, 2);
  wresize(p1accounts, totalrowsp1 * 2, columns - 3);

  totalrowsp1 *= 2;
  PREFRESHP1HEAD;
  PREFRESHP1ACCOUNTS;
  PREFRESHP1INDEX;
}

/*
 * function to resize Part2
 */
void Part2Resize()
{
/*
  if (p2head) delwin(p2head);
  if (p2ips) delwin(p2ips);
  if (p2requests) delwin(p2requests);
  p2head = newpad(1, columns);
  p2ips = newpad((totalrowsp2 * 2) - 2, 31);
  p2requests = newpad((totalrowsp2 * 2) - 2, columns - 32);
*/
  wresize(p2ips, ((totalrowsp2 * 2) - 2), 31);
  wresize(p2requests, ((totalrowsp2 * 2) - 2), columns - 32);

  totalrowsp2 *= 2;
  PREFRESHP2HEAD;
  PREFRESHP2IPS;
  PREFRESHP2REQUESTS;;
}

/*
 * function to shut down Ncurses
 */
int NcursesExit()
{

  // Cleanup Ncurses
  delwin(p1head);
  delwin(p1index);
  delwin(p1accounts);
  delwin(p2head);
  delwin(p2ips);
  delwin(p2requests);
  endwin();
}

/*
 * function to filter out URLs
 */
int filterURL(char *url)
{
  if (strstr(url, Filter)) return 1;
  else return 0;
}

/*
 * Ncurses Part1 - Summary of Accounts
 */
void NcursesPart1(Accounts *Dptr)
{
  int i;

  // clear up screen
  werase(p2head);
  werase(p2ips);
  werase(p2requests);
  Part2Refresh(Dptr);

  // print header
  wmove(p1head, 0, 0);

  if (Pause) waddstr(p1head, "Total\t\tGET\t\tPOST\t\tAccount\t\t*Paused*\n");
  else waddstr(p1head, "Total\t\tGET\t\tPOST\t\tAccount\n");

  PREFRESHP1HEAD;

  // move to the top left corner and output Account Summary Statistics (Part 1)
  wmove(p1accounts, 0, 0);
  for (i = 0; i < Dptr->count; i++)
    wprintw(p1accounts, "%d\t\t%d\t\t%d\t\t%s\n",  Dptr->dptr[i]->total_requests,
                            Dptr->dptr[i]->GET,
                            Dptr->dptr[i]->POST,
                            Dptr->dptr[i]->name);

  PREFRESHP1ACCOUNTS;

  // refresh index arrow
  PREFRESHP1INDEX;
}

/*
 * Ncurses Part2 - Summary of Account
 */
void NcursesPart2(Account *dptr)
{
  int i;

  // clear up screen
  werase(p1head);
  werase(p1index);
  werase(p1accounts);
  Part1Refresh();

  // move to the top left of p2requests pad
  wmove(p2head, 0, 0);
  if (Pause)
    wprintw(p2head, "IPs:count  *Paused*\t%s\t\tGET: %d\t\tPOST: %d\t\tTotal Requests: %d\n",
                      dptr->name,
                      dptr->GET,
                      dptr->POST,
                      dptr->total_requests);
  else
    wprintw(p2head, "IPs:count\t\t%s\t\tGET: %d\t\tPOST: %d\t\tTotal Requests: %d\n",
    //wprintw(p2head, "IPs:count %s GET: %d POST: %d Total Requests: %d part2rows %d totalrowsp2: %d num_ips: %d num_requests %d\n",
                      dptr->name,
                      dptr->GET,
                      dptr->POST,
                      dptr->total_requests);

  PREFRESHP2HEAD;

  // move to top left of p2ips pad
  wmove(p2ips, 0, 0);

  for (i = 0; i < dptr->num_ips; i++)
    wprintw(p2ips, "%s: %d\n",  dptr->ips[i]->ip,
                  dptr->ips[i]->count);

  PREFRESHP2IPS;

  // output URLs
  wmove(p2requests, 0, 0);
  for (i = 0; i < dptr->num_requests; i++) {
    if (useFilter == 1) {
      if (filterURL(dptr->requests[i]->url) == 1)
        wprintw(p2requests, "count: %d\t%s\n",  dptr->requests[i]->count,
                            dptr->requests[i]->url);
    }
    else
      wprintw(p2requests, "count: %d\t%s\n",  dptr->requests[i]->count,
                          dptr->requests[i]->url);
  }

  PREFRESHP2REQUESTS;
}

/*
 * function to have thread run for user input
 */
void UserInput(Accounts *Dptr)
{
  int status = 0;

  selection = 0;
  position = 0;
  input = 0;
  useFilter = 0;

  int ret1 = 0;

  // set scrolling variables
  p1scrolltop = 0;
  p2scrolltop = 0;

  // turn off cursor
  curs_set(0);

  PREFRESHP1INDEX;

  do {
    input = getchar();

    switch(input) {
      case 'e': // switch to part 2 for account
        if (usePart2 == 0) {
          part2account = Dptr->dptr[selection];
          usePart2 = 1;
        }
        break;
      case 'f': // add filter
        if (usePart2 == 0) break;
        if (useFilter == 0) {
          Pause = 1;
          useFilter = 1;
          werase(p2head);
          PREFRESHP2HEAD;
          wmove(p2head, 0, 0);
          wprintw(p2head, "Enter filter: ");
          wmove(p2head, 0, 15);
          prefresh(p2head, 0, 0, 0, 0, 0, columns);
          mvgetnstr(0, 14, Filter, 80);
          werase(p2head);
          werase(p2ips);
          werase(p2requests);
          Part2Refresh(Dptr);
          Pause = 0;
        }
        else {
          useFilter = 0;
          clear();
          move(0, 0);
          addstr("Removing filter");
          refresh();
          sleep(1);
        }
        break;
        case 'i': // switch to part 1
        if (usePart2 == 1) {
          usePart2 = 0;
          useFilter = 0;
          part2account = NULL;
          position = 0;
          selection = 0;
          p1scrolltop = 0;
          p1scrollbottom = part1rows;
          Part2Refresh(Dptr);
          //NcursesPart1(Dptr);
        }
        break;
      case 'j': // move down
        // if at the bottom of the screen
        // use Part1
        if ((selection < Dptr->count-1) && (usePart2 == 0)) {
          if (position == p1scrollbottom-1) {
            wmove(p1index, selection, 0);
            werase(p1index);
            p1scrolltop++;
            p1scrollbottom++;
            selection++;
            position++;
            wmove(p1index, selection, 0);
            waddstr(p1index, "->");
            PREFRESHP1ACCOUNTSSCROLL;
            PREFRESHP1INDEXSCROLL;
          }
          else {
            wmove(p1index, position, 0);
            werase(p1index);
            position++;
            selection++;
            wmove(p1index, position, 0);
            waddstr(p1index, "->");
            PREFRESHP1INDEX;
          }
        }
        break;
      case 'k': // move up
        // if at the top of the screen
        // usePart1
        if ((selection > 0) && (usePart2 == 0)) {
          if (position == p1scrolltop) {
            wmove(p1index, selection, 0);
            werase(p1index);
            p1scrolltop--;
            p1scrollbottom--;
            selection--;
            position--;
            wmove(p1index, selection, 0);
            waddstr(p1index, "->");
            PREFRESHP1ACCOUNTSSCROLL;
            PREFRESHP1INDEXSCROLL;
          }
          else {
            wmove(p1index, position, 0);
            werase(p1index);
            position--;
            selection--;
            wmove(p1index, position, 0);
            waddstr(p1index, "->");
            PREFRESHP1INDEX;
          }
        }

        break;
      case 'p': // pause/resume
        // Pausing
        if (Pause == 0) {
          Pause = 1;

          // if packet capture hasn't started
          if (Dptr->count == 0) {
            clear();
            move(0, 0);
            addstr("Paused");
            refresh();
          }
          // packet capture has already started
          else {
            if (usePart2)
              NcursesPart2(part2account);
            else
              NcursesPart1(Dptr);
          }
        }
        // Unpausing
        else {
          Pause = 0;
          // if packet capture hasn't started
          if (Dptr->count == 0)
            DisplayIntro(Dptr);
          // packet capture has already started
          else {
            if (usePart2)
              NcursesPart2(part2account);
            else
              NcursesPart1(Dptr);
          }
        }
        break;
      default:
        break;
    }
  } while (input != 'q');
    Shutdown = 1;
    werase(p1index);
    PREFRESHP1INDEX;
    sleep(1);
    pthread_exit(&status);
}

/*
 * function to have thread switch between parts
 */
void PartSwitcher(Accounts *Dptr)
{
  int status = 0;
  for(;;) {
    if (Shutdown) pthread_exit(&status);
    do {
      sleep(1);
    } while ((usePart2 == 0) && (Shutdown == 0));
      if (Shutdown) pthread_exit(&status);
      NcursesPart2(part2account);

    do {
      sleep(1);
    } while ((usePart2 == 1) && (Shutdown == 0));
      wmove(p1index, position, 0);
      waddstr(p1index, "->");
      PREFRESHP1INDEX;
      if (Dptr != NULL)
        NcursesPart1(Dptr);
  }
}

/*
 * function to print to the screen every 2 seconds
 */
void PrintScreen(Accounts *Dptr)
{
  int status = 0;

  for(;;) {
    if (Dptr != NULL) sleep(Dptr->seconds);
    if (Shutdown == 1) pthread_exit(&status);
    if (Pause == 0) {
      if (usePart2 == 0) {
        if (Dptr != NULL)
          NcursesPart1(Dptr);
      }
      else
        NcursesPart2(part2account);
    }
  }
}

/*
 * Frees up all the data structures
 */
void TearDown(Accounts *Dptr)
{
  int i, j, ips = 0, size = 0, count = 0;

  size = Dptr->size;
  count = Dptr->count;

  for (i = 0; i < Dptr->count; i++)
    for (j = 0; j < Dptr->dptr[i]->num_requests; j++)
      free(Dptr->dptr[i]->requests[j]);

  for (i = 0; i < Dptr->count; i++)
    for (j = 0; j < Dptr->dptr[i]->num_ips; j++)
      free(Dptr->dptr[i]->ips[j]);

  for (i = 0; i < count; i++)
    free(Dptr->dptr[i]);

  free(Dptr);
  Dptr = NULL;

  return;
}

/*
 * function to print command usage
 */
void PrintUsage(char **argv, Accounts *Dptr)
{
  fprintf(stderr, "%s version %.1f\n", argv[0], version);
  fprintf(stderr, "Usage: %s <account_number>\n\n", argv[0]);
  fprintf(stderr, "Runtime Commands\n");
  fprintf(stderr, "p - pause screen to highlight text for copying\n");
  fprintf(stderr, "q - quit program\n\n");

  fprintf(stderr, "Master mode\n");
  fprintf(stderr, "e - enter Account mode for selected account\n");
  fprintf(stderr, "j/k - move up and down account list\n\n");

  fprintf(stderr, "Account mode\n");
  fprintf(stderr, "f - add filter/release filter\n");
  fprintf(stderr, "i - back out to Master mode\n");
  free(Dptr);
  exit(1);
}

/*
 * function to parse through command line arguments
void ParseArguments(int *argc, char **argv, Accounts *Dptr)
{
  int opt = 0;

  while ((opt = getopt(*argc, argv, "hi:n:p:r")) != -1) {
    switch(opt) {
      case 'i':
          Dptr->interface = optarg;
          break;
      case 'h':
        PrintUsage(argv, Dptr);
        break;
      case 'n':
        if ((atoi(optarg) > 0) && (atoi(optarg) < 121)) {
          Dptr->seconds = atoi(optarg);
          break;
        }
        else
          PrintUsage(argv, Dptr);
      case 'p':
        if ((atoi(optarg) > 0) && (atoi(optarg) < 65536)) {
          strncpy(Dptr->port+5, optarg, 5);
          if (Dptr->port[10] != '\0');
            Dptr->port[10] = '\0';
        }
        else
          PrintUsage(argv, Dptr);
        break;
      case 'r':
        realtime = 1;
        break;
        case '?':  // if user does not use required argument with an option
          if (optopt == 'i') {
          PrintUsage(argv, Dptr);
          } else {
          PrintUsage(argv, Dptr);
          }
        if (optopt == 'n') {
          PrintUsage(argv, Dptr);
          } else {
          PrintUsage(argv, Dptr);
          }
        if (optopt == 'p') {
          PrintUsage(argv, Dptr);
          } else {
          PrintUsage(argv, Dptr);
          }
          break;
    }
  }
  return;
}
*/

int main(int argc, char *argv[]) {

  int i, j, k;
  
  int account_number = 0; 

  // thread variables
  pthread_t user_input, part_switcher, print_screen;

  // have Part1 ready to display
  usePart2 = 0;

  // have pause turned off
  Pause = 0;

  // set realtime to 0
  realtime = 0;

  // initialize mutex
  pthread_mutex_t mylock = PTHREAD_MUTEX_INITIALIZER;

  // main data structure parent
  Accounts *Dptr;

  // Initialize data structures
  Dptr = Initialize();

  // parse arg grab account name
  if (argc > 1)
    strncpy(Dptr->accountNumber, argv[1], 12);

  printf("Logging into %s\n", Dptr->accountNumber);

  // start up user input thread
  pthread_create (&user_input, NULL, (void *) &UserInput, (void *) Dptr);

  // start up part_switcher thread
  pthread_create (&part_switcher, NULL, (void *) &PartSwitcher, (void *) Dptr);

  // start the Print Screen thread
  if (realtime == 0)
    pthread_create (&print_screen, NULL, (void *) &PrintScreen, (void *) Dptr);

  // free up data structures
  TearDown(Dptr);

  return(0);
}
