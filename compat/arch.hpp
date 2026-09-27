#pragma once

namespace floormat {

// Flushes denormals on the calling thread only. Windows starts each new thread in the default FP
// mode rather than its creator's, so a thread doing FP math calls this itself.
void set_fp_mask() noexcept;

} // namespace floormat
