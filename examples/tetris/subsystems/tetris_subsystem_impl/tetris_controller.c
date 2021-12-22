
#include "tetris_controller.h"
#include "engine/subsystems/se_window_subsystem.h"

// https://tetris.fandom.com/wiki/Tetris_Wiki

typedef struct TetrisFigureRotationDesc
{
    int width;
    int height;
    char desc[4][4]; // y, x
} TetrisFigureRotationDesc;

static const TetrisFigureRotationDesc rotationDescs[TETRIS_FIGURE_TYPE_COUNT][TETRIS_FIGURE_ROTATION_COUNT] =
{
    // TETRIS_FIGURE_TYPE_UNDEFINED
    { { 0 }, { 0 }, { 0 }, { 0 }, },
    // TETRIS_FIGURE_TYPE_I
    {
        {   // TETRIS_FIGURE_ROTATION_UP
            .width = 4, .height = 2, .desc =
            {   {  0 ,  0 ,  0 ,  0  },
                { '@', '@', '@', '@' },
                {  0 ,  0 ,  0 ,  0  },
                {  0 ,  0 ,  0 ,  0  }, }
        },
        {   // TETRIS_FIGURE_ROTATION_RIGHT
            .width = 3, .height = 4, .desc =
            {   {  0 ,  0 , '@',  0  },
                {  0 ,  0 , '@',  0  },
                {  0 ,  0 , '@',  0  },
                {  0 ,  0 , '@',  0  }, }
        },
        {   // TETRIS_FIGURE_ROTATION_DOWN
            .width = 4, .height = 3, .desc =
            {   {  0 ,  0 ,  0 ,  0  },
                {  0 ,  0 ,  0 ,  0  },
                { '@', '@', '@', '@' },
                {  0 ,  0 ,  0 ,  0  }, }
        },
        {   // TETRIS_FIGURE_ROTATION_LEFT
            .width = 2, .height = 4, .desc =
            {   {  0 , '@',  0 ,  0  },
                {  0 , '@',  0 ,  0  },
                {  0 , '@',  0 ,  0  },
                {  0 , '@',  0 ,  0  }, }
        },

    },
    // TETRIS_FIGURE_TYPE_J
    {
        {   // TETRIS_FIGURE_ROTATION_UP
            .width = 3, .height = 2, .desc =
            {   { '@',  0 ,  0 ,  0  },
                { '@', '@', '@',  0  },
                {  0 ,  0 ,  0 ,  0  },
                {  0 ,  0 ,  0 ,  0  }, }
        },
        {   // TETRIS_FIGURE_ROTATION_RIGHT
            .width = 3, .height = 3, .desc =
            {   {  0 , '@', '@',  0  },
                {  0 , '@',  0 ,  0  },
                {  0 , '@',  0 ,  0  },
                {  0 ,  0 ,  0 ,  0  }, }
        },
        {   // TETRIS_FIGURE_ROTATION_DOWN
            .width = 3, .height = 3, .desc =
            {   {  0 ,  0 ,  0 ,  0  },
                { '@', '@', '@',  0  },
                {  0 ,  0 , '@',  0  },
                {  0 ,  0 ,  0 ,  0  }, }
        },
        {   // TETRIS_FIGURE_ROTATION_LEFT
            .width = 3, .height = 3, .desc =
            {   {  0 , '@',  0 ,  0  },
                {  0 , '@',  0 ,  0  },
                { '@', '@',  0 ,  0  },
                {  0 ,  0 ,  0 ,  0  }, }
        },

    },
    // TETRIS_FIGURE_TYPE_L
    {
        {   // TETRIS_FIGURE_ROTATION_UP
            .width = 3, .height = 2, .desc =
            {   {  0 ,  0 , '@',  0 },
                { '@', '@', '@',  0 },
                {  0 ,  0 ,  0 ,  0  },
                {  0 ,  0 ,  0 ,  0  }, }
        },
        {   // TETRIS_FIGURE_ROTATION_RIGHT
            .width = 3, .height = 3, .desc =
            {   {  0 , '@',  0 ,  0  },
                {  0 , '@',  0 ,  0  },
                {  0 , '@', '@',  0  },
                {  0 ,  0 ,  0 ,  0  }, }
        },
        {   // TETRIS_FIGURE_ROTATION_DOWN
            .width = 3, .height = 3, .desc =
            {   {  0 ,  0 ,  0 ,  0  },
                { '@', '@', '@',  0  },
                { '@',  0 ,  0 ,  0  },
                {  0 ,  0 ,  0 ,  0  }, }
        },
        {   // TETRIS_FIGURE_ROTATION_LEFT
            .width = 3, .height = 3, .desc =
            {   { '@', '@',  0 ,  0  },
                {  0 , '@',  0 ,  0  },
                {  0 , '@',  0 ,  0  },
                {  0 ,  0 ,  0 ,  0  }, }
        },
    },
    // TETRIS_FIGURE_TYPE_O
    {
        {   // TETRIS_FIGURE_ROTATION_UP
            .width = 2, .height = 2, .desc =
            {   { '@', '@',  0 ,  0  },
                { '@', '@',  0 ,  0  },
                {  0 ,  0 ,  0 ,  0  },
                {  0 ,  0 ,  0 ,  0  }, }
        },
        {   // TETRIS_FIGURE_ROTATION_DOWN
            .width = 2, .height = 2, .desc =
            {   { '@', '@',  0 ,  0  },
                { '@', '@',  0 ,  0  },
                {  0 ,  0 ,  0 ,  0  },
                {  0 ,  0 ,  0 ,  0  }, }
        },
        {   // TETRIS_FIGURE_ROTATION_LEFT
            .width = 2, .height = 2, .desc =
            {   { '@', '@',  0 ,  0  },
                { '@', '@',  0 ,  0  },
                {  0 ,  0 ,  0 ,  0  },
                {  0 ,  0 ,  0 ,  0  }, }
        },
        {   // TETRIS_FIGURE_ROTATION_RIGHT
            .width = 2, .height = 2, .desc =
            {   { '@', '@',  0 ,  0  },
                { '@', '@',  0 ,  0  },
                {  0 ,  0 ,  0 ,  0  },
                {  0 ,  0 ,  0 ,  0  }, }
        },
    },
    // TETRIS_FIGURE_TYPE_S
    {
        {   // TETRIS_FIGURE_ROTATION_UP
            .width = 3, .height = 2, .desc =
            {   {  0 , '@', '@',  0  },
                { '@', '@',  0 ,  0  },
                {  0 ,  0 ,  0 ,  0  },
                {  0 ,  0 ,  0 ,  0  }, }
        },
        {   // TETRIS_FIGURE_ROTATION_RIGHT
            .width = 3, .height = 3, .desc =
            {   {  0 , '@',  0 ,  0  },
                {  0 , '@', '@',  0  },
                {  0 ,  0 , '@',  0  },
                {  0 ,  0 ,  0 ,  0  }, }
        },
        {   // TETRIS_FIGURE_ROTATION_DOWN
            .width = 3, .height = 3, .desc =
            {   {  0 ,  0 ,  0 ,  0  },
                {  0 , '@', '@',  0  },
                { '@', '@',  0 ,  0  },
                {  0 ,  0 ,  0 ,  0  }, }
        },
        {   // TETRIS_FIGURE_ROTATION_LEFT
            .width = 2, .height = 3, .desc =
            {   { '@',  0 ,  0 ,  0  },
                { '@', '@',  0 ,  0  },
                {  0 , '@',  0 ,  0  },
                {  0 ,  0 ,  0 ,  0  }, }
        },
    },
    // TETRIS_FIGURE_TYPE_Z
    {
        {   // TETRIS_FIGURE_ROTATION_UP
            .width = 3, .height = 2, .desc =
            {   { '@', '@',  0 ,  0  },
                {  0 , '@', '@',  0  },
                {  0 ,  0 ,  0 ,  0  },
                {  0 ,  0 ,  0 ,  0  }, }
        },
        {   // TETRIS_FIGURE_ROTATION_RIGHT
            .width = 3, .height = 3, .desc =
            {   {  0 ,  0 , '@',  0  },
                {  0 , '@', '@',  0  },
                {  0 , '@',  0 ,  0  },
                {  0 ,  0 ,  0 ,  0  }, }
        },
        {   // TETRIS_FIGURE_ROTATION_DOWN
            .width = 3, .height = 3, .desc =
            {   {  0 ,  0 ,  0 ,  0  },
                { '@', '@',  0 ,  0  },
                {  0 , '@', '@',  0  },
                {  0 ,  0 ,  0 ,  0  }, }
        },
        {   // TETRIS_FIGURE_ROTATION_LEFT
            .width = 2, .height = 3, .desc =
            {   {  0 , '@',  0 ,  0  },
                { '@', '@',  0 ,  0  },
                { '@',  0 ,  0 ,  0  },
                {  0 ,  0 ,  0 ,  0  }, }
        },
    },
    // TETRIS_FIGURE_TYPE_T
    {
        {   // TETRIS_FIGURE_ROTATION_UP
            .width = 3, .height = 2, .desc =
            {   {  0 ,' @',  0 ,  0  },
                { '@', '@', '@',  0  },
                {  0 ,  0 ,  0 ,  0  },
                {  0 ,  0 ,  0 ,  0  }, }
        },
        {   // TETRIS_FIGURE_ROTATION_RIGHT
            .width = 3, .height = 3, .desc =
            {   {  0 , '@',  0 ,  0  },
                {  0 , '@',' @',  0  },
                {  0 , '@',  0 ,  0  },
                {  0 ,  0 ,  0 ,  0  }, }
        },
        {   // TETRIS_FIGURE_ROTATION_DOWN
            .width = 3, .height = 3, .desc =
            {   {  0 ,  0 ,  0 ,  0  },
                { '@', '@', '@',  0  },
                {  0 ,' @',  0 ,  0  },
                {  0 ,  0 ,  0 ,  0  }, }
        },
        {   // TETRIS_FIGURE_ROTATION_LEFT
            .width = 3, .height = 3, .desc =
            {   {  0 , '@',  0 ,  0  },
                { '@', '@',  0 ,  0  },
                {  0 , '@',  0 ,  0  },
                {  0 ,  0 ,  0 ,  0  }, }
        },
    },
};

static const int standartWallkicks[8][5][2] =
{
    { {  0,  0 }, { -1,  0 }, { -1,  1 }, {  0, -2 }, { -1, -2 }, }, // up -> right
    { {  0,  0 }, {  1,  0 }, {  1,  1 }, {  0, -2 }, {  1, -2 }, }, // up -> left
    { {  0,  0 }, {  1,  0 }, {  1, -1 }, {  0,  2 }, {  1,  2 }, }, // right -> down
    { {  0,  0 }, {  1,  0 }, {  1, -1 }, {  0,  2 }, {  1,  2 }, }, // right -> up
    { {  0,  0 }, {  1,  0 }, {  1,  1 }, {  0, -2 }, {  1, -2 }, }, // down -> left
    { {  0,  0 }, { -1,  0 }, { -1,  1 }, {  0, -2 }, { -1, -2 }, }, // down -> right
    { {  0,  0 }, { -1,  0 }, { -1, -1 }, {  0,  2 }, { -1,  2 }, }, // left -> up
    { {  0,  0 }, { -1,  0 }, { -1, -1 }, {  0,  2 }, { -1,  2 }, }, // left -> down
};

static const int iWallkicks[8][5][2] =
{
    { {  0,  0 }, { -2,  0 }, {  1,  0 }, { -2, -1 }, {  1,  2 }, }, // up -> right
    { {  0,  0 }, { -1,  0 }, {  2,  0 }, { -1,  2 }, {  2, -1 }, }, // up -> left
    { {  0,  0 }, { -1,  0 }, {  2,  0 }, { -1,  2 }, {  2, -1 }, }, // right -> down
    { {  0,  0 }, {  2,  0 }, { -1,  0 }, {  2,  1 }, { -1, -2 }, }, // right -> up
    { {  0,  0 }, {  2,  0 }, { -1,  0 }, {  2,  1 }, { -1, -2 }, }, // down -> left
    { {  0,  0 }, {  1,  0 }, { -2,  0 }, {  1, -2 }, { -2,  1 }, }, // down -> right
    { {  0,  0 }, {  1,  0 }, { -2,  0 }, {  1, -2 }, { -2,  1 }, }, // left -> up
    { {  0,  0 }, { -2,  0 }, {  1,  0 }, { -2, -1 }, {  1,  2 }, }, // left -> down
};

// https://en.wikipedia.org/wiki/Linear_congruential_generator
static int tetris_controller_rand()
{
    static int seed = 0;
    const int a = 1103515245;
    const int c = 12345;
    const int m = 2147483647;
    seed = (a * seed + c) % m;
    return seed;
}

static void tetris_controller_spawn_new_figure(TetrisState* state)
{
    state->activeFigure.type = (TetrisFigureType)((abs(tetris_controller_rand()) % (TETRIS_FIGURE_TYPE_COUNT - 1)) + 1);
    se_assert(state->activeFigure.type > TETRIS_FIGURE_TYPE_UNDEFINED && state->activeFigure.type < TETRIS_FIGURE_TYPE_COUNT);
    state->activeFigure.rotation = TETRIS_FIGURE_ROTATION_UP;
    state->activeFigure.bottomLeftPointPosX = (TETRIS_FIELD_WIDTH / 2) - (rotationDescs[state->activeFigure.type][state->activeFigure.rotation].width / 2);
    state->activeFigure.bottomLeftPointPosY = TETRIS_FIELD_HEIGHT;
}

static void tetris_controller_leave_active_figure_footprint(TetrisState* state, char symbol)
{
    const TetrisFigureRotationDesc rotationDesc = rotationDescs[state->activeFigure.type][state->activeFigure.rotation];
        for (int x = 0; x < rotationDesc.width; x++)
            for (int y = 0; y < rotationDesc.height; y++)
            {
                const int pointX = state->activeFigure.bottomLeftPointPosX + x;
                const int pointY = state->activeFigure.bottomLeftPointPosY + (rotationDesc.height - 1 - y);
                if (pointX < 0 || pointY < 0 || pointX >= TETRIS_FIELD_WIDTH || pointY >= TETRIS_FIELD_HEIGHT) continue;
                if (!rotationDesc.desc[y][x]) continue;
                state->field[pointX][pointY] = symbol;
            }
}

static bool tetris_controller_is_figure_valid(TetrisState* state, TetrisFigureRotationDesc rotationDesc, int posX, int posY)
{
    for (int x = 0; x < rotationDesc.width; x++)
        for (int y = 0; y < rotationDesc.height; y++)
        {
            const int pointX = posX + x;
            const int pointY = posY + (rotationDesc.height - 1 - y);
            if (!rotationDesc.desc[y][x]) continue;
            if (pointX < 0 || pointY < 0 || pointX >= TETRIS_FIELD_WIDTH)
                return false;
            if (pointY < TETRIS_FIELD_HEIGHT && state->field[pointX][pointY])
                return false;
        }
    return true;
}

void tetris_controller_update(TetrisState* state, const SeWindowSubsystemInput* input, float dt)
{
    //
    // Init new figure and return if this is first update
    //
    if (state->activeFigure.type == TETRIS_FIGURE_TYPE_UNDEFINED)
    {
        tetris_controller_spawn_new_figure(state);
        return;
    }
    const bool isSpeedUpInputActive = se_is_keyboard_button_pressed(input, SE_SPACE);
    const bool isMoveLeftInputActive = se_is_keyboard_button_just_pressed(input, SE_A);
    const bool isMoveRightInputActive = se_is_keyboard_button_just_pressed(input, SE_D);
    const bool isRotateLeftInputActive = se_is_keyboard_button_just_pressed(input, SE_W);
    const bool isRotateRightInputActive = se_is_keyboard_button_just_pressed(input, SE_S);
    const bool needFigureFallUpdate = isSpeedUpInputActive
        ? floor(state->elapsedTimeSec / TETRIS_FALL_UPDATE_FAST_PERIOD_SEC) != floor((state->elapsedTimeSec + dt) / TETRIS_FALL_UPDATE_FAST_PERIOD_SEC)
        : floor(state->elapsedTimeSec / TETRIS_FALL_UPDATE_SLOW_PERIOD_SEC) != floor((state->elapsedTimeSec + dt) / TETRIS_FALL_UPDATE_SLOW_PERIOD_SEC);
    const bool needFieldUpdate = needFigureFallUpdate;
    //
    // Update field
    //
    if (needFieldUpdate)
    {
        for (int y = 0; y < TETRIS_FIELD_HEIGHT; y++)
        {
            bool isRowConnected = true;
            for (int x = 0; x < TETRIS_FIELD_WIDTH; x++)
                if (state->field[x][y] != TETRIS_FILLED_FIELD_SQUARE)
                {
                    isRowConnected = false;
                    break;
                }
            if (isRowConnected)
            {
                for (int y2 = y + 1; y2 < TETRIS_FIELD_HEIGHT; y2++)
                    for (int x = 0; x < TETRIS_FIELD_WIDTH; x++)
                        state->field[x][y2 - 1] = state->field[x][y2];
                y -= 1;
            }
        }
    }
    //
    // Update active figure
    //
    {
        for (int x = 0; x < TETRIS_FIELD_WIDTH; x++)
            for (int y = 0; y < TETRIS_FIELD_HEIGHT; y++)
                if (state->field[x][y] == TETRIS_FILLED_ACTIVE_FIGURE_SQUARE) state->field[x][y] = 0;
        {
            const TetrisFigureRotationDesc currentRotationDesc = rotationDescs[state->activeFigure.type][state->activeFigure.rotation];
            const int currentX = state->activeFigure.bottomLeftPointPosX;
            const int currentY = state->activeFigure.bottomLeftPointPosY;
            if (isMoveLeftInputActive && !isMoveRightInputActive)
                if (tetris_controller_is_figure_valid(state, currentRotationDesc, currentX - 1, currentY))
                    state->activeFigure.bottomLeftPointPosX -= 1;
            if (isMoveRightInputActive && !isMoveLeftInputActive)
                if (tetris_controller_is_figure_valid(state, currentRotationDesc, currentX + 1, currentY))
                    state->activeFigure.bottomLeftPointPosX += 1;
        }
        if (isRotateLeftInputActive && !isRotateRightInputActive)
        {
            const TetrisFigureRotation desiredRotation = state->activeFigure.rotation == TETRIS_FIGURE_ROTATION_UP
                ? TETRIS_FIGURE_ROTATION_LEFT
                : state->activeFigure.rotation - 1;
            const TetrisFigureRotationDesc desiredRotationDesc = rotationDescs[state->activeFigure.type][desiredRotation];
            for (int wallkick = 0; wallkick < 5; wallkick++)
            {
                const int kickedX = state->activeFigure.bottomLeftPointPosX + (state->activeFigure.type == TETRIS_FIGURE_TYPE_I ? iWallkicks[desiredRotation * 2 + 1][wallkick][0] : standartWallkicks[desiredRotation * 2 + 1][wallkick][0]);
                const int kickedY = state->activeFigure.bottomLeftPointPosY + (state->activeFigure.type == TETRIS_FIGURE_TYPE_I ? iWallkicks[desiredRotation * 2 + 1][wallkick][1] : standartWallkicks[desiredRotation * 2 + 1][wallkick][1]);
                if (tetris_controller_is_figure_valid(state, desiredRotationDesc, kickedX, kickedY))
                {
                    state->activeFigure.rotation = desiredRotation;
                    state->activeFigure.bottomLeftPointPosX = kickedX;
                    state->activeFigure.bottomLeftPointPosY = kickedY;
                    break;
                }
            }
        }
        if (isRotateRightInputActive && !isRotateLeftInputActive)
        {
            const TetrisFigureRotation desiredRotation = (state->activeFigure.rotation + 1) % TETRIS_FIGURE_ROTATION_COUNT;
            const TetrisFigureRotationDesc desiredRotationDesc = rotationDescs[state->activeFigure.type][desiredRotation];
            for (int wallkick = 0; wallkick < 5; wallkick++)
            {
                const int kickedX = state->activeFigure.bottomLeftPointPosX + (state->activeFigure.type == TETRIS_FIGURE_TYPE_I ? iWallkicks[desiredRotation * 2][wallkick][0] : standartWallkicks[desiredRotation * 2][wallkick][0]);
                const int kickedY = state->activeFigure.bottomLeftPointPosY + (state->activeFigure.type == TETRIS_FIGURE_TYPE_I ? iWallkicks[desiredRotation * 2][wallkick][1] : standartWallkicks[desiredRotation * 2][wallkick][1]);
                if (tetris_controller_is_figure_valid(state, desiredRotationDesc, kickedX, kickedY))
                {
                    state->activeFigure.rotation = desiredRotation;
                    state->activeFigure.bottomLeftPointPosX = kickedX;
                    state->activeFigure.bottomLeftPointPosY = kickedY;
                    break;
                }
            }
        }
        if (needFigureFallUpdate)
        {
            const TetrisFigureRotationDesc currentRotationDesc = rotationDescs[state->activeFigure.type][state->activeFigure.rotation];
            if (tetris_controller_is_figure_valid(state, currentRotationDesc, state->activeFigure.bottomLeftPointPosX, state->activeFigure.bottomLeftPointPosY - 1))
                state->activeFigure.bottomLeftPointPosY -= 1;
            else
            {
                tetris_controller_leave_active_figure_footprint(state, TETRIS_FILLED_FIELD_SQUARE);
                tetris_controller_spawn_new_figure(state);
            }
        }
        tetris_controller_leave_active_figure_footprint(state, TETRIS_FILLED_ACTIVE_FIGURE_SQUARE);
    }
    //
    // Increase time counter
    //
    state->elapsedTimeSec += dt;
}
