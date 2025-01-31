#pragma once

#include <libwidget/Element.h>

namespace Widget
{

class PanelElement : public Element
{
private:
    int _border_radius = 0;

public:
    void border_radius(int value) { _border_radius = value; }

    PanelElement(int radius = 0);

    void paint(Graphic::Painter &painter, const Math::Recti &) override;
};

static inline RefPtr<PanelElement> panel(int radius = 0)
{
    return make<PanelElement>(radius);
}

template <typename TElement>
static inline RefPtr<PanelElement> panel(RefPtr<TElement> child)
{
    auto panel = make<PanelElement>();
    panel->add(child);
    return panel;
}

template <typename TElement>
static inline RefPtr<PanelElement> panel(int radius, RefPtr<TElement> child)
{
    auto panel = make<PanelElement>(radius);
    panel->add(child);
    return panel;
}

} // namespace Widget
