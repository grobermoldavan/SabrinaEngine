
#include <math.h>
#include "tetris_controller.hpp"
#include "engine/engine.hpp"

// https://tetris.fandom.com/wiki/Tetris_Wiki

struct TetrisFigureRotationDesc
{
    int width;
    int height;
    char desc[4][4]; // y, x
};

static TetrisState g_state = { };

static const TetrisFigureRotationDesc rotationDescs[TETRIS_FIGURE_TYPE_COUNT][TETRIS_FIGURE_ROTATION_COUNT] =
{
    // TETRIS_FIGURE_TYPE_UNDEFINED
    { {'\0'}, {'\0'}, {'\0'}, {'\0'}, },
    // TETRIS_FIGURE_TYPE_I
    {
        {   // TETRIS_FIGURE_ROTATION_UP
            .width = 4, .height = 2, .desc =
            {   { '\0', '\0', '\0', '\0' },
                { '@' , '@' , '@' , '@' },
                { '\0', '\0', '\0', '\0' },
                { '\0', '\0', '\0', '\0' }, }
        },
        {   // TETRIS_FIGURE_ROTATION_RIGHT
            .width = 3, .height = 4, .desc =
            {   { '\0', '\0', '@' , '\0' },
                { '\0', '\0', '@' , '\0' },
                { '\0', '\0', '@' , '\0' },
                { '\0', '\0', '@' , '\0' }, }
        },
        {   // TETRIS_FIGURE_ROTATION_DOWN
            .width = 4, .height = 3, .desc =
            {   { '\0', '\0', '\0', '\0' },
                { '\0', '\0', '\0', '\0' },
                { '@' , '@' , '@' , '@' },
                { '\0', '\0', '\0', '\0' }, }
        },
        {   // TETRIS_FIGURE_ROTATION_LEFT
            .width = 2, .height = 4, .desc =
            {   { '\0', '@' , '\0', '\0' },
                { '\0', '@' , '\0', '\0' },
                { '\0', '@' , '\0', '\0' },
                { '\0', '@' , '\0', '\0' }, }
        },

    },
    // TETRIS_FIGURE_TYPE_J
    {
        {   // TETRIS_FIGURE_ROTATION_UP
            .width = 3, .height = 2, .desc =
            {   { '@' , '\0', '\0', '\0' },
                { '@' , '@' , '@' , '\0' },
                { '\0', '\0', '\0', '\0' },
                { '\0', '\0', '\0', '\0' }, }
        },
        {   // TETRIS_FIGURE_ROTATION_RIGHT
            .width = 3, .height = 3, .desc =
            {   { '\0', '@' , '@' , '\0' },
                { '\0', '@' , '\0', '\0' },
                { '\0', '@' , '\0', '\0' },
                { '\0', '\0', '\0', '\0' }, }
        },
        {   // TETRIS_FIGURE_ROTATION_DOWN
            .width = 3, .height = 3, .desc =
            {   { '\0', '\0', '\0', '\0' },
                { '@' , '@' , '@' , '\0' },
                { '\0', '\0', '@' , '\0' },
                { '\0', '\0', '\0', '\0' }, }
        },
        {   // TETRIS_FIGURE_ROTATION_LEFT
            .width = 3, .height = 3, .desc =
            {   { '\0', '@' , '\0', '\0' },
                { '\0', '@' , '\0', '\0' },
                { '@' , '@' , '\0', '\0' },
                { '\0', '\0', '\0', '\0' }, }
        },

    },
    // TETRIS_FIGURE_TYPE_L
    {
        {   // TETRIS_FIGURE_ROTATION_UP
            .width = 3, .height = 2, .desc =
            {   { '\0', '\0', '@' , '\0' },
                { '@' , '@' , '@' , '\0' },
                { '\0', '\0', '\0', '\0' },
                { '\0', '\0', '\0', '\0' }, }
        },
        {   // TETRIS_FIGURE_ROTATION_RIGHT
            .width = 3, .height = 3, .desc =
            {   { '\0', '@' , '\0', '\0' },
                { '\0', '@' , '\0', '\0' },
                { '\0', '@' , '@' , '\0' },
                { '\0', '\0', '\0', '\0' }, }
        },
        {   // TETRIS_FIGURE_ROTATION_DOWN
            .width = 3, .height = 3, .desc =
            {   { '\0', '\0', '\0', '\0' },
                { '@' , '@' , '@' , '\0' },
                { '@' , '\0', '\0', '\0' },
                { '\0', '\0', '\0', '\0' }, }
        },
        {   // TETRIS_FIGURE_ROTATION_LEFT
            .width = 3, .height = 3, .desc =
            {   { '@' , '@' , '\0', '\0' },
                { '\0', '@' , '\0', '\0' },
                { '\0', '@' , '\0', '\0' },
                { '\0', '\0', '\0', '\0' }, }
        },
    },
    // TETRIS_FIGURE_TYPE_O
    {
        {   // TETRIS_FIGURE_ROTATION_UP
            .width = 2, .height = 2, .desc =
            {   { '@' , '@' , '\0', '\0' },
                { '@' , '@' , '\0', '\0' },
                { '\0', '\0', '\0', '\0' },
                { '\0', '\0', '\0', '\0' }, }
        },
        {   // TETRIS_FIGURE_ROTATION_DOWN
            .width = 2, .height = 2, .desc =
            {   { '@' , '@' , '\0', '\0' },
                { '@' , '@' , '\0', '\0' },
                { '\0', '\0', '\0', '\0' },
                { '\0', '\0', '\0', '\0' }, }
        },
        {   // TETRIS_FIGURE_ROTATION_LEFT
            .width = 2, .height = 2, .desc =
            {   { '@' , '@' , '\0', '\0' },
                { '@' , '@' , '\0', '\0' },
                { '\0', '\0', '\0', '\0' },
                { '\0', '\0', '\0', '\0' }, }
        },
        {   // TETRIS_FIGURE_ROTATION_RIGHT
            .width = 2, .height = 2, .desc =
            {   { '@' , '@' , '\0', '\0' },
                { '@' , '@' , '\0', '\0' },
                { '\0', '\0', '\0', '\0' },
                { '\0', '\0', '\0', '\0' }, }
        },
    },
    // TETRIS_FIGURE_TYPE_S
    {
        {   // TETRIS_FIGURE_ROTATION_UP
            .width = 3, .height = 2, .desc =
            {   { '\0', '@' , '@' , '\0' },
                { '@' , '@' , '\0', '\0' },
                { '\0', '\0', '\0', '\0' },
                { '\0', '\0', '\0', '\0' }, }
        },
        {   // TETRIS_FIGURE_ROTATION_RIGHT
            .width = 3, .height = 3, .desc =
            {   { '\0', '@' , '\0', '\0' },
                { '\0', '@' , '@' , '\0' },
                { '\0', '\0', '@' , '\0' },
                { '\0', '\0', '\0', '\0' }, }
        },
        {   // TETRIS_FIGURE_ROTATION_DOWN
            .width = 3, .height = 3, .desc =
            {   { '\0', '\0', '\0', '\0' },
                { '\0', '@' , '@' , '\0' },
                { '@' , '@' , '\0', '\0' },
                { '\0', '\0', '\0', '\0' }, }
        },
        {   // TETRIS_FIGURE_ROTATION_LEFT
            .width = 2, .height = 3, .desc =
            {   { '@' , '\0', '\0', '\0' },
                { '@' , '@' , '\0', '\0' },
                { '\0', '@' , '\0', '\0' },
                { '\0', '\0', '\0', '\0' }, }
        },
    },
    // TETRIS_FIGURE_TYPE_Z
    {
        {   // TETRIS_FIGURE_ROTATION_UP
            .width = 3, .height = 2, .desc =
            {   { '@' , '@' , '\0', '\0' },
                { '\0', '@' , '@' , '\0' },
                { '\0', '\0', '\0', '\0' },
                { '\0', '\0', '\0', '\0' }, }
        },
        {   // TETRIS_FIGURE_ROTATION_RIGHT
            .width = 3, .height = 3, .desc =
            {   { '\0', '\0', '@' , '\0' },
                { '\0', '@' , '@' , '\0' },
                { '\0', '@' , '\0', '\0' },
                { '\0', '\0', '\0', '\0' }, }
        },
        {   // TETRIS_FIGURE_ROTATION_DOWN
            .width = 3, .height = 3, .desc =
            {   { '\0', '\0', '\0', '\0' },
                { '@' , '@' , '\0', '\0' },
                { '\0', '@' , '@' , '\0' },
                { '\0', '\0', '\0', '\0' }, }
        },
        {   // TETRIS_FIGURE_ROTATION_LEFT
            .width = 2, .height = 3, .desc =
            {   { '\0', '@' , '\0', '\0' },
                { '@' , '@' , '\0', '\0' },
                { '@' , '\0', '\0', '\0' },
                { '\0', '\0', '\0', '\0' }, }
        },
    },
    // TETRIS_FIGURE_TYPE_T
    {
        {   // TETRIS_FIGURE_ROTATION_UP
            .width = 3, .height = 2, .desc =
            {   { '\0', '@' , '\0', '\0' },
                { '@' , '@' , '@' , '\0' },
                { '\0', '\0', '\0', '\0' },
                { '\0', '\0', '\0', '\0' }, }
        },
        {   // TETRIS_FIGURE_ROTATION_RIGHT
            .width = 3, .height = 3, .desc =
            {   { '\0', '@' , '\0', '\0' },
                { '\0', '@' , '@' , '\0' },
                { '\0', '@' , '\0', '\0' },
                { '\0', '\0', '\0', '\0' }, }
        },
        {   // TETRIS_FIGURE_ROTATION_DOWN
            .width = 3, .height = 3, .desc =
            {   { '\0', '\0', '\0', '\0' },
                { '@' , '@' , '@' , '\0' },
                { '\0', '@' , '\0', '\0' },
                { '\0', '\0', '\0', '\0' }, }
        },
        {   // TETRIS_FIGURE_ROTATION_LEFT
            .width = 3, .height = 3, .desc =
            {   { '\0', '@' , '\0', '\0' },
                { '@' , '@' , '\0', '\0' },
                { '\0', '@' , '\0', '\0' },
                { '\0', '\0', '\0', '\0' }, }
        },
    },
};

static const int standartWallkicks[8][5][2] =
{
    { {  0, '\0'}, { -1, '\0'}, { -1,  1 }, {  0, -2 }, { -1, -2 }, }, // up -> right
    { {  0, '\0'}, {  1, '\0'}, {  1,  1 }, {  0, -2 }, {  1, -2 }, }, // up -> left
    { {  0, '\0'}, {  1, '\0'}, {  1, -1 }, {  0,  2 }, {  1,  2 }, }, // right -> down
    { {  0, '\0'}, {  1, '\0'}, {  1, -1 }, {  0,  2 }, {  1,  2 }, }, // right -> up
    { {  0, '\0'}, {  1, '\0'}, {  1,  1 }, {  0, -2 }, {  1, -2 }, }, // down -> left
    { {  0, '\0'}, { -1, '\0'}, { -1,  1 }, {  0, -2 }, { -1, -2 }, }, // down -> right
    { {  0, '\0'}, { -1, '\0'}, { -1, -1 }, {  0,  2 }, { -1,  2 }, }, // left -> up
    { {  0, '\0'}, { -1, '\0'}, { -1, -1 }, {  0,  2 }, { -1,  2 }, }, // left -> down
};

static const int iWallkicks[8][5][2] =
{
    { {  0, '\0'}, { -2, '\0'}, {  1, '\0'}, { -2, -1 }, {  1,  2 }, }, // up -> right
    { {  0, '\0'}, { -1, '\0'}, {  2, '\0'}, { -1,  2 }, {  2, -1 }, }, // up -> left
    { {  0, '\0'}, { -1, '\0'}, {  2, '\0'}, { -1,  2 }, {  2, -1 }, }, // right -> down
    { {  0, '\0'}, {  2, '\0'}, { -1, '\0'}, {  2,  1 }, { -1, -2 }, }, // right -> up
    { {  0, '\0'}, {  2, '\0'}, { -1, '\0'}, {  2,  1 }, { -1, -2 }, }, // down -> left
    { {  0, '\0'}, {  1, '\0'}, { -2, '\0'}, {  1, -2 }, { -2,  1 }, }, // down -> right
    { {  0, '\0'}, {  1, '\0'}, { -2, '\0'}, {  1, -2 }, { -2,  1 }, }, // left -> up
    { {  0, '\0'}, { -2, '\0'}, {  1, '\0'}, { -2, -1 }, {  1,  2 }, }, // left -> down
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

static void tetris_controller_spawn_new_figure()
{
    g_state.activeFigure.type = (TetrisFigureType)((abs(tetris_controller_rand()) % (TETRIS_FIGURE_TYPE_COUNT - 1)) + 1);
    se_assert(g_state.activeFigure.type > TETRIS_FIGURE_TYPE_UNDEFINED && g_state.activeFigure.type < TETRIS_FIGURE_TYPE_COUNT);
    g_state.activeFigure.rotation = TETRIS_FIGURE_ROTATION_UP;
    g_state.activeFigure.bottomLeftPointPosX = (TETRIS_FIELD_WIDTH / 2) - (rotationDescs[g_state.activeFigure.type][g_state.activeFigure.rotation].width / 2);
    g_state.activeFigure.bottomLeftPointPosY = TETRIS_FIELD_HEIGHT;
}

static void tetris_controller_leave_active_figure_footprint(char symbol)
{
    const TetrisFigureRotationDesc rotationDesc = rotationDescs[g_state.activeFigure.type][g_state.activeFigure.rotation];
    for (int y = 0; y < rotationDesc.height; y++)
        for (int x = 0; x < rotationDesc.width; x++)
        {
            const int pointX = g_state.activeFigure.bottomLeftPointPosX + x;
            const int pointY = g_state.activeFigure.bottomLeftPointPosY + (rotationDesc.height - 1 - y);
            if (pointX < 0 || pointY < 0 || pointX >= TETRIS_FIELD_WIDTH || pointY >= TETRIS_FIELD_HEIGHT) continue;
            if (!rotationDesc.desc[y][x]) continue;
            g_state.field[pointX][pointY] = symbol;
        }
}

static bool tetris_controller_is_figure_valid(TetrisFigureRotationDesc rotationDesc, int posX, int posY)
{
    for (int x = 0; x < rotationDesc.width; x++)
        for (int y = 0; y < rotationDesc.height; y++)
        {
            const int pointX = posX + x;
            const int pointY = posY + (rotationDesc.height - 1 - y);
            if (!rotationDesc.desc[y][x]) continue;
            if (pointX < 0 || pointY < 0 || pointX >= TETRIS_FIELD_WIDTH)
                return false;
            if (pointY < TETRIS_FIELD_HEIGHT && g_state.field[pointX][pointY])
                return false;
        }
    return true;
}

void tetris_controller_update(const SeWindowSubsystemInput* input, float dt)
{
    //
    // Init new figure and return if this is first update
    //
    if (g_state.activeFigure.type == TETRIS_FIGURE_TYPE_UNDEFINED)
    {
        tetris_controller_spawn_new_figure();
        return;
    }
    const bool isSpeedUpInputActive = win::is_keyboard_button_pressed(input, SE_SPACE);
    const bool isMoveLeftInputActive = win::is_keyboard_button_just_pressed(input, SE_A);
    const bool isMoveRightInputActive = win::is_keyboard_button_just_pressed(input, SE_D);
    const bool isRotateLeftInputActive = win::is_keyboard_button_just_pressed(input, SE_W);
    const bool isRotateRightInputActive = win::is_keyboard_button_just_pressed(input, SE_S);
    const bool needFigureFallUpdate = isSpeedUpInputActive
        ? floor(g_state.elapsedTimeSec / TETRIS_FALL_UPDATE_FAST_PERIOD_SEC) != floor((g_state.elapsedTimeSec + dt) / TETRIS_FALL_UPDATE_FAST_PERIOD_SEC)
        : floor(g_state.elapsedTimeSec / TETRIS_FALL_UPDATE_SLOW_PERIOD_SEC) != floor((g_state.elapsedTimeSec + dt) / TETRIS_FALL_UPDATE_SLOW_PERIOD_SEC);
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
                if (g_state.field[x][y] != TETRIS_FILLED_FIELD_SQUARE)
                {
                    isRowConnected = false;
                    break;
                }
            if (isRowConnected)
            {
                for (int y2 = y + 1; y2 < TETRIS_FIELD_HEIGHT; y2++)
                    for (int x = 0; x < TETRIS_FIELD_WIDTH; x++)
                        g_state.field[x][y2 - 1] = g_state.field[x][y2];
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
                if (g_state.field[x][y] == TETRIS_FILLED_ACTIVE_FIGURE_SQUARE) g_state.field[x][y] = 0;
        {
            const TetrisFigureRotationDesc currentRotationDesc = rotationDescs[g_state.activeFigure.type][g_state.activeFigure.rotation];
            const int currentX = g_state.activeFigure.bottomLeftPointPosX;
            const int currentY = g_state.activeFigure.bottomLeftPointPosY;
            if (isMoveLeftInputActive && !isMoveRightInputActive)
                if (tetris_controller_is_figure_valid(currentRotationDesc, currentX - 1, currentY))
                    g_state.activeFigure.bottomLeftPointPosX -= 1;
            if (isMoveRightInputActive && !isMoveLeftInputActive)
                if (tetris_controller_is_figure_valid(currentRotationDesc, currentX + 1, currentY))
                    g_state.activeFigure.bottomLeftPointPosX += 1;
        }
        if (isRotateLeftInputActive && !isRotateRightInputActive)
        {
            const TetrisFigureRotation desiredRotation = (TetrisFigureRotation)(g_state.activeFigure.rotation == TETRIS_FIGURE_ROTATION_UP
                ? TETRIS_FIGURE_ROTATION_LEFT
                : g_state.activeFigure.rotation - 1);
            const TetrisFigureRotationDesc desiredRotationDesc = rotationDescs[g_state.activeFigure.type][desiredRotation];
            for (int wallkick = 0; wallkick < 5; wallkick++)
            {
                const int kickedX = g_state.activeFigure.bottomLeftPointPosX + (g_state.activeFigure.type == TETRIS_FIGURE_TYPE_I
                    ? iWallkicks[desiredRotation * 2 + 1][wallkick][0] : standartWallkicks[desiredRotation * 2 + 1][wallkick][0]);
                const int kickedY = g_state.activeFigure.bottomLeftPointPosY + (g_state.activeFigure.type == TETRIS_FIGURE_TYPE_I
                    ? iWallkicks[desiredRotation * 2 + 1][wallkick][1] : standartWallkicks[desiredRotation * 2 + 1][wallkick][1]);
                if (tetris_controller_is_figure_valid(desiredRotationDesc, kickedX, kickedY))
                {
                    g_state.activeFigure.rotation = desiredRotation;
                    g_state.activeFigure.bottomLeftPointPosX = kickedX;
                    g_state.activeFigure.bottomLeftPointPosY = kickedY;
                    break;
                }
            }
        }
        if (isRotateRightInputActive && !isRotateLeftInputActive)
        {
            const TetrisFigureRotation desiredRotation = (TetrisFigureRotation)((g_state.activeFigure.rotation + 1) % TETRIS_FIGURE_ROTATION_COUNT);
            const TetrisFigureRotationDesc desiredRotationDesc = rotationDescs[g_state.activeFigure.type][desiredRotation];
            for (int wallkick = 0; wallkick < 5; wallkick++)
            {
                const int kickedX = g_state.activeFigure.bottomLeftPointPosX + (g_state.activeFigure.type == TETRIS_FIGURE_TYPE_I
                    ? iWallkicks[desiredRotation * 2][wallkick][0] : standartWallkicks[desiredRotation * 2][wallkick][0]);
                const int kickedY = g_state.activeFigure.bottomLeftPointPosY + (g_state.activeFigure.type == TETRIS_FIGURE_TYPE_I
                    ? iWallkicks[desiredRotation * 2][wallkick][1] : standartWallkicks[desiredRotation * 2][wallkick][1]);
                if (tetris_controller_is_figure_valid(desiredRotationDesc, kickedX, kickedY))
                {
                    g_state.activeFigure.rotation = desiredRotation;
                    g_state.activeFigure.bottomLeftPointPosX = kickedX;
                    g_state.activeFigure.bottomLeftPointPosY = kickedY;
                    break;
                }
            }
        }
        if (needFigureFallUpdate)
        {
            const TetrisFigureRotationDesc currentRotationDesc = rotationDescs[g_state.activeFigure.type][g_state.activeFigure.rotation];
            if (tetris_controller_is_figure_valid(currentRotationDesc, g_state.activeFigure.bottomLeftPointPosX, g_state.activeFigure.bottomLeftPointPosY - 1))
                g_state.activeFigure.bottomLeftPointPosY -= 1;
            else
            {
                tetris_controller_leave_active_figure_footprint(TETRIS_FILLED_FIELD_SQUARE);
                tetris_controller_spawn_new_figure();
            }
        }
        tetris_controller_leave_active_figure_footprint(TETRIS_FILLED_ACTIVE_FIGURE_SQUARE);
    }
    //
    // Increase time counter
    //
    g_state.elapsedTimeSec += dt;
}
