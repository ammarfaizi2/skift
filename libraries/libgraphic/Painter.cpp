#include <math.h>
#include <stdlib.h>

#include <libgraphic/Font.h>
#include <libgraphic/Painter.h>
#include <libgraphic/StackBlur.h>
#include <libutils/Assert.h>
#include <libutils/Random.h>

namespace Graphic
{

Painter::Painter(RefPtr<Bitmap> bitmap)
{
    _bitmap = bitmap;
    _state_stack_top = 0;
    _state_stack[0] = {
        Math::Vec2i::zero(),
        bitmap->bound(),
    };
}

/* --- Context -------------------------------------------------------------- */

void Painter::push()
{
    Assert::lower_than(_state_stack_top, STATESTACK_SIZE);

    _state_stack_top++;
    _state_stack[_state_stack_top] = _state_stack[_state_stack_top - 1];
}

void Painter::pop()
{
    Assert::greater_than(_state_stack_top, 0);
    _state_stack_top--;
}

void Painter::clip(Recti rectangle)
{
    Recti transformed_rectangle = rectangle.offset(origin());
    Recti clipped_rectangle = transformed_rectangle.clipped_with(clip());

    _state_stack[_state_stack_top].clip = clipped_rectangle;
}

void Painter::transform(Math::Vec2i offset)
{
    _state_stack[_state_stack_top].origin += offset;
}

Recti Painter::apply_clip(Recti rectangle)
{
    if (rectangle.colide_with(clip()))
    {
        rectangle = rectangle.clipped_with(clip());
        rectangle = rectangle.clipped_with(_bitmap->bound());

        return rectangle;
    }
    else
    {
        return Recti::empty();
    }
}

Recti Painter::apply_transform(Recti rectangle)
{
    return rectangle.offset(_state_stack[_state_stack_top].origin);
}

Recti Painter::apply(Recti rectangle)
{
    Recti result = apply_transform(rectangle);
    result = apply_clip(result);

    return result;
}

SourceDestionation Painter::apply(Recti source, Recti destination)
{
    if (destination.is_empty())
    {
        return {Recti::empty(), Recti::empty()};
    }

    Recti transformed_destination = apply_transform(destination);
    Recti clipped_destination = apply_clip(transformed_destination);

    if (clipped_destination.is_empty())
    {
        return {Recti::empty(), Recti::empty()};
    }

    double scalex = clipped_destination.width() / (double)destination.width();
    double scaley = clipped_destination.height() / (double)destination.height();

    double other_scalex = source.width() / (double)destination.width();
    double other_scaley = source.height() / (double)destination.height();

    Recti clipped_source = source.scaled(scalex, scaley);
    clipped_source = clipped_source.offset((clipped_destination.position() - transformed_destination.position()) * Math::Vec2d{other_scalex, other_scaley});

    return {clipped_source, clipped_destination};
}

/* --- Drawing -------------------------------------------------------------- */

void Painter::plot(Math::Vec2i position, Color color)
{
    Math::Vec2i transformed = position + _state_stack[_state_stack_top].origin;

    if (clip().contains(transformed))
    {
        _bitmap->blend_pixel(transformed, color);
    }
}

void Painter::blit_fast(Bitmap &bitmap, Recti source, Recti destination)
{
    auto result = apply(source, destination);

    if (result.is_empty())
    {
        return;
    }

    for (int y = 0; y < result.destination.height(); y++)
    {
        for (int x = 0; x < result.destination.width(); x++)
        {
            Math::Vec2i position(x, y);

            Color sample = bitmap.get_pixel(result.source.position() + position);
            _bitmap->blend_pixel(result.destination.position() + position, sample);
        }
    }
}

void Painter::blit_scaled(Bitmap &bitmap, Recti source, Recti destination)
{
    auto result = apply(source, destination);

    if (result.is_empty())
    {
        return;
    }

    for (int y = 0; y < result.destination.height(); y++)
    {
        for (int x = 0; x < result.destination.width(); x++)
        {
            float xx = x / (float)result.destination.width();
            float yy = y / (float)result.destination.height();

            Color sample = bitmap.sample(result.source, Math::Vec2f(xx, yy));
            _bitmap->blend_pixel(result.destination.position() + Math::Vec2i(x, y), sample);
        }
    }
}

FLATTEN void Painter::blit(Bitmap &bitmap, Recti source, Recti destination)
{
    if (source.width() == destination.width() &&
        source.height() == destination.height())
    {
        blit_fast(bitmap, source, destination);
    }
    else
    {
        blit_scaled(bitmap, source, destination);
    }
}

void Painter::blit(Bitmap &bitmap, BitmapScaling scaling, Recti destionation)
{
    if (scaling == BitmapScaling::COVER)
    {
        blit(bitmap, bitmap.bound(), bitmap.bound().cover(destionation));
    }
    else if (scaling == BitmapScaling::STRETCH)
    {
        blit(bitmap, bitmap.bound(), destionation);
    }
    else if (scaling == BitmapScaling::CENTER)
    {
        blit(bitmap, bitmap.bound(), bitmap.bound().centered_within(destionation));
    }
    else if (scaling == BitmapScaling::FIT)
    {
        blit(bitmap, bitmap.bound(), bitmap.bound().fit(destionation));
    }
    else
    {
        ASSERT_NOT_REACHED();
    }
}

FLATTEN void Painter::clear(Color color)
{
    clear(_bitmap->bound(), color);
}

FLATTEN void Painter::clear(Recti rectangle, Color color)
{
    rectangle = apply(rectangle);

    if (rectangle.is_empty())
    {
        return;
    }

    for (int y = 0; y < rectangle.height(); y++)
    {
        for (int x = 0; x < rectangle.width(); x++)
        {
            _bitmap->set_pixel_no_check(Math::Vec2i(rectangle.x() + x, rectangle.y() + y), color);
        }
    }
}

FLATTEN void Painter::fill_rectangle(Recti rectangle, Color color)
{
    rectangle = apply(rectangle);

    if (rectangle.is_empty())
    {
        return;
    }

    for (int y = 0; y < rectangle.height(); y++)
    {
        for (int x = 0; x < rectangle.width(); x++)
        {
            _bitmap->blend_pixel_no_check(Math::Vec2i(rectangle.x() + x, rectangle.y() + y), color);
        }
    }
}

FLATTEN void Painter::fill_insets(Recti rectangle, Insetsi insets, Color color)
{
    Recti left_ear = rectangle.take_left(insets.left());
    Recti right_ear = rectangle.take_right(insets.right());

    Recti top = rectangle.cutoff_left_and_right(insets.left(), insets.right()).take_top(insets.top());
    Recti bottom = rectangle.cutoff_left_and_right(insets.left(), insets.right()).take_bottom(insets.bottom());

    fill_rectangle(left_ear, color);
    fill_rectangle(right_ear, color);
    fill_rectangle(top, color);
    fill_rectangle(bottom, color);
}

static void fill_circle_helper(Painter &painter, Recti bound, Math::Vec2i center, int radius, Color color)
{
    auto circle_distance = [](Math::Vec2i center, float radius, Math::Vec2i position) {
        float distance = center.distance_to(position) - radius;
        return clamp(0.5 - distance, 0, 1);
    };

    for (int y = 0; y < bound.height(); y++)
    {
        for (int x = 0; x < bound.width(); x++)
        {
            Math::Vec2i position = Math::Vec2i(x, y);

            float distance = circle_distance(center, radius - 0.5, Math::Vec2i(x, y));
            float alpha = color.alphaf() * distance;

            painter.plot(bound.position() + position, color.with_alpha(alpha));
        }
    }
}

static void blit_circle_helper(Painter &painter, Bitmap &bitmap, Recti source, Recti destination, Math::Vec2i center, int radius)
{
    if (!painter.clip().colide_with(destination))
    {
        return;
    }

    auto circle_distance = [](Math::Vec2i center, float radius, Math::Vec2i position) {
        float distance = center.distance_to(position) - radius;
        return clamp(0.5 - distance, 0, 1);
    };

    for (int y = 0; y < destination.height(); y++)
    {
        for (int x = 0; x < destination.width(); x++)
        {
            Math::Vec2i position = Math::Vec2i(x, y);

            float distance = circle_distance(center, radius - 0.5, Math::Vec2i(x, y));

            float xx = x / (float)destination.width();
            float yy = y / (float)destination.height();

            Color color = bitmap.sample(source, {xx, yy});
            double alpha = color.alphaf() * distance;

            painter.plot(destination.position() + position, color.with_alpha(alpha));
        }
    }
}

FLATTEN void Painter::blit_rounded(Bitmap &bitmap, Recti source, Recti destination, int radius)
{
    radius = MIN(radius, destination.height() / 2);
    radius = MIN(radius, destination.width() / 2);

    Recti left_ear_source = source.take_left(radius);
    Recti right_ear_source = source.take_right(radius);

    Recti left_ear_destination = destination.take_left(radius);
    Recti right_ear_destination = destination.take_right(radius);

    blit_circle_helper(*this, bitmap, source.take_top_left(radius), destination.take_top_left(radius), Math::Vec2i(radius - 1, radius - 1), radius);
    blit_circle_helper(*this, bitmap, source.take_bottom_left(radius), destination.take_bottom_left(radius), Math::Vec2i(radius - 1, 0), radius);
    blit(bitmap, left_ear_source.cutoff_top_and_botton(radius, radius), left_ear_destination.cutoff_top_and_botton(radius, radius));

    blit_circle_helper(*this, bitmap, source.take_top_right(radius), destination.take_top_right(radius), Math::Vec2i(0, radius - 1), radius);
    blit_circle_helper(*this, bitmap, source.take_bottom_right(radius), destination.take_bottom_right(radius), Math::Vec2i::zero(), radius);
    blit(bitmap, right_ear_source.cutoff_top_and_botton(radius, radius), right_ear_destination.cutoff_top_and_botton(radius, radius));

    blit(bitmap, source.cutoff_left_and_right(radius, radius), destination.cutoff_left_and_right(radius, radius));
}

FLATTEN void Painter::fill_rectangle_rounded(Recti bound, int radius, Color color)
{
    radius = MIN(radius, bound.height() / 2);
    radius = MIN(radius, bound.width() / 2);

    Recti left_ear = bound.take_left(radius);
    Recti right_ear = bound.take_right(radius);

    fill_circle_helper(*this, left_ear.take_top(radius), Math::Vec2i(radius - 1, radius - 1), radius, color);
    fill_circle_helper(*this, left_ear.take_bottom(radius), Math::Vec2i(radius - 1, 0), radius, color);
    fill_rectangle(left_ear.cutoff_top_and_botton(radius, radius), color);

    fill_circle_helper(*this, right_ear.take_top(radius), Math::Vec2i(0, radius - 1), radius, color);
    fill_circle_helper(*this, right_ear.take_bottom(radius), Math::Vec2i::zero(), radius, color);
    fill_rectangle(right_ear.cutoff_top_and_botton(radius, radius), color);

    fill_rectangle(bound.cutoff_left_and_right(radius, radius), color);
}

FLATTEN void Painter::fill_checkboard(Recti bound, int cell_size, Color fg_color, Color bg_color)
{
    for (int y = 0; y < bound.height(); y++)
    {
        for (int x = 0; x < bound.width(); x++)
        {
            Math::Vec2i position = bound.position() + Math::Vec2i(x, y);

            if (((y / cell_size) * bound.width() + (x / cell_size) + (y / cell_size) % 2) % 2 == 0)
            {
                plot(position, fg_color);
            }
            else
            {
                plot(position, bg_color);
            }
        }
    }
}

#define __ipart(X) ((int)(X))
#define __round(X) ((int)(((double)(X)) + 0.5))
#define __fpart(X) (((double)(X)) - (double)__ipart(X))
#define __rfpart(X) (1.0 - __fpart(X))
#define __plot(__x, __y, __brightness) plot(Math::Vec2i((__x), (__y)), color.with_alpha(color.alphaf() * (__brightness)))

FLATTEN void Painter::draw_line(Math::Vec2i a, Math::Vec2i b, Color color)
{
    if (a.x() == b.x())
    {
        for (int i = MIN(a.y(), b.y()); i <= MAX(a.y(), b.y()); i++)
        {
            plot(Math::Vec2i(a.x(), i), color);
        }

        return;
    }

    if (a.y() == b.y())
    {
        for (int i = MIN(a.x(), b.x()); i <= MAX(a.x(), b.x()); i++)
        {
            plot(Math::Vec2i(i, a.y()), color);
        }

        return;
    }

    double x0 = a.x();
    double y0 = a.y();
    double x1 = b.x();
    double y1 = b.y();

    const bool steep = fabs(y1 - a.y()) > fabs(x1 - x0);
    if (steep)
    {
        swap(x0, y0);
        swap(x1, y1);
    }
    if (x0 > x1)
    {
        swap(x0, x1);
        swap(y0, y1);
    }

    const float dx = x1 - x0;
    const float dy = y1 - y0;
    const float gradient = (dx == 0) ? 1 : dy / dx;

    int xpx11;
    float intery;
    {
        const float xend = __round(x0);
        const float yend = y0 + gradient * (xend - x0);
        const float xgap = __rfpart(x0 + 0.5);
        xpx11 = (int)xend;
        const int ypx11 = __ipart(yend);
        if (steep)
        {
            __plot(ypx11, xpx11, __rfpart(yend) * xgap);
            __plot(ypx11 + 1, xpx11, __fpart(yend) * xgap);
        }
        else
        {
            __plot(xpx11, ypx11, __rfpart(yend) * xgap);
            __plot(xpx11, ypx11 + 1, __fpart(yend) * xgap);
        }
        intery = yend + gradient;
    }

    int xpx12;
    {
        const float xend = __round(x1);
        const float yend = y1 + gradient * (xend - x1);
        const float xgap = __rfpart(x1 + 0.5);
        xpx12 = (int)xend;
        const int ypx12 = __ipart(yend);
        if (steep)
        {
            __plot(ypx12, xpx12, __rfpart(yend) * xgap);
            __plot(ypx12 + 1, xpx12, __fpart(yend) * xgap);
        }
        else
        {
            __plot(xpx12, ypx12, __rfpart(yend) * xgap);
            __plot(xpx12, ypx12 + 1, __fpart(yend) * xgap);
        }
    }

    if (steep)
    {
        for (int x = xpx11 + 1; x < xpx12; x++)
        {
            __plot(__ipart(intery), x, __rfpart(intery));
            __plot(__ipart(intery) + 1, x, __fpart(intery));
            intery += gradient;
        }
    }
    else
    {
        for (int x = xpx11 + 1; x < xpx12; x++)
        {
            __plot(x, __ipart(intery), __rfpart(intery));
            __plot(x, __ipart(intery) + 1, __fpart(intery));
            intery += gradient;
        }
    }
}

FLATTEN void Painter::draw_rectangle(Recti rect, Color color)
{
    Math::Vec2i topleft = rect.position();
    Math::Vec2i topright = rect.position() + rect.size().extract_x() - Math::Vec2i::oneX();
    Math::Vec2i bottomleft = rect.position() + rect.size().extract_y() - Math::Vec2i::oneY();
    Math::Vec2i bottomright = rect.position() + rect.size() - Math::Vec2i::one();

    draw_line(topleft, topright, color);
    draw_line(bottomleft, bottomright, color);

    draw_line(topleft + Math::Vec2i::oneY(), bottomleft - Math::Vec2i::oneY(), color);
    draw_line(topright + Math::Vec2i::oneY(), bottomright - Math::Vec2i::oneY(), color);
}

FLATTEN void Painter::draw_triangle(Math::Vec2i p0, Math::Vec2i p1, Math::Vec2i p2, Color color)
{
    draw_line(p0, p1, color);
    draw_line(p1, p2, color);
    draw_line(p2, p0, color);
}

static double sample_draw_circle(Math::Vec2i center, double radius, double thickness, Math::Vec2i position)
{
    auto distance = fabs(center.distance_to(position) - (radius - thickness / 2));

    return clamp((0.5 - distance) + (thickness / 2), 0, 1);
}

void Painter::draw_circle_helper(Recti bound, Math::Vec2i center, int radius, int thickness, Color color)
{
    for (int y = 0; y < bound.height(); y++)
    {
        for (int x = 0; x < bound.width(); x++)
        {
            Math::Vec2i position = Math::Vec2i(x, y);

            double distance = sample_draw_circle(center, radius - 0.5, thickness, position);
            double alpha = color.alphaf() * distance;

            plot(bound.position() + position, color.with_alpha(alpha));
        }
    }
}

FLATTEN void Painter::draw_rectangle_rounded(Recti bound, int radius, int thickness, Color color)
{
    radius = MIN(radius, bound.height() / 2);
    radius = MIN(radius, bound.width() / 2);

    Recti left_ear = bound.take_left(radius);
    Recti right_ear = bound.take_right(radius);

    draw_circle_helper(left_ear.take_top(radius), Math::Vec2i(radius - 1, radius - 1), radius, thickness, color);
    draw_circle_helper(left_ear.take_bottom(radius), Math::Vec2i(radius - 1, 0), radius, thickness, color);

    draw_circle_helper(right_ear.take_top(radius), Math::Vec2i(0, radius - 1), radius, thickness, color);
    draw_circle_helper(right_ear.take_bottom(radius), Math::Vec2i::zero(), radius, thickness, color);

    fill_rectangle(left_ear.cutoff_top_and_botton(radius, radius).take_left(thickness), color);
    fill_rectangle(right_ear.cutoff_top_and_botton(radius, radius).take_right(thickness), color);

    fill_rectangle(bound.cutoff_left_and_right(radius, radius).take_top(thickness), color);
    fill_rectangle(bound.cutoff_left_and_right(radius, radius).take_bottom(thickness), color);
}

FLATTEN void Painter::blit(Icon &icon, IconSize size, Recti destination, Color color)
{
    Bitmap &bitmap = *icon.bitmap(size);

    for (int y = 0; y < destination.height(); y++)
    {
        for (int x = 0; x < destination.width(); x++)
        {
            Math::Vec2f sample_point(
                x / (double)destination.width(),
                y / (double)destination.height());

            Color sample = bitmap.sample(sample_point);

            auto alpha = sample.alphaf() * color.alphaf();
            plot(destination.position() + Math::Vec2i(x, y), color.with_alpha(alpha));
        }
    }
}

FLATTEN void Painter::blit_colored(Bitmap &bitmap, Recti source, Recti destination, Color color)
{
    for (int y = 0; y < destination.height(); y++)
    {
        for (int x = 0; x < destination.width(); x++)
        {
            Math::Vec2f sample_point(
                x / (double)destination.width(),
                y / (double)destination.height());

            Color sample = bitmap.sample(source, sample_point);

            auto alpha = sample.redf() * color.alphaf();
            plot(destination.position() + Math::Vec2i(x, y), color.with_alpha(alpha));
        }
    }
}

void Painter::draw_glyph(Font &font, const Glyph &glyph, Math::Vec2i position, Color color)
{
    Recti dest(position - glyph.origin, glyph.bound.size());

    // auto metrics = font.metrics();
    // auto baseline = position.y();
    //
    // auto draw_metric = [&](int value, Color color) {
    //     draw_line({position.x(), value}, {position.x() + glyph.bound.width(), value}, color);
    // };
    //
    //  draw_metric(metrics.fullascend(baseline), Colors::RED);
    //  draw_metric(metrics.ascend(baseline), Colors::ORANGE);
    //  draw_metric(metrics.captop(baseline), Colors::BLUE);
    //  draw_metric(baseline, Colors::MAGENTA);
    //  draw_metric(metrics.descend(baseline), Colors::ORANGE);
    //  draw_metric(metrics.fulldescend(baseline), Colors::RED);

    blit_colored(font.bitmap(), glyph.bound, dest, color);
}

FLATTEN void Painter::draw_string(Font &font, const char *str, Math::Vec2i position, Color color)
{
    codepoint_foreach(reinterpret_cast<const uint8_t *>(str), [&](auto codepoint) {
        auto &glyph = font.glyph(codepoint);
        draw_glyph(font, glyph, position, color);
        position = position + Math::Vec2i(glyph.advance, 0);
    });
}

void Painter::draw_string_within(Font &font, const char *str, Recti container, Anchor anchor, Color color)
{
    Recti bound = font.mesure(str);

    bound = bound.place_within(container, anchor);

    draw_string(font, str, Math::Vec2i(bound.x(), bound.y() + bound.height() / 2 + 4), color);
}

/* --- Effects -------------------------------------------------------------- */

FLATTEN void Painter::blur(Recti rectangle, int radius)
{
    rectangle = apply(rectangle);

    stackblur((unsigned char *)_bitmap->pixels(),
              _bitmap->width(),
              _bitmap->height(),
              radius,
              rectangle.x(), rectangle.x() + rectangle.width(),
              rectangle.y(), rectangle.y() + rectangle.height());
}

FLATTEN void Painter::saturation(Recti rectangle, float value)
{
    rectangle = apply(rectangle);

    for (int y = 0; y < rectangle.height(); y++)
    {
        for (int x = 0; x < rectangle.width(); x++)
        {
            Color color = _bitmap->get_pixel({rectangle.x() + x, rectangle.y() + y});

            // weights from CCIR 601 spec
            // https://stackoverflow.com/questions/13806483/increase-or-decrease-color-saturation
            double gray = 0.2989 * color.red() + 0.5870 * color.green() + 0.1140 * color.blue();

            uint8_t red = (uint8_t)clamp(-gray * value + color.red() * (1 + value), 0, 255);
            uint8_t green = (uint8_t)clamp(-gray * value + color.green() * (1 + value), 0, 255);
            uint8_t blue = (uint8_t)clamp(-gray * value + color.blue() * (1 + value), 0, 255);

            color = Color::from_rgb_byte(red, green, blue);

            _bitmap->set_pixel({rectangle.x() + x, rectangle.y() + y}, color);
        }
    }
}

FLATTEN void Painter::noise(Recti rectangle, float opacity)
{
    Random rand{0x12341234};

    for (int y = 0; y < rectangle.height(); y++)
    {
        for (int x = 0; x < rectangle.width(); x++)
        {
            double noise = rand.next_double();
            plot({rectangle.x() + x, rectangle.y() + y}, Color::from_rgba(noise, noise, noise, opacity));
        }
    }
}

FLATTEN void Painter::sepia(Recti rectangle, float value)
{
    rectangle = apply(rectangle);

    for (int y = 0; y < rectangle.height(); y++)
    {
        for (int x = 0; x < rectangle.width(); x++)
        {
            Color color = _bitmap->get_pixel({rectangle.x() + x, rectangle.y() + y});

            uint32_t red = (color.red() * 0.393) + (color.green() * 0.769) + (color.blue() * 0.189);
            uint32_t green = (color.red() * 0.349) + (color.green() * 0.686) + (color.blue() * 0.168);
            uint32_t blue = (color.red() * 0.272) + (color.green() * 0.534) + (color.blue() * 0.131);

            Color sepia_color = Color::from_rgb_byte(MIN(255, red), MIN(255, green), MIN(blue, 255));

            _bitmap->set_pixel({rectangle.x() + x, rectangle.y() + y}, Color::lerp(color, sepia_color, value));
        }
    }
}

FLATTEN void Painter::tint(Recti rectangle, Color color)
{
    rectangle = apply(rectangle);

    for (int y = 0; y < rectangle.height(); y++)
    {
        for (int x = 0; x < rectangle.width(); x++)
        {
            Color sample = _bitmap->get_pixel({rectangle.x() + x, rectangle.y() + y});

            uint32_t red = (sample.red() * color.redf());
            uint32_t green = (sample.green() * color.greenf());
            uint32_t blue = (sample.blue() * color.bluef());

            Color sepia_color = Color::from_rgb_byte(MIN(255, red), MIN(255, green), MIN(blue, 255));
            _bitmap->set_pixel({rectangle.x() + x, rectangle.y() + y}, sepia_color);
        }
    }
}

void Painter::acrylic(Recti rectangle)
{
    saturation(rectangle, 0.25);
    blur(rectangle, 16);
    noise(rectangle, 0.05);
}

} // namespace Graphic
