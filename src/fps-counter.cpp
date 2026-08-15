#include "fps-counter.hpp"
#include "nanosecond.inl"
#include <cmath>
#include <mg/Constants.h>

namespace floormat {

namespace {

// 1€ filter (Casiez, Roussel & Vogel, CHI 2012), except the gate is the relative deviation
// itself, not its derivative: dividing by dt makes the reading 10x twitchier at 1000 fps.
constexpr inline double fc_min   = 0.15; // Hz, cutoff with the gate shut; tau = 1/(2*pi*fc) = 1.06 s
constexpr inline double beta     = 3.5;  // Hz per unit of relative deviation
constexpr inline double d_cutoff = 0.5;  // Hz, low-pass on the gate itself

constexpr inline double min_dt = 1e-9;
constexpr inline double two_pi = 2 * Math::Constants<double>::pi();

// The published beta values are calibrated against this form, not the exact 1 - exp(-dt/tau),
// which gives a larger alpha for long frames and so makes a stall register harder.
double filter_alpha(double cutoff_hz, double dt)
{
    const double tau = 1 / (two_pi * cutoff_hz);
    return 1 / (1 + tau/dt);
}

} // namespace

FPS_Counter::FPS_Counter(Ns settle_time) noexcept: settle_time{settle_time} {}

void FPS_Counter::reset()
{
    frame_time = 0;
    change_rate = 0;
    total_time = Ns{};
    value_ok = false;
}

float FPS_Counter::get() const
{
    return value_ok ? (float)(1 / frame_time) : 0;
}

float FPS_Counter::update(Ns ns)
{
    if (ns > 10*Seconds) [[unlikely]]
    {
        reset();
        return 0;
    }

    const double dt = std::fmax(min_dt, (double)ns.stamp * 1e-9);
    total_time += ns;

    // Seed rather than blend: the frame that ends the settle period would otherwise inherit
    // a prior built from unrepresentative startup frames.
    if (!value_ok) [[unlikely]]
    {
        frame_time = dt;
        change_rate = 0;
        value_ok = total_time >= settle_time;
        return get();
    }

    const double rel = (dt - frame_time) / frame_time;
    change_rate += filter_alpha(d_cutoff, dt) * (rel - change_rate);
    const double cutoff = fc_min + beta * std::fabs(change_rate);
    frame_time += filter_alpha(cutoff, dt) * (dt - frame_time);

    return get();
}

void FPS_Counter::set_settle_time(Ns time) { settle_time = time; }
Ns FPS_Counter::get_settle_time() const { return settle_time; }

} // namespace floormat
