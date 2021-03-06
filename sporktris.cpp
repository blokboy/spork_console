#include "sporktris.h"
#include "config.h"

Tetromino Tetromino::random_piece() {
  Tetromino pieces[] = {
    { // long
      .positions = 2,
      .axis_x = 1,
      .axis_y = 1,
      .cur_pos = 0,
      .points = { {1,0}, {1,1}, {1,2}, {1,3} }
    },
    { // L
      .positions = 4,
      .axis_x = 1,
      .axis_y = 2,
      .cur_pos = 0,
      .points = { {0,2}, {1,2}, {2,2}, {2,3} }
    },
    { // S
      .positions = 2,
      .axis_x = 1,
      .axis_y = 2,
      .cur_pos = 0,
      .points = { {1,2}, {2,2}, {0,3}, {1,3} }
    },
    { // #
      .positions = 1,
      .axis_x = 1,
      .axis_y = 2,
      .cur_pos = 0,
      .points = { {1,2}, {2,2}, {1,3}, {2,3} }
    },
    { // L
      .positions = 4,
      .axis_x = 1,
      .axis_y = 2,
      .cur_pos = 0,
      .points = { {0,2}, {1,2}, {2,2}, {0,3} }
    },
    { // S
      .positions = 2,
      .axis_x = 1,
      .axis_y = 2,
      .cur_pos = 0,
      .points = { {0,2}, {1,2}, {1,3}, {2,3} }
    },
    { // T
      .positions = 4,
      .axis_x = 1,
      .axis_y = 2,
      .cur_pos = 0,
      .points = { {0,2}, {1,2}, {2,2}, {1,3} }
    }
  };
  int index = random(sizeof(pieces) / sizeof(*pieces));
  return pieces[index];
}

Tetromino Tetromino::rotated(bool cw) {
  if (positions == 1) {
    return *this;
  } else if (positions == 2) {
    cw = cur_pos == 0;
  }
  
  Tetromino new_piece = *this;
  new_piece.cur_pos = (cur_pos + 1) % positions;
  for (int i = 0; i < sizeof(points) / sizeof(*points); i++) {
    new_piece.points[i][0] = axis_x + (axis_y - points[i][1]) * (cw ? -1 : 1);
    new_piece.points[i][1] = axis_y + (axis_x - points[i][0]) * (cw ? 1 : -1);
  }
  
  return new_piece;
}

Tetromino::Rect Tetromino::bounding_rect() {
  Tetromino::Rect rect = {.x = 3, .y = 3, .x2 = 0, .y2 = 0};
  for (int i = 0; i < sizeof(points) / sizeof(*points); i++) {
    rect.x = min(rect.x, points[i][0]);
    rect.y = min(rect.y, points[i][1]);
    rect.x2 = max(rect.x2, points[i][0]);
    rect.y2 = max(rect.y2, points[i][1]);
  }
  return rect;
}

byte Tetromino::Rect::center_x() {
  return (x + x2) / 2;
}

bool Sporktris::play() {
  randomSeed(analogRead(A0));
  
  bool board[MAX_DISPLAY_PIXELS];
  memset(board, false, sizeof(board));
  this->board = board;
  need_new_piece = true;
  clearing_lines = false;
  memset(last_input_cycles, 0, sizeof(last_input_cycles));

  bool alive = true;
  unsigned long last_cycle = millis();
  unsigned long cycle_length = 500;
  while (alive) {
    unsigned long now = millis();

    bool should_exit = handle_input();
    if (should_exit) {
      return true;
    }

    if (need_new_piece) {
      cur_piece = Tetromino::random_piece();
      Tetromino::Rect rect = cur_piece.bounding_rect();
      piece_x = disp.width / 2 - rect.center_x();
      // TODO: Figure out why this doesn't work with negative numbers
      piece_y = -rect.y2 - 1;
      need_new_piece = false;
    }

    if (now > last_cycle + cycle_length) {
      alive = cycle();
      last_cycle = now;
    }

    draw();
  }
  return false;
}

bool Sporktris::handle_input() {
  unsigned long now = millis();
  Controller::update_state(&controller, 1);
  if (controller.is_connected()) {
    if (controller[Controller::Button::start]) {
      return true;
    }
    if (controller[Controller::Button::left] && now > last_input_cycles[Controller::Button::left] + 100) {
      if (is_valid_position(cur_piece, piece_x - 1, piece_y)) {
        piece_x--;
        last_input_cycles[Controller::Button::left] = now;
      }
    } else if (controller[Controller::Button::right] && now > last_input_cycles[Controller::Button::right] + 100) {
      if (is_valid_position(cur_piece, piece_x + 1, piece_y)) {
        piece_x++;
        last_input_cycles[Controller::Button::right] = now;
      }
    }
    if (controller[Controller::Button::down] && now > last_input_cycles[Controller::Button::down] + 100) {
      if (is_valid_position(cur_piece, piece_x, piece_y + 1)) {
        piece_y++;
        last_input_cycles[Controller::Button::down] = now;
      }
    }
    if (controller[Controller::Button::a] && now > last_input_cycles[Controller::Button::a] + 200) {
      Tetromino new_piece = cur_piece.rotated(true);
      if (is_valid_position(new_piece, piece_x, piece_y)) {
        cur_piece = new_piece;
        last_input_cycles[Controller::Button::a] = now;
      }
    } else if (controller[Controller::Button::b] && now > last_input_cycles[Controller::Button::b] + 200) {
      Tetromino new_piece = cur_piece.rotated(false);
      if (is_valid_position(new_piece, piece_x, piece_y)) {
        cur_piece = new_piece;
        last_input_cycles[Controller::Button::b] = now;
      }
    }
  }
  return false;
}

bool Sporktris::cycle() {
  bool alive = true;

  // Move all the lines down to fill the cleared lines, instead of moving the piece
  if (clearing_lines) {
    int clear_lines = 0;
    for (int y = disp.height - 1; y >= 0; y--) {
      if (clear_lines > 0) {
        memcpy(&board[(y + clear_lines) * disp.width], &board[y * disp.width], disp.width);
      }
      
      bool is_clear = true;
      for (int x = 0; x < disp.width; x++) {
        if (board[y * disp.width + x]) {
          is_clear = false;
          break;
        }
      }
      if (is_clear) {
        clear_lines++;
      }
    }
    clearing_lines = false;
    return true;
  }
  
  if (is_valid_position(cur_piece, piece_x, piece_y + 1)) {
    piece_y++;
  } else {
    // Apply piece to the board
    int top = disp.height;
    for (int i = 0; i < sizeof(cur_piece.points) / sizeof(*cur_piece.points); i++) {
      int px = cur_piece.points[i][0] + piece_x;
      int py = cur_piece.points[i][1] + piece_y;
      top = min(py, top);
      if (py < 0) {
        continue;
      }
      board[py * disp.width + px] = true;
      
      bool cleared_line = true;
      for (int x = 0; x < disp.width; x++) {
        if (!board[py * disp.width + x]) {
          cleared_line = false;
        }
      }
      if (cleared_line) {
        memset(board + py * disp.width, false, disp.width);
        clearing_lines = true;
      }
    }
    // The player is only alive if the top of the piece that just landed is fully on the board
    alive = top >= 0;
    
    need_new_piece = true;
  }
  return alive;
}

void Sporktris::draw() {
  disp.clear_all();

  // Draw blocks on the board
  for (int x = 0; x < disp.width; x++) {
    for (int y = 0; y < disp.height; y++) {
      disp.set_pixel(x, y, board[y * disp.width + x]);
    }
  }

  // Draw current piece
  for (int i = 0; i < sizeof(cur_piece.points) / sizeof(*cur_piece.points); i++) {
    int px = cur_piece.points[i][0] + piece_x;
    int py = cur_piece.points[i][1] + piece_y;
    disp.set_pixel(px, py, true);
  }
  
  disp.refresh();
}

bool Sporktris::is_valid_position(Tetromino piece, int x, int y) {
  Tetromino::Rect rect = piece.bounding_rect();
  if (rect.x + x < 0 || rect.x2 + x >= disp.width ||
      rect.y2 + y >= disp.height) {
    return false;
  }
  for (int i = 0; i < sizeof(piece.points) / sizeof(*piece.points); i++) {
    int px = piece.points[i][0] + x;
    int py = piece.points[i][1] + y;
    if (py >= 0 && board[py * disp.width + px]) {
      return false;
    }
  }
  return true;
}
