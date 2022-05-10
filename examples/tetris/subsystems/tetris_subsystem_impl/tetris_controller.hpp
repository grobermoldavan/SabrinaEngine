#ifndef _TETRIS_CONTROLLER_HPP_
#define _TETRIS_CONTROLLER_HPP_

/*
    Field bottom left corner is [0, 0], top right corner is [TETRIS_FIELD_WIDTH, TETRIS_FIELD_HEIGHT]
*/

#define TETRIS_FIELD_WIDTH 10
#define TETRIS_FIELD_HEIGHT 24
#define TETRIS_FALL_UPDATE_SLOW_PERIOD_SEC 0.5
#define TETRIS_FALL_UPDATE_FAST_PERIOD_SEC 0.1

#define TETRIS_FILLED_FIELD_SQUARE 'x'
#define TETRIS_FILLED_ACTIVE_FIGURE_SQUARE '@'

// https://tetris.fandom.com/wiki/Tetromino
enum TetrisFigureType
{
    TETRIS_FIGURE_TYPE_UNDEFINED,
    TETRIS_FIGURE_TYPE_I,
    TETRIS_FIGURE_TYPE_J,
    TETRIS_FIGURE_TYPE_L,
    TETRIS_FIGURE_TYPE_O,
    TETRIS_FIGURE_TYPE_S,
    TETRIS_FIGURE_TYPE_Z,
    TETRIS_FIGURE_TYPE_T,
    TETRIS_FIGURE_TYPE_COUNT,
};

enum TetrisFigureRotation
{
    TETRIS_FIGURE_ROTATION_UP,
    TETRIS_FIGURE_ROTATION_RIGHT,
    TETRIS_FIGURE_ROTATION_DOWN,
    TETRIS_FIGURE_ROTATION_LEFT,
    TETRIS_FIGURE_ROTATION_COUNT,
};

struct TetrisActiveFigure
{
    TetrisFigureType type;
    TetrisFigureRotation rotation;
    int bottomLeftPointPosX;
    int bottomLeftPointPosY;
};

struct TetrisState
{
    double elapsedTimeSec;
    TetrisActiveFigure activeFigure;
    char field[TETRIS_FIELD_WIDTH][TETRIS_FIELD_HEIGHT];
};

void tetris_controller_update(const struct SeWindowSubsystemInput* input, float dt);

#endif
