#include <algorithm>
#include <trux/layout/region.hpp>

using namespace trux;

layout::Split layout::Region::v_split(int percent) const {
    percent = std::clamp(percent, 0, 100);

    int first_width = size().width * percent / 100;

    return {
        Region{position(), {first_width, size().height}},
        Region{
               {position().x + first_width, position().y},
               {size().width - first_width, size().height},
               },
    };
}

layout::Split layout::Region::h_split(int percent) const {
    percent = std::clamp(percent, 0, 100);

    int first_height = size().height * percent / 100;

    return {
        Region{position(), {size().width, first_height}},
        Region{
               {
                position().x,
                position().y + first_height,
            }, {
                size().width,
                size().height - first_height,
            }, },
    };
}
