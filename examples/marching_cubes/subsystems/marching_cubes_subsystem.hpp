#ifndef _MARCHING_CUBES_SUBSYSTEM_HPP_
#define _MARCHING_CUBES_SUBSYSTEM_HPP_

struct MarchingCubesSubsystemInterface
{
    static constexpr const char* NAME = "MarchingCubesSubsystemInterface";
};

struct MarchingCubesSubsystem
{
    using Interface = MarchingCubesSubsystemInterface;
    static constexpr const char* NAME = "marching_cubes_subsystem";
};

#endif
