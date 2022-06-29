#ifndef _UI_TEST_SUBSYSTEM_HPP_
#define _UI_TEST_SUBSYSTEM_HPP_

struct UiTestSubsystemInterface
{
    static constexpr const char* NAME = "UiTestSubsystemInterface";
};

struct UiTestSubsystem
{
    using Interface = UiTestSubsystemInterface;
    static constexpr const char* NAME = "ui_test_subsystem";
};

#endif
