#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#define ROWS 20 // no of rows
#define COLS 20 // no of cols
#define SNAKE_CAP (ROWS * COLS)

// Types
typedef enum { cell_empty, cell_egg, cell_head, cell_body } Cell;

typedef struct {
  int x;
  int y;
} Point;

typedef enum { up, down, left, right } Dir;

// Functions
int mod(int, int);
void msleep(int);

void set_nonblocking_io();
void setup_terminal();
void reset_terminal();

Cell *cell_at(int, int);
void step_point(Point*, Dir);
void display();
void snake_add(int, int);
void snake_del();
void spawn_snake(int, int, Dir, int);
void spawn_egg();

void get_high_score();
void set_high_score();

void quit(int);

// Globals
struct termios tattr;
struct termios saved_tattr;

const char *symbols = " o@.";

Cell cells[ROWS][COLS] = {cell_empty};

Point snake[SNAKE_CAP];
int snake_begin = 0;
int snake_size = 0;
Dir snake_head_dir;
Point head_pos;
int score = 0;
int high_score;

int main() {
  char key;
  int pause_flag;
  ssize_t bytes_read;

  srandom(time(NULL));

  get_high_score();

  setup_terminal();
  set_nonblocking_io();

  bytes_read = read(STDIN_FILENO, &key, 1);
  printf("\033[2J\033[;H");
  puts("Use h/j/k/l to move the snake,\nq to quit the game and p to pause/resume.\nPress any key to start!");
  while (bytes_read == EOF) {
    bytes_read = read(STDIN_FILENO, &key, 1);
  }

  spawn_snake(ROWS / 2, COLS / 2, left, 1);
  spawn_egg();

  pause_flag = 0;
  while (1) {
    // Clear the screen.
    printf("\033[2J\033[;H");
    display();

    bytes_read = read(STDIN_FILENO, &key, 1);
    if (bytes_read > 0) {
      switch (key) {
      case 'k':
        if (snake_head_dir != down)
          snake_head_dir = up;
        break;
      case 'j':
        if (snake_head_dir != up)
          snake_head_dir = down;
        break;
      case 'h':
        if (snake_head_dir != right)
          snake_head_dir = left;
        break;
      case 'l':
        if (snake_head_dir != left)
          snake_head_dir = right;
        break;
      case 'q':
        quit(0);
        break;
      case 'p':
        pause_flag = !pause_flag;
      }
    } else if (EAGAIN != errno) {
      // for errors other than EAGAIN.
      // EAGAIN in this context means no input was
      // provided from stdin.
      perror("read() failed");
      quit(1);
    }

    if (!pause_flag) {
      // Mark current head_pos as part of the body.
      *cell_at(head_pos.x, head_pos.y) = cell_body;
      step_point(&head_pos, snake_head_dir);

      Cell head_state = *cell_at(head_pos.x, head_pos.y);

      if (head_state == cell_body) {
        // Snake collided with itself.
        printf("You lost!\n");
        quit(0);
      } else if (head_state == cell_egg) {
        // Snake ate an egg.
        score++;
        spawn_egg();
      } else {
        // Snake entered an empty cell.
        // Move the snake_begin index and decrement the size.
        snake_del();
      }
      snake_add(head_pos.x, head_pos.y);
      // New head_pos is the head of the snake now.
      *cell_at(head_pos.x, head_pos.y) = cell_head;
    }
    msleep(150);
  }

  return 0;
}

int mod(int a, int b) {
  int res = a % b;
  if (res < 0)
    return res + b;
  return res;
}

void set_nonblocking_io() {
  int fd, flags;
  // file descriptor for stdin, usually 0
  fd = fileno(stdin);
  // get the file access flags
  flags = fcntl(fd, F_GETFL);
  // set the stdin in non-blocking mode
  // this allows the program to continue executing
  // the loop without waiting for user input
  // any input provided hence will change the game state
  fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void reset_terminal() { tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_tattr); }

void quit(int exit_code) {
  set_high_score();
  reset_terminal();
  exit(exit_code);
}

void setup_terminal() {

  if (!isatty(STDIN_FILENO)) {
    perror("ERROR! This game only works in a terminal!\n");
    quit(1);
  }
  tcgetattr(STDIN_FILENO, &saved_tattr);

  tcgetattr(STDIN_FILENO, &tattr);
  // set the noncanonical and no echo flags
  tattr.c_lflag &= ~(ICANON | ECHO);
  tattr.c_cc[VMIN] = 1;  // read a single character
  tattr.c_cc[VTIME] = 0; // wait indefinitely for another input i.e. 0 timeout
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &tattr);
}

void msleep(int ms) { usleep(1000 * ms); }

Cell *cell_at(int x, int y) { return &cells[x][y]; }

void step_point(Point *p, Dir dir) {
  switch (dir) {
  case up:
    p->x = mod(p->x - 1, ROWS);
    break;
  case down:
    p->x = mod(p->x + 1, ROWS);
    break;
  case left:
    p->y = mod(p->y - 1, COLS);
    break;
  case right:
    p->y = mod(p->y + 1, COLS);
    break;
  default:
    puts("Unreachable");
    quit(1);
  }
}

void snake_add(int x, int y) {
  // x -> ROWS
  // y -> COLS
  int add_at;
  if (snake_size >= SNAKE_CAP) {
    printf("You Won!");
    quit(0);
  }
  // Mark the cell at x, y to have snake.
  *cell_at(x, y) = cell_body;
  // Add that point to the snake array of Points
  add_at = (snake_begin + snake_size) % SNAKE_CAP;
  snake[add_at].x = x;
  snake[add_at].y = y;
  snake_size++;
}

void snake_del() {
  if (snake_size <= 0) {
    puts("Assertion Failed: Snake underflow!");
    quit(1);
  }
  int x = snake[snake_begin].x;
  int y = snake[snake_begin].y;
  *cell_at(x, y) = cell_empty;
  snake_size--;
  snake_begin = (snake_begin + 1) % SNAKE_CAP;
}

void display() {
  int i, j;

  for (j = 0; j < COLS + 2; j++)
    printf("██");
  putchar('\n');
  for (i = 0; i < ROWS; i++) {
    for (j = 0; j < COLS; j++) {
      if (j == 0) {
        printf("██");
      }
      printf("%c ", symbols[cells[i][j]]);
      if (j == COLS - 1) {
        printf("██");
      }
    }
    putchar('\n');
  }
  for (j = 0; j < COLS + 2; j++)
    printf("██");
  putchar('\n');

  printf("Score: %d\nHigh Score: %d\n", score, high_score);
}

void spawn_snake(int x, int y, Dir dir, int size) {
  head_pos.x = x;
  head_pos.y = y;
  snake_head_dir = dir;
  // body
  for (int i = 0; i < size - 1; i++) {
    snake_add(head_pos.x, head_pos.y);
    step_point(&head_pos, snake_head_dir);
  }
  // head
  snake_add(head_pos.x, head_pos.y);
  *cell_at(head_pos.x, head_pos.y) = cell_head;
}

void spawn_egg() {
  int rand_x, rand_y;
  while (1) {
    rand_x = rand() % ROWS;
    rand_y = rand() % COLS;
    if (*cell_at(rand_x, rand_y) == cell_empty) {
      *cell_at(rand_x, rand_y) = cell_egg;
      break;
    }
  }
}

void get_high_score() {
  FILE *high_score_file;
  high_score_file = fopen("highscore", "rb");
  if (high_score_file) {
    // read the highscore if the file exists
    fscanf(high_score_file, "%d", &high_score);
    fclose(high_score_file);
  }
  else {
    // create a new highscore file initialized as 0
    high_score = 0;
    high_score_file = fopen("highscore", "wb");
    fprintf(high_score_file, "%d", high_score);
    fclose(high_score_file);
  }
}

void set_high_score() {
  // set new highscore
  FILE *high_score_file;
  if (score > high_score) {
    high_score_file = fopen("highscore", "wb");
    fprintf(high_score_file, "%d", score);
    fclose(high_score_file);
  }
}
