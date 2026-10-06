#pragma once

namespace Bot
{
    // Driver has a synchronous, targeted message path; no input is held across
    // updates. Even a rejected/throwing keydown must get a keyup attempt.
    template<class Driver> bool AfkInputPulse(Driver& driver)
    {
        bool down=false;
        try { down=driver.Down(); }
        catch (...) { try { driver.Up(); } catch (...) {} return false; }
        bool up=false;
        try { up=driver.Up(); } catch (...) { return false; }
        return down && up;
    }
}
