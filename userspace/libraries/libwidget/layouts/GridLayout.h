#pragma once

#include <libwidget/Element.h>

namespace Widget
{

class GridLayout : public Element
{
private:
    int _hcells;
    int _vcells;
    int _hspacing;
    int _vspacing;

public:
    GridLayout(int hcells, int vcells, int hspacing, int vspacing)
        : _hcells{hcells},
          _vcells{vcells},
          _hspacing{hspacing},
          _vspacing{vspacing}
    {
    }

    Math::Vec2i size() override
    {
        if (childs().count() == 0)
        {
            return Math::Vec2i(0);
        }

        int width = 0;
        int height = 0;

        for (auto &child : childs())
        {
            Math::Vec2i child_size = child->compute_size();

            width = MAX(width, child_size.x());
            height = MAX(height, child_size.y());
        }

        width = width * _hcells;
        height = height * _vcells;

        width += _hspacing * _hcells;
        height += _vspacing * _vcells;

        return {width, height};
    }

    void layout() override
    {
        int index = 0;

        for (auto &child : childs())
        {
            int x = index % _hcells;
            int y = index / _hcells;

            Math::Recti row = bound().row(_vcells, y, _vspacing);
            Math::Recti column = row.column(_hcells, x, _hspacing);

            child->container(column);
            index++;
        }
    }
};

static inline RefPtr<Element> grid(int hcells, int vcells, int hspacing, int vspacing, Vector<RefPtr<Element>> childs)
{
    auto layout = make<GridLayout>(hcells, vcells, hspacing, vspacing);
    layout->add(childs);
    return layout;
}

} // namespace Widget
