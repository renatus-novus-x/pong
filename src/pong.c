#include <x68k/iocs.h>
#include <stdint.h>

/* Human68k-free PONG: mode 12, 512 x 480 visible, 60 Hz. */

#define FIELD_W 512
#define FIELD_H 480
#define BALL_SIZE 4
#define BALL_SPEED 4
#define PADDLE_W 12
#define PADDLE_H 56
#define PADDLE_SPEED 8
#define CPU_SPEED 6
#define WIN_SCORE 3
#define TITLE_DEMO_WAIT_FRAMES (60 * 10)
#define DEMO_DURATION_FRAMES   (60 * 15)

#define MODE_ONE_PLAYER 1
#define MODE_TWO_PLAYER 2

#define COLOR_BLACK  0x0000
#define COLOR_WHITE  0xffff
#define COLOR_ACCENT 0x07e0

#define KEY_ESC   0x01
#define KEY_Q     0x10
#define KEY_W     0x11
#define KEY_S     0x1f
#define KEY_SPACE 0x35
#define KEY_UP    0x3c
#define KEY_DOWN  0x3e

#define JOY_UP            0x01
#define JOY_DOWN          0x02
#define JOY_BUTTON        0x20
#define JOY_ACTIVITY_MASK 0x6f

#define CRTC_R04 ((void *)0x00E80008UL)
#define CRTC_R05 ((void *)0x00E8000AUL)
#define CRTC_R06 ((void *)0x00E8000CUL)
#define CRTC_R07 ((void *)0x00E8000EUL)
#define MFP_GPIP ((const void *)0x00E88001UL)

#define CENTISEC_PER_DAY 8640000L
#define GPIP_VDISP 0x10
#define WAIT_VDISP_TIMEOUT_CS 100
#define WINNER_HOLD_CS 300

typedef struct {
  int x;
  int y;
} Vec2i;

typedef struct {
  int x;
  int y;
  int vx;
  int vy;
} Ball;

typedef enum {
  GAME_MODE_TITLE,
  GAME_MODE_HOW_TO_PLAY,
  GAME_MODE_PONG,
  GAME_MODE_DEMO,
  GAME_MODE_WINNER,
  GAME_MODE_EXIT,
  GAME_MODE_COUNT = GAME_MODE_EXIT
} GameModeId;

typedef enum {
  PONG_CONTROLLER_PLAYER1,
  PONG_CONTROLLER_PLAYER2,
  PONG_CONTROLLER_CPU
} PongController;

typedef struct GameContext GameContext;

typedef struct {
  int selected;
  int idle_frames;
  int old_up;
  int old_down;
  int old_confirm;
} TitleState;

typedef struct {
  int input_released;
  int blocked_scan;
} HowToPlayState;

typedef struct {
  Vec2i left;
  Vec2i right;
  Ball ball;
  PongController left_controller;
  PongController right_controller;
  int mode;
  int left_score;
  int right_score;
  int frame;
} PongGameState;

typedef struct {
  int elapsed_frames;
} DemoState;

typedef struct {
  int player;
  struct iocs_time start;
} WinnerState;

typedef struct {
  int old_mode;
  GameModeId mode;
} ApplicationState;

typedef struct {
  void (*initialize)(GameContext *context);
  GameModeId (*update)(GameContext *context);
  void (*finalize)(GameContext *context);
} GameMode;

struct GameContext {
  ApplicationState application;
  TitleState title;
  HowToPlayState how_to_play;
  PongGameState pong;
  DemoState demo;
  WinnerState winner;
};

typedef struct {
  int left_up;
  int left_down;
  int right_up;
  int right_down;
  int quit;
} Controls;

/* Compact 5 x 7 font: 0-9, then A-Z. */
static const uint8_t font5x7[36][7] = {
  {0x0e,0x11,0x13,0x15,0x19,0x11,0x0e},
  {0x04,0x0c,0x04,0x04,0x04,0x04,0x0e},
  {0x0e,0x11,0x01,0x02,0x04,0x08,0x1f},
  {0x1e,0x01,0x01,0x0e,0x01,0x01,0x1e},
  {0x02,0x06,0x0a,0x12,0x1f,0x02,0x02},
  {0x1f,0x10,0x10,0x1e,0x01,0x01,0x1e},
  {0x0e,0x10,0x10,0x1e,0x11,0x11,0x0e},
  {0x1f,0x01,0x02,0x04,0x08,0x08,0x08},
  {0x0e,0x11,0x11,0x0e,0x11,0x11,0x0e},
  {0x0e,0x11,0x11,0x0f,0x01,0x01,0x0e},
  {0x0e,0x11,0x11,0x1f,0x11,0x11,0x11},
  {0x1e,0x11,0x11,0x1e,0x11,0x11,0x1e},
  {0x0f,0x10,0x10,0x10,0x10,0x10,0x0f},
  {0x1e,0x11,0x11,0x11,0x11,0x11,0x1e},
  {0x1f,0x10,0x10,0x1e,0x10,0x10,0x1f},
  {0x1f,0x10,0x10,0x1e,0x10,0x10,0x10},
  {0x0f,0x10,0x10,0x17,0x11,0x11,0x0f},
  {0x11,0x11,0x11,0x1f,0x11,0x11,0x11},
  {0x0e,0x04,0x04,0x04,0x04,0x04,0x0e},
  {0x01,0x01,0x01,0x01,0x11,0x11,0x0e},
  {0x11,0x12,0x14,0x18,0x14,0x12,0x11},
  {0x10,0x10,0x10,0x10,0x10,0x10,0x1f},
  {0x11,0x1b,0x15,0x15,0x11,0x11,0x11},
  {0x11,0x19,0x15,0x13,0x11,0x11,0x11},
  {0x0e,0x11,0x11,0x11,0x11,0x11,0x0e},
  {0x1e,0x11,0x11,0x1e,0x10,0x10,0x10},
  {0x0e,0x11,0x11,0x11,0x15,0x12,0x0d},
  {0x1e,0x11,0x11,0x1e,0x14,0x12,0x11},
  {0x0f,0x10,0x10,0x0e,0x01,0x01,0x1e},
  {0x1f,0x04,0x04,0x04,0x04,0x04,0x04},
  {0x11,0x11,0x11,0x11,0x11,0x11,0x0e},
  {0x11,0x11,0x11,0x11,0x11,0x0a,0x04},
  {0x11,0x11,0x11,0x15,0x15,0x15,0x0a},
  {0x11,0x11,0x0a,0x04,0x0a,0x11,0x11},
  {0x11,0x11,0x0a,0x04,0x04,0x04,0x04},
  {0x1f,0x01,0x02,0x04,0x08,0x10,0x1f}
};

static inline long ontime_diff_cs(struct iocs_time start, struct iocs_time end) {
  return ((long)end.day - (long)start.day) * CENTISEC_PER_DAY
       + (long)end.sec - (long)start.sec;
}

static inline uint8_t read_gpip(void) {
  return (uint8_t)_iocs_b_bpeek(MFP_GPIP);
}

static inline void write_crtc(void *addr, uint16_t value) {
  _iocs_b_wpoke(addr, value);
}

static int wait_vdisp(void) {
  struct iocs_time start = _iocs_ontime();
  int level = read_gpip() & GPIP_VDISP;
  for (;;) {
    int next = read_gpip() & GPIP_VDISP;
    if (ontime_diff_cs(start, _iocs_ontime()) > WAIT_VDISP_TIMEOUT_CS) return -1;
    if (next != level) {
      level = next;
      if (level != 0) return 0;
    }
  }
}

static int set_60hz(void) {
  if (wait_vdisp() != 0) return -1;
  write_crtc(CRTC_R05, 0x0001);
  write_crtc(CRTC_R06, 0x0022);
  write_crtc(CRTC_R07, 0x0202);
  write_crtc(CRTC_R04, 0x020c);
  return 0;
}

static void application_finalize(GameContext *context) {
  int mode = context->application.old_mode;

  if (mode < 0 || mode > 0x7f) mode = 12;
  _iocs_crtmod(mode);
  _iocs_g_clr_on();
}

static void draw_fill_block(int x1, int y1, int x2, int y2, iocs_color_t color) {
  struct iocs_fillptr rect;
  rect.x1 = (short)x1;
  rect.y1 = (short)y1;
  rect.x2 = (short)x2;
  rect.y2 = (short)y2;
  rect.color = color;
  _iocs_fill(&rect);
}

static void draw_fill(int x, int y, int w, int h, iocs_color_t color) {
  if (x < 0) {
    w += x;
    x = 0;
  }
  if (y < 0) {
    h += y;
    y = 0;
  }
  if (x + w > FIELD_W) w = FIELD_W - x;
  if (y + h > FIELD_H) h = FIELD_H - y;
  if (w > 0 && h > 0) {
    draw_fill_block(x, y, x + w - 1, y + h - 1, color);
  }
}

static uint8_t glyph_row(char c, int row) {
  static const uint8_t arrow[7] = {0x10,0x08,0x04,0x02,0x04,0x08,0x10};
  if (c >= '0' && c <= '9') return font5x7[c - '0'][row];
  if (c >= 'A' && c <= 'Z') return font5x7[10 + c - 'A'][row];
  if (c == '-') return row == 3 ? 0x1f : 0;
  if (c == '!') return (row < 5 || row == 6) ? 0x04 : 0;
  if (c == '>') return arrow[row];
  return 0;
}

static int text_width(const char *text, int scale) {
  int count = 0;
  while (*text++) ++count;
  return count ? count * 6 * scale - scale : 0;
}

static void draw_text(const char *text, int x, int y, int scale, iocs_color_t color) {
  while (*text) {
    int row;
    for (row = 0; row < 7; ++row) {
      uint8_t bits = glyph_row(*text, row);
      int col;
      for (col = 0; col < 5; ++col) {
        if (bits & (0x10 >> col)) {
          draw_fill(x + col * scale, y + row * scale, scale, scale, color);
        }
      }
    }
    x += 6 * scale;
    ++text;
  }
}

static void draw_centered(const char *text, int y, int scale, iocs_color_t color) {
  draw_text(text, (FIELD_W - text_width(text, scale)) / 2, y, scale, color);
}

static void draw_frame(int x, int y, int w, int h, int thickness, iocs_color_t color) {
  draw_fill(x, y, w, thickness, color);
  draw_fill(x, y + h - thickness, w, thickness, color);
  draw_fill(x, y, thickness, h, color);
  draw_fill(x + w - thickness, y, thickness, h, color);
}

static int poll_key(void) {
  if (_iocs_b_keysns() == 0) return -1;
  return _iocs_b_keyinp();
}

static void flush_key_buffer(void) {
  while (_iocs_b_keysns() != 0) {
    (void)_iocs_b_keyinp();
  }
}

static int key_down(int scan) {
  return (_iocs_bitsns(scan >> 3) & (1 << (scan & 7))) != 0;
}

static int pad_down(int port, int mask) {
  return (_iocs_joyget(port) & mask) == 0;
}

static int either_pad_down(int mask) {
  return pad_down(0, mask) || pad_down(1, mask);
}

static int pad_has_activity(int port) {
  return (_iocs_joyget(port) & JOY_ACTIVITY_MASK) != JOY_ACTIVITY_MASK;
}

static int input_has_activity(void) {
  if (_iocs_b_keysns() != 0) return 1;
  if (key_down(KEY_ESC) || key_down(KEY_Q) ||
      key_down(KEY_W) || key_down(KEY_S) ||
      key_down(KEY_SPACE) || key_down(KEY_UP) || key_down(KEY_DOWN)) {
    return 1;
  }
  return pad_has_activity(0) || pad_has_activity(1);
}

static void draw_title(int selected) {
  _iocs_g_clr_on();
  draw_frame(16, 16, FIELD_W - 32, FIELD_H - 32, 4, COLOR_ACCENT);
  draw_centered("X68000", 42, 3, COLOR_WHITE);
  draw_centered("PONG", 86, 12, COLOR_ACCENT);
  draw_centered("PONG", 78, 12, COLOR_WHITE);
  draw_frame(72, 224, 368, 146, 3, COLOR_WHITE);
  draw_text(selected == MODE_ONE_PLAYER ? ">" : " ", 98, 252, 5, COLOR_ACCENT);
  draw_text("1 PLAYER", 142, 252, 5, COLOR_WHITE);
  draw_text(selected == MODE_TWO_PLAYER ? ">" : " ", 98, 310, 5, COLOR_ACCENT);
  draw_text("2 PLAYERS", 142, 310, 5, COLOR_WHITE);
  draw_centered("UP DOWN AND START", 394, 3, COLOR_WHITE);
  draw_centered("KEYBOARD OR PAD", 424, 3, COLOR_ACCENT);
}

static void move_title_cursor(int *selected, int next) {
  int old_y;
  int next_y;

  if (*selected == next) return;
  old_y = *selected == MODE_ONE_PLAYER ? 252 : 310;
  next_y = next == MODE_ONE_PLAYER ? 252 : 310;
  draw_fill(98, old_y, 25, 35, COLOR_BLACK);
  draw_text(">", 98, next_y, 5, COLOR_ACCENT);
  *selected = next;
}

static void title_initialize(GameContext *context) {
  TitleState *state = &context->title;

  state->selected = MODE_ONE_PLAYER;
  state->idle_frames = 0;
  draw_title(state->selected);
  state->old_up = key_down(KEY_W) || key_down(KEY_UP) || either_pad_down(JOY_UP);
  state->old_down = key_down(KEY_S) || key_down(KEY_DOWN) || either_pad_down(JOY_DOWN);
  state->old_confirm = key_down(KEY_SPACE) || either_pad_down(JOY_BUTTON);
}

static GameModeId open_how_to_play(GameContext *context, int mode, int scan) {
  context->pong.mode = mode;
  context->how_to_play.blocked_scan = scan;
  return GAME_MODE_HOW_TO_PLAY;
}

static GameModeId title_update(GameContext *context) {
  TitleState *state = &context->title;
  int event;
  int scan;
  int ascii;
  int up;
  int down;
  int confirm;
  int activity;

  if (wait_vdisp() != 0) return GAME_MODE_EXIT;
  activity = input_has_activity();
  up = key_down(KEY_W) || key_down(KEY_UP) || either_pad_down(JOY_UP);
  down = key_down(KEY_S) || key_down(KEY_DOWN) || either_pad_down(JOY_DOWN);
  confirm = key_down(KEY_SPACE) || either_pad_down(JOY_BUTTON);
  event = poll_key();
  scan = event < 0 ? -1 : ((event >> 8) & 0x7f);
  ascii = event < 0 ? -1 : (event & 0xff);

  if (ascii == '1' || ascii == '2') {
    return open_how_to_play(context,
                            ascii == '1' ? MODE_ONE_PLAYER
                                         : MODE_TWO_PLAYER,
                            scan);
  }
  if (scan == KEY_ESC || scan == KEY_Q || ascii == 'q' || ascii == 'Q') {
    return GAME_MODE_EXIT;
  }
  if ((up && !state->old_up) || scan == KEY_UP || scan == KEY_W) {
    move_title_cursor(&state->selected, MODE_ONE_PLAYER);
  }
  if ((down && !state->old_down) || scan == KEY_DOWN || scan == KEY_S) {
    move_title_cursor(&state->selected, MODE_TWO_PLAYER);
  }
  if ((confirm && !state->old_confirm) ||
      ascii == ' ' || ascii == '\r' || ascii == '\n') {
    return open_how_to_play(context, state->selected, scan);
  }

  state->old_up = up;
  state->old_down = down;
  state->old_confirm = confirm;
  if (activity || event >= 0) {
    state->idle_frames = 0;
  } else if (++state->idle_frames >= TITLE_DEMO_WAIT_FRAMES) {
    return GAME_MODE_DEMO;
  }
  return GAME_MODE_TITLE;
}

static void title_finalize(GameContext *context) {
  (void)context;
}

static void draw_how_to_play(int mode) {
  _iocs_g_clr_on();
  draw_frame(16, 16, FIELD_W - 32, FIELD_H - 32, 4, COLOR_ACCENT);
  draw_centered("HOW TO PLAY", 42, 5, COLOR_WHITE);
  draw_centered(mode == MODE_ONE_PLAYER ? "1 PLAYER" : "2 PLAYERS",
                96, 4, COLOR_ACCENT);

  if (mode == MODE_ONE_PLAYER) {
    draw_centered("PLAYER 1 LEFT PADDLE", 154, 3, COLOR_WHITE);
    draw_centered("W S OR PAD 1 UP DOWN", 190, 3, COLOR_ACCENT);
    draw_centered("PLAYER 2 IS CPU", 244, 3, COLOR_WHITE);
  } else {
    draw_centered("PLAYER 1 LEFT PADDLE", 142, 3, COLOR_WHITE);
    draw_centered("W S OR PAD 1 UP DOWN", 176, 3, COLOR_ACCENT);
    draw_centered("PLAYER 2 RIGHT PADDLE", 224, 3, COLOR_WHITE);
    draw_centered("CURSOR UP DOWN OR PAD 2", 258, 3, COLOR_ACCENT);
  }

  draw_centered("FIRST TO 3 POINTS WINS", 316, 3, COLOR_WHITE);
  draw_centered("Q OR ESC BACK TO TITLE", 356, 3, COLOR_ACCENT);
  draw_centered("SPACE RETURN OR PAD A", 414, 3, COLOR_WHITE);
}

static void how_to_play_initialize(GameContext *context) {
  HowToPlayState *state = &context->how_to_play;

  state->input_released = 0;
  flush_key_buffer();
  draw_how_to_play(context->pong.mode);
}

static GameModeId how_to_play_update(GameContext *context) {
  HowToPlayState *state = &context->how_to_play;
  int event;
  int scan;
  int ascii;
  int confirm;
  int blocked;

  if (wait_vdisp() != 0) return GAME_MODE_EXIT;
  confirm = key_down(KEY_SPACE) || either_pad_down(JOY_BUTTON);
  blocked = state->blocked_scan >= 0 && key_down(state->blocked_scan);
  event = poll_key();
  scan = event < 0 ? -1 : ((event >> 8) & 0x7f);
  ascii = event < 0 ? -1 : (event & 0xff);

  if (scan == KEY_ESC || scan == KEY_Q ||
      ascii == 'q' || ascii == 'Q') {
    return GAME_MODE_TITLE;
  }
  if (!state->input_released) {
    if (!blocked && !confirm && event < 0) state->input_released = 1;
    return GAME_MODE_HOW_TO_PLAY;
  }
  if (confirm || ascii == ' ' || ascii == '\r' || ascii == '\n') {
    return GAME_MODE_PONG;
  }
  return GAME_MODE_HOW_TO_PLAY;
}

static void how_to_play_finalize(GameContext *context) {
  (void)context;
  flush_key_buffer();
}

static void clamp_in_field(int *value, int min, int max) {
  if (*value < min) *value = min;
  else if (*value > max) *value = max;
}

static void pong_reset_ball(PongGameState *state, int direction) {
  state->ball.x = (FIELD_W - BALL_SIZE) / 2;
  state->ball.y = (FIELD_H - BALL_SIZE) / 2;
  state->ball.vx = direction * BALL_SPEED;
  state->ball.vy = (state->frame & 1) ? 3 : -3;
}

static void pong_state_initialize(PongGameState *state, int mode,
                                  PongController left,
                                  PongController right) {
  state->mode = mode;
  state->left_controller = left;
  state->right_controller = right;
  state->left.x = 28;
  state->right.x = FIELD_W - 28 - PADDLE_W;
  state->left.y = (FIELD_H - PADDLE_H) / 2;
  state->right.y = state->left.y;
  state->left_score = 0;
  state->right_score = 0;
  state->frame = 0;
  pong_reset_ball(state, 1);
}

static void read_controller(PongController controller, int *up, int *down) {
  *up = 0;
  *down = 0;
  if (controller == PONG_CONTROLLER_PLAYER1) {
    *up = key_down(KEY_W) || pad_down(0, JOY_UP);
    *down = key_down(KEY_S) || pad_down(0, JOY_DOWN);
  } else if (controller == PONG_CONTROLLER_PLAYER2) {
    *up = key_down(KEY_UP) || pad_down(1, JOY_UP);
    *down = key_down(KEY_DOWN) || pad_down(1, JOY_DOWN);
  }
}

static Controls read_controls(const PongGameState *state) {
  Controls input;
  read_controller(state->left_controller, &input.left_up, &input.left_down);
  read_controller(state->right_controller, &input.right_up, &input.right_down);
  input.quit = key_down(KEY_ESC) || key_down(KEY_Q);
  return input;
}

static void pong_move_cpu_paddle(PongGameState *state, Vec2i *paddle,
                                 int is_left) {
  int paddle_center;
  int target = FIELD_H / 2;

  if ((state->frame & 1) != 0) return;
  if ((is_left && state->ball.vx < 0) ||
      (!is_left && state->ball.vx > 0)) {
    target = state->ball.y + BALL_SIZE / 2;
    target += ((state->frame >> 5) & 1) ? 6 : -6;
  }

  paddle_center = paddle->y + PADDLE_H / 2;
  if (paddle_center < target - 5) paddle->y += CPU_SPEED;
  if (paddle_center > target + 5) paddle->y -= CPU_SPEED;
}

static void pong_move_controllers(PongGameState *state,
                                  const Controls *input) {
  if (input->left_up) state->left.y -= PADDLE_SPEED;
  if (input->left_down) state->left.y += PADDLE_SPEED;
  if (input->right_up) state->right.y -= PADDLE_SPEED;
  if (input->right_down) state->right.y += PADDLE_SPEED;

  if (state->left_controller == PONG_CONTROLLER_CPU) {
    pong_move_cpu_paddle(state, &state->left, 1);
  }
  if (state->right_controller == PONG_CONTROLLER_CPU) {
    pong_move_cpu_paddle(state, &state->right, 0);
  }

  clamp_in_field(&state->left.y, 3, FIELD_H - PADDLE_H - 3);
  clamp_in_field(&state->right.y, 3, FIELD_H - PADDLE_H - 3);
}

static int ball_overlaps(const Ball *ball, const Vec2i *paddle) {
  return ball->x < paddle->x + PADDLE_W && ball->x + BALL_SIZE > paddle->x &&
         ball->y < paddle->y + PADDLE_H && ball->y + BALL_SIZE > paddle->y;
}

static void pong_reflect(PongGameState *state, const Vec2i *paddle, int direction) {
  int offset = state->ball.y + BALL_SIZE / 2 - (paddle->y + PADDLE_H / 2);
  state->ball.vx = direction * BALL_SPEED;
  state->ball.vy += offset / 10;
  clamp_in_field(&state->ball.vy, -8, 8);
  if (state->ball.vy == 0) state->ball.vy = (state->frame & 1) ? 2 : -2;
}

static void pong_update_ball(PongGameState *state) {
  state->ball.x += state->ball.vx;
  state->ball.y += state->ball.vy;
  if (state->ball.y <= 3) {
    state->ball.y = 3;
    state->ball.vy = -state->ball.vy;
  }
  if (state->ball.y >= FIELD_H - BALL_SIZE - 3) {
    state->ball.y = FIELD_H - BALL_SIZE - 3;
    state->ball.vy = -state->ball.vy;
  }
  if (state->ball.vx < 0 && ball_overlaps(&state->ball, &state->left)) {
    state->ball.x = state->left.x + PADDLE_W;
    pong_reflect(state, &state->left, 1);
  }
  if (state->ball.vx > 0 && ball_overlaps(&state->ball, &state->right)) {
    state->ball.x = state->right.x - BALL_SIZE;
    pong_reflect(state, &state->right, -1);
  }
  if (state->ball.x <= 2) {
    ++state->right_score;
    pong_reset_ball(state, 1);
  } else if (state->ball.x >= FIELD_W - BALL_SIZE - 2) {
    ++state->left_score;
    pong_reset_ball(state, -1);
  }
}

static void draw_center_line(void) {
  int y;
  for (y = 8; y < FIELD_H - 8; y += 24) draw_fill(FIELD_W / 2 - 1, y, 3, 12, COLOR_WHITE);
}

static void draw_score(const PongGameState *state) {
  char left[2];
  char right[2];
  left[0] = (char)('0' + state->left_score);
  left[1] = 0;
  right[0] = (char)('0' + state->right_score);
  right[1] = 0;
  draw_fill(194, 8, 124, 44, COLOR_BLACK);
  draw_text(left, 210, 12, 5, COLOR_WHITE);
  draw_text(right, 278, 12, 5, COLOR_WHITE);
}

static int ball_overlaps_rect(const Ball *ball, int x, int y, int w, int h) {
  return ball->x < x + w && ball->x + BALL_SIZE > x &&
         ball->y < y + h && ball->y + BALL_SIZE > y;
}

static void pong_restore_ball_background(const PongGameState *previous,
                                         const PongGameState *state) {
  draw_fill(previous->ball.x, previous->ball.y,
            BALL_SIZE, BALL_SIZE, COLOR_BLACK);

  if (ball_overlaps_rect(&previous->ball,
                         FIELD_W / 2 - 1, 0, 3, FIELD_H)) {
    draw_center_line();
  }
  if (previous->left_score != state->left_score ||
      previous->right_score != state->right_score ||
      ball_overlaps_rect(&previous->ball, 194, 8, 124, 44)) {
    draw_score(state);
  }
}

static void pong_draw_dynamic(const PongGameState *state, iocs_color_t paddle_color,
                              iocs_color_t ball_color) {
  draw_fill(state->left.x, state->left.y, PADDLE_W, PADDLE_H, paddle_color);
  draw_fill(state->right.x, state->right.y, PADDLE_W, PADDLE_H, paddle_color);
  draw_fill(state->ball.x, state->ball.y, BALL_SIZE, BALL_SIZE, ball_color);
}

static void pong_redraw_paddle(int x, int old_y, int new_y) {
  if (old_y == new_y) return;
  draw_fill(x, old_y, PADDLE_W, PADDLE_H, COLOR_BLACK);
  draw_fill(x, new_y, PADDLE_W, PADDLE_H, COLOR_ACCENT);
}

static void pong_draw_match(const PongGameState *state) {
  _iocs_g_clr_on();
  draw_frame(0, 0, FIELD_W, FIELD_H, 2, COLOR_WHITE);
  draw_center_line();
  draw_score(state);
  pong_draw_dynamic(state, COLOR_ACCENT, COLOR_WHITE);
}

static void pong_game_update(PongGameState *state, const Controls *input) {
  PongGameState previous = *state;

  pong_move_controllers(state, input);
  pong_update_ball(state);
  ++state->frame;
  pong_restore_ball_background(&previous, state);
  pong_redraw_paddle(state->left.x, previous.left.y, state->left.y);
  pong_redraw_paddle(state->right.x, previous.right.y, state->right.y);
  draw_fill(state->ball.x, state->ball.y,
            BALL_SIZE, BALL_SIZE, COLOR_WHITE);
}

static void pong_initialize(GameContext *context) {
  PongGameState *state = &context->pong;
  PongController right = state->mode == MODE_TWO_PLAYER
                       ? PONG_CONTROLLER_PLAYER2
                       : PONG_CONTROLLER_CPU;

  pong_state_initialize(state, state->mode,
                        PONG_CONTROLLER_PLAYER1, right);
  pong_draw_match(state);
}

static GameModeId pong_update(GameContext *context) {
  PongGameState *state = &context->pong;
  Controls input;

  if (wait_vdisp() != 0) return GAME_MODE_EXIT;
  input = read_controls(state);
  flush_key_buffer();
  if (input.quit) return GAME_MODE_TITLE;

  pong_game_update(state, &input);
  if (state->left_score >= WIN_SCORE) {
    context->winner.player = 1;
    return GAME_MODE_WINNER;
  }
  if (state->right_score >= WIN_SCORE) {
    context->winner.player = 2;
    return GAME_MODE_WINNER;
  }
  return GAME_MODE_PONG;
}

static void pong_finalize(GameContext *context) {
  (void)context;
}

static void demo_initialize(GameContext *context) {
  PongGameState *pong = &context->pong;

  context->demo.elapsed_frames = 0;
  pong_state_initialize(pong, MODE_TWO_PLAYER,
                        PONG_CONTROLLER_CPU, PONG_CONTROLLER_CPU);
  pong_draw_match(pong);
  flush_key_buffer();
}

static GameModeId demo_update(GameContext *context) {
  PongGameState *pong = &context->pong;
  Controls input = {0, 0, 0, 0, 0};

  if (wait_vdisp() != 0) return GAME_MODE_EXIT;
  if (input_has_activity()) return GAME_MODE_TITLE;

  pong_game_update(pong, &input);
  if (pong->left_score >= WIN_SCORE ||
      pong->right_score >= WIN_SCORE) {
    pong->left_score = 0;
    pong->right_score = 0;
    draw_score(pong);
  }
  if (++context->demo.elapsed_frames >= DEMO_DURATION_FRAMES) {
    return GAME_MODE_TITLE;
  }
  return GAME_MODE_DEMO;
}

static void demo_finalize(GameContext *context) {
  (void)context;
  flush_key_buffer();
}

static void draw_crown(void) {
  draw_fill(190, 112, 132, 18, COLOR_ACCENT);
  draw_fill(190, 82, 24, 32, COLOR_ACCENT);
  draw_fill(244, 68, 24, 46, COLOR_ACCENT);
  draw_fill(298, 82, 24, 32, COLOR_ACCENT);
  draw_fill(198, 130, 116, 14, COLOR_WHITE);
}

static void winner_initialize(GameContext *context) {
  const PongGameState *pong = &context->pong;
  WinnerState *state = &context->winner;
  const char *message;

  if (state->player == 1) {
    message = pong->mode == MODE_ONE_PLAYER ? "PLAYER WINS" : "PLAYER 1 WINS";
  } else {
    message = pong->mode == MODE_ONE_PLAYER ? "CPU WINS" : "PLAYER 2 WINS";
  }
  _iocs_g_clr_on();
  draw_frame(16, 16, FIELD_W - 32, FIELD_H - 32, 4, COLOR_ACCENT);
  draw_crown();
  draw_centered("CHAMPION", 174, 7, COLOR_WHITE);
  draw_centered(message, 260, 5, COLOR_ACCENT);
  draw_centered("CONGRATULATIONS", 330, 3, COLOR_WHITE);
  draw_centered("BACK TO TITLE", 402, 3, COLOR_WHITE);
  flush_key_buffer();
  state->start = _iocs_ontime();
}

static GameModeId winner_update(GameContext *context) {
  WinnerState *state = &context->winner;

  if (wait_vdisp() != 0) return GAME_MODE_EXIT;
  flush_key_buffer();
  if (ontime_diff_cs(state->start, _iocs_ontime()) >= WINNER_HOLD_CS) {
    return GAME_MODE_TITLE;
  }
  return GAME_MODE_WINNER;
}

static void winner_finalize(GameContext *context) {
  (void)context;
  flush_key_buffer();
}

static int application_initialize(GameContext *context) {
  context->application.old_mode = _iocs_crtmod(-1);
  _iocs_crtmod(12);
  _iocs_g_clr_on();
  if (set_60hz() != 0) {
    application_finalize(context);
    return 0;
  }
  return 1;
}

static const GameMode game_modes[GAME_MODE_COUNT] = {
  {title_initialize, title_update, title_finalize},
  {how_to_play_initialize, how_to_play_update, how_to_play_finalize},
  {pong_initialize, pong_update, pong_finalize},
  {demo_initialize, demo_update, demo_finalize},
  {winner_initialize, winner_update, winner_finalize}
};

static void application_loop(GameContext *context) {
  ApplicationState *application = &context->application;

  application->mode = GAME_MODE_TITLE;
  game_modes[application->mode].initialize(context);

  while (application->mode != GAME_MODE_EXIT) {
    GameModeId current = application->mode;
    GameModeId next = game_modes[current].update(context);

    if (next == current) continue;
    game_modes[current].finalize(context);
    application->mode = next;
    if (next != GAME_MODE_EXIT) game_modes[next].initialize(context);
  }
}

int main(void) {
  GameContext context;
  if (!application_initialize(&context)) return 0;
  application_loop(&context);
  application_finalize(&context);
  return 0;
}
