#include <libwidget/Application.h>

#include "neko/graphics/Animations.h"
#include "neko/model/Neko.h"
#include "neko/states/Sleep.h"
#include "neko/states/Surprised.h"
#include "neko/states/Yawning.h"

namespace neko
{

Yawning::Yawning()
    : _last_mouse_position(Widget::Application::the()->mouse_position())
{
}

void Yawning::update(Neko &neko)
{
    auto new_mouse_position = Widget::Application::the()->mouse_position();

    if (_last_mouse_position.distance_to(new_mouse_position) > 16)
    {
        neko.behavior(own<Surprised>());
    }
    else if (neko.tick() > 4)
    {
        neko.behavior(own<Sleep>());
    }
    else
    {
        _last_mouse_position = new_mouse_position;
        neko.did_update();
    }
}

Animation Yawning::animation(Neko &)
{
    return Animations::AKUBI;
}

} // namespace neko
