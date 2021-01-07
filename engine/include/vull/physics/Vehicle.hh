#pragma once

#include <vull/core/Entity.hh>

class Vehicle {
    const Entity m_fl_wheel;
    const Entity m_fr_wheel;
    const Entity m_rl_wheel;
    const Entity m_rr_wheel;

public:
    Vehicle(const Entity &fl_wheel, const Entity &fr_wheel, const Entity &rl_wheel, const Entity &rr_wheel)
        : m_fl_wheel(fl_wheel), m_fr_wheel(fr_wheel), m_rl_wheel(rl_wheel), m_rr_wheel(rr_wheel) {}

    const Entity &fl_wheel() const { return m_fl_wheel; }
    const Entity &fr_wheel() const { return m_fr_wheel; }
    const Entity &rl_wheel() const { return m_rl_wheel; }
    const Entity &rr_wheel() const { return m_rr_wheel; }
};
