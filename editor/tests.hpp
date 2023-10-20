#pragma once
#include "compat/defs.hpp"

namespace floormat {

struct tests_data;

struct tests_data_
{
    fm_DECLARE_DELETED_COPY_ASSIGNMENT(tests_data_);

    virtual ~tests_data_() noexcept;
    static Pointer<tests_data_> make();

protected:
    tests_data_();
};

} // namespace floormat
