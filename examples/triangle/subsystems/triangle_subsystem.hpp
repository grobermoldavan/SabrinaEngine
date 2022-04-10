#ifndef _TRIANGLE_SUBSYSTEM_H_
#define _TRIANGLE_SUBSYSTEM_H_

struct TriangleSubsystemInterface
{
    static constexpr const char* NAME = "TriangleSubsystemInterface";
};

struct TriangleSubsystem
{
    using Interface = TriangleSubsystemInterface;
    static constexpr const char* NAME = "triangle_subsystem";
};

#endif
