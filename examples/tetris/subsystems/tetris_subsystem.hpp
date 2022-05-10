#ifndef _TETRIS_SUBSYSTEM_HPP_
#define _TETRIS_SUBSYSTEM_HPP_

struct TetrisSubsystemInterface
{
    static constexpr const char* NAME = "TetrisSubsystemInterface";
};

struct TetrisSubsystem
{
    using Interface = TetrisSubsystemInterface;
    static constexpr const char* NAME = "tetris_subsystem";
};

#endif
