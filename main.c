//
//  caws.c
//
//  Tristan Gonzalez, caws, a wrapper for awscli written in C
//  Copyright (c) 2022 Tristan Gonzalez. All rights reserved.
//  rgonzale@darkterminal.net
//
// credits to Tim Carstens for TCP/IP data structures from sniffer.c
// credits to Vito Ruiz for the sorting algorithm and the name "ddoc"
//
#include <stdio.h>
#include <stdlib.h>
#include <curses.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <sys/types.h>
#include <signal.h>

/* program version */
float version = 0.1;

/* default snap length (maximum bytes per packet to capture) */
#define SNAP_LEN 1518

/* max char length for filter */
#define FILTER 96

/* Starting number of struct account pointers to hold before realloc() is called */
#define ACCOUNTS 2

/* Print Screen every number of seconds */
#define SECONDS 1

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
  int seconds;            // rate to print to screen
  u_int count;            // number of accounts
  u_int size;             // size of account pointer array
};

typedef struct Accounts Accounts;

/*
 * struct to store data on account
 */
struct Account {
  char accountNumber[64]; // account number
  struct VPC **vpcs;      // array of pointers pointing to struct VPC
  struct EC2 **ec2s;      // array of pointers pointing to struct EC2
};

typedef struct Account Account;

/*
 * struct to store EC2
 */
struct EC2 {
    u_int count;
    char name[128];
    char instanceid[128];
    char privateip[128];
};

typedef struct EC2 EC2;

/*
 * struct to store VPCs
 */
struct VPC {
  u_int count;
  char name[16];
  char vpc[16];
  char cidr[32];
  struct SUBNET **subnets;
};

typedef struct VPC VPC;

/*
 * struct to store Subnets
 */
struct SUBNET {
  u_int count;
  char name[16];
  char vpc[16];
  char cidr[32];
};

typedef struct SUBNET SUBNET;


void TearDown(struct Accounts *);
int NcursesExit();

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

  //Dptr->size = ACCOUNTS;

  return Dptr;
}

/*
 * allocates and creates a account struct
 */
void AddAccount(Accounts *Dptr, char *accountNumber)
{
  int count = 0, size = 0;
  Account **tmp;
  count = Dptr->count;
  size = Dptr->size;

  // if the number of struct Accounts is the same as number of struct pointers in the array then realloc array by factor of 2
  if (count == size) {
    tmp = (Account **) realloc(Dptr->dptr, (sizeof(Account *) * size) * 2);
    if (tmp == NULL) {
        fprintf(stderr, "Realloc failed\n");
          exit(1);
    }
    else
      Dptr->dptr = tmp;

    tmp = NULL;
    size *= 2;
    Dptr->size = size;
  }

  // allocate memory for 1 struct account
  Account *account = calloc(1, sizeof(struct Account));

  Dptr->dptr[count] = account;
  strcpy(Dptr->dptr[0]->accountNumber, accountNumber);
  Dptr->count++;

  return;
}

/*
 * gets index of account
int GetAccountIndex(Accounts *Dptr, char *host)
{
  int i;

  for (i = 0; i < Dptr->count; i++) {
    if (strncmp(Dptr->dptr[i]->name, host, strlen(host)) == 0)
      return i;
  }
  // didn't find account
  return -1;
}
*/

/*
 * sort Account function, only compares the elements before it
int sortAccounts(Accounts *Dptr, int *account_index)
{
  int i, index = -1;
  struct Account *tmp;
  for (i = *account_index; i > 0; i--) {
    if (Dptr->dptr[i]->total_requests > Dptr->dptr[i - 1]->total_requests) {
      tmp = Dptr->dptr[i];
      Dptr->dptr[i] = Dptr->dptr[i - 1];
      Dptr->dptr[i - 1] = tmp;
      index = i - 1;
    }
  }
  if (index != -1)
    return index;
  else
    return *account_index;
}
*/

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
 * catch Cntrl-C
 */
void mysighand(Accounts *Dptr, int signum) {
      if (signum == 2) {
    move(0, 0);
    clear();
    refresh();
          addstr("Inga\nCatching SIGINT\nShutting Down\n");
    refresh();
    sleep(1);
          TearDown(Dptr);
    NcursesExit();
          exit(1);
      }
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

  if (Pause) waddstr(p1head, "AccountNumber\t\t*Paused*\n");
  else waddstr(p1head, "AccountNumber\n");

  PREFRESHP1HEAD;

  // move to the top left corner and output Account Summary Statistics (Part 1)
  wmove(p1accounts, 0, 0);
  for (i = 0; i < Dptr->count; i++)
    wprintw(p1accounts, "%s\n",  Dptr->dptr[i]->accountNumber);

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
    wprintw(p2head, "AccountNumber:  *Paused*\t%s\n",
                      dptr[0].accountNumber);
  else
    wprintw(p2head, "AccountNumber: \t\t%s\t\t\n",
    //wprintw(p2head, "IPs:count %s GET: %d POST: %d Total Requests: %d part2rows %d totalrowsp2: %d num_ips: %d num_requests %d\n",
                      dptr->accountNumber);

  PREFRESHP2HEAD;

  // move to top left of p2ips pad
  wmove(p2ips, 0, 0);

  PREFRESHP2IPS;

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

  /*
  for (i = 0; i < Dptr->count; i++)
    for (j = 0; j < Dptr->dptr[i]->num_requests; j++)
      free(Dptr->dptr[i]->requests[j]);

  for (i = 0; i < Dptr->count; i++)
    for (j = 0; j < Dptr->dptr[i]->num_ips; j++)
      free(Dptr->dptr[i]->ips[j]);
  */

  for (i = 0; i < count; i++)
    free(Dptr->dptr[i]);

  free(Dptr);
  Dptr = NULL;

  return;
}

/*
 * main capture  function
 */
int RaiseCurtain(Accounts *Dptr, char *accountNumber) {

  // set control-c handler
  //signal(SIGINT, mysighand);

  AddAccount(Dptr, accountNumber);

  // fire up Ncurses
  NcursesInit(Dptr);

  RefreshAll();
  
  return 0;
}

/*
 * function to print command usage
 */
void PrintUsage(char **argv, Accounts *Dptr)
{
  fprintf(stderr, "%s version %.1f\n", argv[0], version);
  fprintf(stderr, "Usage: %s <account_number>\n\n", argv[0]);
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
  if (argc > 2) 
    PrintUsage(argv, Dptr);

  // start up user input thread
  //pthread_create (&user_input, NULL, (void *) &UserInput, (void *) Dptr);

  // start up part_switcher thread
  //pthread_create (&part_switcher, NULL, (void *) &PartSwitcher, (void *) Dptr);

  // start the Print Screen thread
  //if (realtime == 1)
    //pthread_create (&print_screen, NULL, (void *) &PrintScreen, (void *) Dptr);

  // main method 
  //if (argc == 2)
    //RaiseCurtain(Dptr, argv[1]);

  AddAccount(Dptr, argv[1]);

  printf("Account Number: %s\n", Dptr->dptr[0]->accountNumber);

  //UserInput(Dptr);

  // free up data structures
  TearDown(Dptr);

  return(0);
}
