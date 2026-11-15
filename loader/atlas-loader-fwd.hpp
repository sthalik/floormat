#pragma once

namespace floormat::detail::loader {

template<typename ATLAS> struct atlas_loader_traits;
template<typename ATLAS, typename TRAITS = atlas_loader_traits<ATLAS>> class atlas_loader;
template<typename ATLAS, typename TRAITS = atlas_loader_traits<ATLAS>> struct atlas_storage;

} // namespace floormat::detail::loader
