#pragma once
#include "compat/defs.hpp"

namespace floormat::tests { struct base_test; struct tests_data; }

namespace floormat {

template<typename T> class safe_ptr;

struct tests_data_
{
    fm_DECLARE_DELETED_COPY_ASSIGNMENT(tests_data_);

    virtual ~tests_data_() noexcept;
    [[nodiscard]] static safe_ptr<tests_data_> make();

protected:
    tests_data_();
};

} // namespace floormat
