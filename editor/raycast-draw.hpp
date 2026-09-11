#pragma once

namespace floormat::rc { struct raycast_result_s; struct raycast_diag_s; }

namespace floormat {

struct app;

// raycast_test draws one ray, scene_raycast a wheel of 32, and both want the same picture.
// dot_size scales the endpoint marker; 32 dots at the test's size cover the field they sit on.
void draw_raycast_line(app& a, const rc::raycast_result_s& r, float dot_size = 1);

// One ray's DDA cells and RTree queries. Never more than one -- raycast_with_diag() reuses
// the arrays per call.
void draw_raycast_diag(app& a, const rc::raycast_diag_s& diag);

} // namespace floormat
