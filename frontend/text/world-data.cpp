#include "world-data.hpp"
#include "constants.hpp"

#include <QChar>

cell world_data::operator()(int x, int y) const
{
    if (((y+x)%5) == 0)
        return cell(u'ł', color::from_rgb(0, 200, 50));
    else
        return cell(u'*', color::from_rgb(192, 192, 192));
}
