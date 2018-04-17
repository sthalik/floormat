#include "world-data.hpp"
#include "constants.hpp"

#include <QChar>

cell world_data::operator()(int x, int y) const
{
    if (((y+2)%5) == 0)
        return cell(u'~', color::from_rgb(0, 0, 255), color::from_rgb(0, 5, 50));
    else if ((((x%3)+x+y)%5) == 0)
        return cell(u'ł', color::from_rgb(0, 200, 50), color::from_rgb(10, 50, 5));
    else
        return cell(u'-', color::from_rgb(192, 192, 192), color::from_rgb(10, 50, 5));
}
