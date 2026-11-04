#include "SceneCanvas.h"

using namespace ric;

namespace
{
    constexpr float kToolStripWidth = 44.0f;

    juce::Path shapeIcon (Tool t, juce::Rectangle<float> r)
    {
        juce::Path p;
        const auto c = r.getCentre();
        const float s = r.getWidth() * 0.5f;
        auto ring = [&] (int n, float rad, float start, float inner = -1.0f)
        {
            const int count = inner > 0.0f ? n * 2 : n;
            for (int i = 0; i < count; ++i)
            {
                const float a = start + kTwoPi * (float) i / (float) count;
                const float rr = (inner > 0.0f && (i % 2)) ? inner : rad;
                const juce::Point<float> pt (c.x + std::cos (a) * rr, c.y + std::sin (a) * rr);
                if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
            }
            p.closeSubPath();
        };
        switch (t)
        {
            case Tool::Select:
                p.startNewSubPath (c.x - s * 0.5f, c.y - s * 0.8f);
                p.lineTo (c.x + s * 0.6f, c.y + s * 0.1f);
                p.lineTo (c.x + s * 0.05f, c.y + s * 0.2f);
                p.lineTo (c.x - s * 0.2f, c.y + s * 0.8f);
                p.closeSubPath();
                break;
            case Tool::Circle:   p.addEllipse (r.reduced (s * 0.2f)); break;
            case Tool::Rect:     p.addRectangle (r.reduced (s * 0.15f, s * 0.45f)); break;
            case Tool::Triangle: ring (3, s * 0.85f, -kPi * 0.5f); break;
            case Tool::Diamond:
                p.addQuadrilateral (c.x, c.y - s * 0.85f, c.x + s * 0.55f, c.y, c.x, c.y + s * 0.85f, c.x - s * 0.55f, c.y);
                break;
            case Tool::Polygon:  ring (6, s * 0.8f, -kPi * 0.5f); break;
            case Tool::Star:     ring (5, s * 0.9f, -kPi * 0.5f, s * 0.4f); break;
            case Tool::Line:
                p.startNewSubPath (c.x - s * 0.8f, c.y + s * 0.5f);
                p.lineTo (c.x + s * 0.8f, c.y - s * 0.5f);
                break;
            case Tool::Arc:
                p.addCentredArc (c.x, c.y + s * 0.3f, s * 0.75f, s * 0.75f, 0.0f, -1.2f, 1.2f, true);
                break;
            case Tool::Emitter:
                p.addEllipse (juce::Rectangle<float> (s * 0.6f, s * 0.6f).withCentre (c.translated (-s * 0.35f, 0)));
                p.startNewSubPath (c.x - s * 0.05f, c.y);
                p.lineTo (c.x + s * 0.85f, c.y);
                p.addTriangle (c.x + s * 0.85f, c.y - s * 0.3f, c.x + s * 0.85f, c.y + s * 0.3f, c.x + s * 1.05f, c.y);
                break;
            case Tool::NumTools:
            default: break;
        }
        return p;
    }

    const char* toolTip (Tool t)
    {
        switch (t)
        {
            case Tool::Select:   return "Select / move (V)";
            case Tool::Circle:   return "Add circle (1)";
            case Tool::Rect:     return "Add rectangle (2)";
            case Tool::Triangle: return "Add triangle (3)";
            case Tool::Diamond:  return "Add diamond (4)";
            case Tool::Polygon:  return "Add polygon (5)";
            case Tool::Star:     return "Add star (6)";
            case Tool::Line:     return "Add line (7)";
            case Tool::Arc:      return "Add arc (8)";
            case Tool::Emitter:  return "Add ball emitter (E)";
            case Tool::NumTools:
            default:             return "";
        }
    }

    Shape toolShape (Tool t) { return (Shape) ((int) t - 1); }
}

int stepNoteInScale (int note, int dir, const World& w)
{
    int n = note;
    for (int i = 0; i < 13; ++i)
    {
        n += dir > 0 ? 1 : -1;
        if (n < 0 || n > 127) return note;
        if (quantizeToScale (n, w.scale, w.root) == n) return n;
    }
    return note;
}

//==============================================================================
class SceneCanvas::ToolButton : public juce::Button
{
public:
    ToolButton (Tool t) : juce::Button (toolTip (t)), tool (t)
    {
        setTooltip (toolTip (t));
        setClickingTogglesState (false);
    }

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        auto r = getLocalBounds().toFloat().reduced (3.0f);
        const bool on = getToggleState();
        g.setColour (on ? Theme::accent.withAlpha (0.18f) : (over ? Theme::panelHi : Theme::panel));
        g.fillRoundedRectangle (r, 8.0f);
        if (on)
        {
            g.setColour (Theme::accent.withAlpha (0.8f));
            g.drawRoundedRectangle (r, 8.0f, 1.2f);
        }
        const auto icon = shapeIcon (tool, r.reduced (9.0f));
        const auto col = on ? Theme::accent : (down ? Theme::text : Theme::textDim.brighter (over ? 0.4f : 0.0f));
        g.setColour (col);
        if (tool == Tool::Line || tool == Tool::Arc)
            g.strokePath (icon, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        else if (tool == Tool::Select)
            g.fillPath (icon);
        else
        {
            g.setColour (col.withAlpha (0.2f));
            g.fillPath (icon);
            g.setColour (col);
            g.strokePath (icon, juce::PathStrokeType (1.6f));
        }
    }

    Tool tool;
};

//==============================================================================
SceneCanvas::SceneCanvas (RicochetProcessor& p, Selection& s)
    : processor (p), selection (s)
{
    for (int i = 0; i < (int) Tool::NumTools; ++i)
    {
        auto* b = toolButtons.add (new ToolButton ((Tool) i));
        b->onClick = [this, i] { setTool ((Tool) i); };
        addAndMakeVisible (b);
    }
    setTool (Tool::Select);
    setWantsKeyboardFocus (true);
    processor.addChangeListener (this);
    selection.addChangeListener (this);
    flashNote.fill (60);
    ripples.reserve (128);
    startTimerHz (60);
}

SceneCanvas::~SceneCanvas()
{
    processor.removeChangeListener (this);
    selection.removeChangeListener (this);
}

void SceneCanvas::setTool (Tool t)
{
    tool = t;
    for (auto* b : toolButtons)
        b->setToggleState (b->tool == t, juce::dontSendNotification);
    setMouseCursor (t == Tool::Select ? juce::MouseCursor::NormalCursor : juce::MouseCursor::CrosshairCursor);
}

void SceneCanvas::resized()
{
    auto r = getLocalBounds().toFloat();
    auto strip = r.removeFromLeft (kToolStripWidth);
    const float bh = juce::jmin (42.0f, strip.getHeight() / (float) Tool::NumTools);
    for (auto* b : toolButtons)
        b->setBounds (strip.removeFromTop (bh).toNearestInt());

    r = r.reduced (10.0f);
    r.removeFromBottom (24.0f);
    const float side = juce::jmin (r.getWidth(), r.getHeight());
    area = r.withSizeKeepingCentre (side, side);
}

juce::Point<float> SceneCanvas::toScreen (float x, float y) const noexcept
{
    return { area.getX() + x * area.getWidth(), area.getY() + y * area.getHeight() };
}

juce::Point<float> SceneCanvas::toWorld (juce::Point<float> s) const noexcept
{
    return { (s.x - area.getX()) / area.getWidth(), (s.y - area.getY()) / area.getHeight() };
}

void SceneCanvas::changeListenerCallback (juce::ChangeBroadcaster*)
{
    // Drop a selection that no longer exists (undo, preset change...)
    const auto& sc = processor.getScene();
    if (selection.isObstacle() && ! sc.obstacles[(size_t) selection.index].active) selection.clear();
    if (selection.isEmitter() && ! sc.emitters[(size_t) selection.index].active) selection.clear();
    repaint();
}

//==============================================================================
void SceneCanvas::timerCallback()
{
    ++frame;
    processor.copySnapshot (live);

    const int n = processor.popHits (hitScratch.data(), (int) hitScratch.size());
    for (int i = 0; i < n; ++i)
    {
        const auto& h = hitScratch[(size_t) i];
        if (juce::isPositiveAndBelow (h.obstacle, kMaxObstacles))
        {
            flash[(size_t) h.obstacle] = juce::jmin (1.0f, 0.35f + h.velocity);
            flashNote[(size_t) h.obstacle] = h.note;
        }
        const auto col = Theme::noteColour (h.note);
        if (ripples.size() < 120)
            ripples.push_back ({ h.x, h.y, 0.0f, h.velocity, col });
        ambient = ambient.interpolatedWith (col, 0.35f);
        ambientEnergy = juce::jmin (1.0f, ambientEnergy + 0.25f * h.velocity);
    }

    for (auto& f : flash) f *= 0.9f;
    for (auto& r : ripples) r.age += 1.0f / 60.0f;
    ripples.erase (std::remove_if (ripples.begin(), ripples.end(), [] (const Ripple& r) { return r.age > 0.9f; }), ripples.end());
    ambientEnergy *= 0.96f;

    // update trails
    for (int b = 0; b < live.numBalls; ++b)
    {
        const auto& bv = live.balls[(size_t) b];
        Trail* t = nullptr;
        for (auto& tr : trails) if (tr.id == bv.id && tr.lastFrame >= frame - 2) { t = &tr; break; }
        if (t == nullptr)
        {
            for (auto& tr : trails) if (tr.lastFrame < frame - 2) { t = &tr; break; }
            if (t == nullptr) continue;
            *t = Trail();
            t->id = bv.id;
        }
        t->lastFrame = frame;
        t->note = bv.lastNote;
        t->pts[(size_t) t->head] = { bv.x, bv.y };
        t->head = (t->head + 1) % (int) t->pts.size();
        t->count = juce::jmin (t->count + 1, (int) t->pts.size());
    }

    repaint();
}

//==============================================================================
juce::Path SceneCanvas::obstaclePath (int index, float rotation) const
{
    const auto& o = processor.getScene().obstacles[(size_t) index];
    CompiledObstacle c;
    buildOutline (o, rotation, c);
    juce::Path p;
    if (c.circle)
    {
        const auto ctr = toScreen (o.x, o.y);
        const float r = worldToScreen (o.size);
        p.addEllipse (ctr.x - r, ctr.y - r, r * 2.0f, r * 2.0f);
        return p;
    }
    for (int i = 0; i < c.numVerts; ++i)
    {
        const auto pt = toScreen (c.v[(size_t) i].x, c.v[(size_t) i].y);
        if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
    }
    if (c.closed) p.closeSubPath();
    return p;
}

int SceneCanvas::hitObstacle (juce::Point<float> w) const
{
    const auto& sc = processor.getScene();
    const auto screen = toScreen (w.x, w.y);
    for (int i = kMaxObstacles - 1; i >= 0; --i)
    {
        const auto& o = sc.obstacles[(size_t) i];
        if (! o.active) continue;
        const float dx = o.x - w.x, dy = o.y - w.y;
        if (dx * dx + dy * dy > (o.size + 0.02f) * (o.size + 0.02f)) continue;

        const auto path = obstaclePath (i, live.rotations[(size_t) i]);
        if (o.shape == Shape::Line || o.shape == Shape::Arc)
        {
            juce::Point<float> nearest;
            path.getNearestPoint (screen, nearest);
            if (nearest.getDistanceFrom (screen) < 8.0f) return i;
        }
        else if (path.contains (screen) || std::sqrt (dx * dx + dy * dy) < 0.012f)
        {
            return i;
        }
    }
    return -1;
}

int SceneCanvas::hitEmitter (juce::Point<float> w) const
{
    const auto& sc = processor.getScene();
    for (int i = kMaxEmitters - 1; i >= 0; --i)
    {
        const auto& e = sc.emitters[(size_t) i];
        if (! e.active) continue;
        if (juce::Point<float> (e.x, e.y).getDistanceFrom (w) < 0.03f) return i;
    }
    return -1;
}

juce::Point<float> SceneCanvas::resizeHandle (const Obstacle& o) const
{
    const float a = -kPi * 0.25f;
    return toScreen (o.x + std::cos (a) * o.size, o.y + std::sin (a) * o.size);
}

juce::Point<float> SceneCanvas::rotateHandle (const Obstacle& o, float rotation) const
{
    const float d = o.size + 0.04f;
    return toScreen (o.x + std::cos (rotation - kPi * 0.5f) * d, o.y + std::sin (rotation - kPi * 0.5f) * d);
}

juce::Point<float> SceneCanvas::aimHandle (const Emitter& e) const
{
    const float len = 0.035f + e.speed * 0.09f;
    return toScreen (e.x + std::cos (e.angle) * len, e.y + std::sin (e.angle) * len);
}

void SceneCanvas::editScene (const std::function<void (Scene&)>& fn, bool undo)
{
    auto s = processor.getScene();
    fn (s);
    processor.setScene (s, undo);
}

void SceneCanvas::addAt (juce::Point<float> w, Tool t)
{
    w.x = juce::jlimit (0.02f, 0.98f, w.x);
    w.y = juce::jlimit (0.02f, 0.98f, w.y);
    auto s = processor.getScene();

    if (t == Tool::Emitter)
    {
        Emitter e;
        e.x = w.x; e.y = w.y;
        e.angle = w.y < 0.5f ? kPi * 0.5f : -kPi * 0.5f;
        const int idx = s.addEmitter (e);
        if (idx >= 0)
        {
            processor.setScene (s, true);
            selection.select (Selection::Kind::Emitter, idx);
        }
        return;
    }

    Obstacle o;
    o.shape = toolShape (t);
    o.x = w.x; o.y = w.y;
    o.size = o.shape == Shape::Line ? 0.07f : 0.045f;
    o.aspect = o.shape == Shape::Rect ? 2.5f : (o.shape == Shape::Arc ? 0.4f : 1.0f);
    o.sides = o.shape == Shape::Star ? 5 : 6;
    o.engine = lastEngine;
    const int len = scaleLength (s.world.scale);
    o.note = scaleDegreeToNote (juce::roundToInt ((1.0f - w.y) * (float) len * 2.0f), s.world.scale, s.world.root, 3);
    const int idx = s.addObstacle (o);
    if (idx >= 0)
    {
        processor.setScene (s, true);
        selection.select (Selection::Kind::Obstacle, idx);
        processor.previewNote (o.engine, effectiveNote (s.world, o));
    }
}

void SceneCanvas::deleteSelection()
{
    if (selection.isObstacle())
        editScene ([i = selection.index] (Scene& s) { s.obstacles[(size_t) i] = Obstacle(); }, true);
    else if (selection.isEmitter())
        editScene ([i = selection.index] (Scene& s) { s.emitters[(size_t) i] = Emitter(); }, true);
    selection.clear();
}

void SceneCanvas::duplicateSelection()
{
    auto s = processor.getScene();
    if (selection.isObstacle())
    {
        auto o = s.obstacles[(size_t) selection.index];
        o.x = juce::jlimit (0.02f, 0.98f, o.x + 0.05f);
        o.y = juce::jlimit (0.02f, 0.98f, o.y + 0.05f);
        const int idx = s.addObstacle (o);
        if (idx >= 0) { processor.setScene (s, true); selection.select (Selection::Kind::Obstacle, idx); }
    }
    else if (selection.isEmitter())
    {
        auto e = s.emitters[(size_t) selection.index];
        e.x = juce::jlimit (0.02f, 0.98f, e.x + 0.05f);
        const int idx = s.addEmitter (e);
        if (idx >= 0) { processor.setScene (s, true); selection.select (Selection::Kind::Emitter, idx); }
    }
}

//==============================================================================
void SceneCanvas::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    const auto w = toWorld (e.position);
    drag = Drag::None;
    dragStartedEdit = false;

    if (e.mods.isPopupMenu())
    {
        showContextMenu (w);
        return;
    }

    const auto& sc = processor.getScene();

    // handles of the current selection first
    if (selection.isObstacle())
    {
        const auto& o = sc.obstacles[(size_t) selection.index];
        if (resizeHandle (o).getDistanceFrom (e.position) < 9.0f) { drag = Drag::Resize; processor.pushUndo(); return; }
        const float rot = live.rotations[(size_t) selection.index];
        if (rotateHandle (o, rot).getDistanceFrom (e.position) < 9.0f)
        {
            drag = Drag::Rotate;
            rotateOffset = o.rotation - std::atan2 (w.y - o.y, w.x - o.x);
            processor.pushUndo();
            return;
        }
    }
    if (selection.isEmitter())
    {
        const auto& em = sc.emitters[(size_t) selection.index];
        if (aimHandle (em).getDistanceFrom (e.position) < 10.0f) { drag = Drag::Aim; processor.pushUndo(); return; }
    }

    if (tool == Tool::Select || e.mods.isAltDown())
    {
        if (const int ei = hitEmitter (w); ei >= 0)
        {
            selection.select (Selection::Kind::Emitter, ei);
            drag = Drag::MoveEmitter;
            dragOffset = { sc.emitters[(size_t) ei].x - w.x, sc.emitters[(size_t) ei].y - w.y };
            return;
        }
        if (const int oi = hitObstacle (w); oi >= 0)
        {
            selection.select (Selection::Kind::Obstacle, oi);
            drag = Drag::Move;
            dragOffset = { sc.obstacles[(size_t) oi].x - w.x, sc.obstacles[(size_t) oi].y - w.y };
            lastEngine = sc.obstacles[(size_t) oi].engine;
            return;
        }
        selection.clear();
        return;
    }

    // creation tools: clicking an existing object still selects it
    if (const int oi = hitObstacle (w); oi >= 0)
    {
        selection.select (Selection::Kind::Obstacle, oi);
        drag = Drag::Move;
        dragOffset = { sc.obstacles[(size_t) oi].x - w.x, sc.obstacles[(size_t) oi].y - w.y };
        return;
    }
    if (w.x >= 0.0f && w.x <= 1.0f && w.y >= 0.0f && w.y <= 1.0f)
        addAt (w, tool);
}

void SceneCanvas::mouseDrag (const juce::MouseEvent& e)
{
    if (drag == Drag::None) return;
    const auto w = toWorld (e.position);
    const bool snap = e.mods.isShiftDown();
    auto snapv = [snap] (float v) { return snap ? std::round (v * 32.0f) / 32.0f : v; };

    if ((drag == Drag::Move || drag == Drag::MoveEmitter) && ! dragStartedEdit)
    {
        if (e.getDistanceFromDragStart() < 2) return;
        processor.pushUndo();
        dragStartedEdit = true;
    }

    const int i = selection.index;
    editScene ([&] (Scene& s)
    {
        switch (drag)
        {
            case Drag::Move:
            {
                auto& o = s.obstacles[(size_t) i];
                o.x = juce::jlimit (0.0f, 1.0f, snapv (w.x + dragOffset.x));
                o.y = juce::jlimit (0.0f, 1.0f, snapv (w.y + dragOffset.y));
                break;
            }
            case Drag::MoveEmitter:
            {
                auto& em = s.emitters[(size_t) i];
                em.x = juce::jlimit (0.0f, 1.0f, snapv (w.x + dragOffset.x));
                em.y = juce::jlimit (0.0f, 1.0f, snapv (w.y + dragOffset.y));
                break;
            }
            case Drag::Resize:
            {
                auto& o = s.obstacles[(size_t) i];
                o.size = juce::jlimit (0.012f, 0.35f, juce::Point<float> (o.x, o.y).getDistanceFrom (w));
                break;
            }
            case Drag::Rotate:
            {
                auto& o = s.obstacles[(size_t) i];
                float r = std::atan2 (w.y - o.y, w.x - o.x) + rotateOffset;
                if (snap) r = std::round (r / (kPi / 12.0f)) * (kPi / 12.0f);
                o.rotation = r;
                break;
            }
            case Drag::Aim:
            {
                auto& em = s.emitters[(size_t) i];
                float a = std::atan2 (w.y - em.y, w.x - em.x);
                if (snap) a = std::round (a / (kPi / 12.0f)) * (kPi / 12.0f);
                em.angle = a;
                const float len = juce::Point<float> (em.x, em.y).getDistanceFrom (w);
                em.speed = juce::jlimit (0.02f, 3.0f, (len - 0.035f) / 0.09f);
                break;
            }
            case Drag::None:
            default: break;
        }
    }, false);
}

void SceneCanvas::mouseUp (const juce::MouseEvent&)
{
    drag = Drag::None;
}

void SceneCanvas::mouseDoubleClick (const juce::MouseEvent& e)
{
    const auto w = toWorld (e.position);
    const auto& sc = processor.getScene();
    if (const int oi = hitObstacle (w); oi >= 0)
    {
        const auto& o = sc.obstacles[(size_t) oi];
        processor.previewNote (o.engine, effectiveNote (sc.world, o));
        flash[(size_t) oi] = 1.0f;
        flashNote[(size_t) oi] = effectiveNote (sc.world, o);
        return;
    }
    if (const int ei = hitEmitter (w); ei >= 0)
    {
        processor.fireEmitter (ei);
        return;
    }
    if (tool == Tool::Select)
        addAt (w, Tool::Circle);
}

void SceneCanvas::mouseMove (const juce::MouseEvent& e)
{
    hoverWorld = toWorld (e.position);
}

void SceneCanvas::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    const auto w = toWorld (e.position);
    const int dir = wheel.deltaY > 0 ? 1 : (wheel.deltaY < 0 ? -1 : 0);
    if (dir == 0) return;

    if (const int oi = hitObstacle (w); oi >= 0)
    {
        selection.select (Selection::Kind::Obstacle, oi);
        auto s = processor.getScene();
        auto& o = s.obstacles[(size_t) oi];
        if (e.mods.isShiftDown())
            o.size = juce::jlimit (0.012f, 0.35f, o.size * (dir > 0 ? 1.08f : 0.92f));
        else
            o.note = stepNoteInScale (o.note, dir, s.world);
        processor.setScene (s, false);
        if (! e.mods.isShiftDown())
            processor.previewNote (o.engine, effectiveNote (s.world, o));
        return;
    }
    if (const int ei = hitEmitter (w); ei >= 0)
    {
        selection.select (Selection::Kind::Emitter, ei);
        editScene ([ei, dir] (Scene& s)
        {
            auto& em = s.emitters[(size_t) ei];
            em.speed = juce::jlimit (0.02f, 3.0f, em.speed * (dir > 0 ? 1.1f : 0.9f));
        }, false);
    }
}

bool SceneCanvas::keyPressed (const juce::KeyPress& key)
{
    const auto code = key.getKeyCode();
    if (code == juce::KeyPress::deleteKey || code == juce::KeyPress::backspaceKey) { deleteSelection(); return true; }
    if (key.getModifiers().isCommandDown() && key.getTextCharacter() == 'd') { duplicateSelection(); return true; }
    if (key.getModifiers().isCommandDown()) return false;

    const auto ch = juce::CharacterFunctions::toLowerCase (key.getTextCharacter());
    if (ch == 'v') { setTool (Tool::Select); return true; }
    if (ch == 'e') { setTool (Tool::Emitter); return true; }
    if (ch >= '1' && ch <= '8') { setTool ((Tool) (ch - '0')); return true; }

    const float step = key.getModifiers().isShiftDown() ? 0.02f : 0.004f;
    float dx = 0, dy = 0;
    if (code == juce::KeyPress::leftKey) dx = -step;
    else if (code == juce::KeyPress::rightKey) dx = step;
    else if (code == juce::KeyPress::upKey) dy = -step;
    else if (code == juce::KeyPress::downKey) dy = step;
    if ((dx != 0 || dy != 0) && (selection.isObstacle() || selection.isEmitter()))
    {
        editScene ([&] (Scene& s)
        {
            if (selection.isObstacle())
            {
                auto& o = s.obstacles[(size_t) selection.index];
                o.x = juce::jlimit (0.0f, 1.0f, o.x + dx); o.y = juce::jlimit (0.0f, 1.0f, o.y + dy);
            }
            else
            {
                auto& em = s.emitters[(size_t) selection.index];
                em.x = juce::jlimit (0.0f, 1.0f, em.x + dx); em.y = juce::jlimit (0.0f, 1.0f, em.y + dy);
            }
        }, true);
        return true;
    }
    return false;
}

//==============================================================================
void SceneCanvas::showContextMenu (juce::Point<float> w)
{
    const auto& sc = processor.getScene();
    juce::PopupMenu menu;
    juce::Component::SafePointer<SceneCanvas> safe (this);

    if (const int oi = hitObstacle (w); oi >= 0)
    {
        selection.select (Selection::Kind::Obstacle, oi);
        const auto o = sc.obstacles[(size_t) oi];
        auto edit = [safe, oi] (std::function<void (Obstacle&)> fn)
        {
            return [safe, oi, fn]
            {
                if (safe == nullptr) return;
                safe->editScene ([oi, fn] (Scene& s) { fn (s.obstacles[(size_t) oi]); }, true);
            };
        };

        menu.addSectionHeader (shapeNames()[(int) o.shape] + "  -  " + noteName (effectiveNote (sc.world, o)));
        juce::PopupMenu shapes, engines;
        for (int i = 0; i < (int) Shape::NumShapes; ++i)
            shapes.addItem (shapeNames()[i], true, (int) o.shape == i, edit ([i] (Obstacle& ob) { ob.shape = (Shape) i; }));
        for (int i = 0; i < kNumEngines; ++i)
            engines.addItem (engineNames()[i], true, o.engine == i, edit ([i] (Obstacle& ob) { ob.engine = i; }));
        menu.addSubMenu ("Shape", shapes);
        menu.addSubMenu ("Sound", engines);
        const auto world = sc.world;
        menu.addItem ("Note up", edit ([world] (Obstacle& ob) { ob.note = stepNoteInScale (ob.note, 1, world); }));
        menu.addItem ("Note down", edit ([world] (Obstacle& ob) { ob.note = stepNoteInScale (ob.note, -1, world); }));
        menu.addItem ("Octave up", edit ([] (Obstacle& ob) { ob.note = juce::jmin (120, ob.note + 12); }));
        menu.addItem ("Octave down", edit ([] (Obstacle& ob) { ob.note = juce::jmax (12, ob.note - 12); }));
        menu.addItem ("Spin", true, o.spin != 0.0f, edit ([] (Obstacle& ob) { ob.spin = ob.spin != 0.0f ? 0.0f : 0.8f; }));
        menu.addItem ("Mute", true, o.muted, edit ([] (Obstacle& ob) { ob.muted = ! ob.muted; }));
        menu.addSeparator();
        menu.addItem ("Preview sound", [safe, o, world]
        {
            if (safe != nullptr) safe->processor.previewNote (o.engine, effectiveNote (world, o));
        });
        menu.addItem ("Duplicate", [safe] { if (safe != nullptr) safe->duplicateSelection(); });
        menu.addItem ("Delete", [safe] { if (safe != nullptr) safe->deleteSelection(); });
    }
    else if (const int ei = hitEmitter (w); ei >= 0)
    {
        selection.select (Selection::Kind::Emitter, ei);
        const auto em = sc.emitters[(size_t) ei];
        menu.addSectionHeader ("Emitter");
        juce::PopupMenu rates;
        for (int i = 0; i < (int) Rate::NumRates; ++i)
            rates.addItem (rateNames()[i], true, (int) em.rate == i, [safe, ei, i]
            {
                if (safe != nullptr)
                    safe->editScene ([ei, i] (Scene& s) { s.emitters[(size_t) ei].rate = (Rate) i; }, true);
            });
        menu.addSubMenu ("Rate", rates);
        menu.addItem ("Fire now", [safe, ei] { if (safe != nullptr) safe->processor.fireEmitter (ei); });
        menu.addItem ("Duplicate", [safe] { if (safe != nullptr) safe->duplicateSelection(); });
        menu.addItem ("Delete", [safe] { if (safe != nullptr) safe->deleteSelection(); });
    }
    else
    {
        juce::PopupMenu add;
        for (int t = (int) Tool::Circle; t <= (int) Tool::Emitter; ++t)
            add.addItem (toolTip ((Tool) t), [safe, w, t] { if (safe != nullptr) safe->addAt (w, (Tool) t); });
        menu.addSubMenu ("Add here", add);
        menu.addSeparator();
        menu.addItem ("Remove all obstacles", [safe]
        {
            if (safe != nullptr)
                safe->editScene ([] (Scene& s) { for (auto& o : s.obstacles) o = Obstacle(); }, true);
        });
        menu.addItem ("Remove all emitters", [safe]
        {
            if (safe != nullptr)
                safe->editScene ([] (Scene& s) { for (auto& e : s.emitters) e = Emitter(); }, true);
        });
    }

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this)
                            .withTargetScreenArea (juce::Rectangle<int> (1, 1).withPosition (localPointToGlobal (toScreen (w.x, w.y)).toInt())));
}

//==============================================================================
void SceneCanvas::paint (juce::Graphics& g)
{
    g.fillAll (Theme::bg);
    g.setColour (Theme::panel);
    g.fillRect (getLocalBounds().removeFromLeft ((int) kToolStripWidth));

    drawBackground (g);
    drawBoundary (g);
    drawObstacles (g);
    drawEmitters (g);
    drawBalls (g);
    drawSelection (g);
    drawHud (g);
}

void SceneCanvas::drawBackground (juce::Graphics& g)
{
    const auto c = area.getCentre();
    const auto tint = Theme::bg.interpolatedWith (ambient, 0.10f + 0.22f * ambientEnergy);
    juce::ColourGradient grad (tint, c.x, c.y, Theme::bg, area.getX(), area.getY(), true);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (area.expanded (6.0f), 14.0f);

    // dot grid
    g.setColour (Theme::outline.withAlpha (0.55f));
    const int n = 16;
    for (int i = 1; i < n; ++i)
        for (int j = 1; j < n; ++j)
        {
            const auto p = toScreen ((float) i / (float) n, (float) j / (float) n);
            g.fillEllipse (p.x - 0.9f, p.y - 0.9f, 1.8f, 1.8f);
        }

    // ripples
    for (const auto& r : ripples)
    {
        const auto p = toScreen (r.x, r.y);
        const float rad = worldToScreen (0.01f + r.age * 0.16f * (0.5f + r.strength));
        g.setColour (r.colour.withAlpha (juce::jmax (0.0f, 0.5f * (1.0f - r.age / 0.9f))));
        g.drawEllipse (p.x - rad, p.y - rad, rad * 2.0f, rad * 2.0f, 1.5f);
    }
}

void SceneCanvas::drawBoundary (juce::Graphics& g)
{
    juce::Path p;
    switch (processor.getScene().world.boundary)
    {
        case Boundary::Circle: p.addEllipse (area); break;
        case Boundary::Hexagon:
            for (int k = 0; k < 6; ++k)
            {
                const float a = kTwoPi * (float) k / 6.0f;
                const auto pt = toScreen (0.5f + 0.5f * std::cos (a), 0.5f + 0.5f * std::sin (a));
                if (k == 0) p.startNewSubPath (pt); else p.lineTo (pt);
            }
            p.closeSubPath();
            break;
        case Boundary::Open:
        {
            const float dash[] { 6.0f, 6.0f };
            juce::Path r; r.addRoundedRectangle (area, 6.0f);
            juce::PathStrokeType (1.0f).createDashedStroke (p, r, dash, 2);
            g.setColour (Theme::textDim.withAlpha (0.5f));
            g.fillPath (p);
            return;
        }
        case Boundary::Box:
        case Boundary::NumBoundaries:
        default: p.addRoundedRectangle (area, 6.0f); break;
    }
    const auto col = Theme::accent.interpolatedWith (ambient, 0.6f);
    g.setColour (col.withAlpha (0.08f + 0.12f * ambientEnergy));
    g.strokePath (p, juce::PathStrokeType (8.0f));
    g.setColour (col.withAlpha (0.6f));
    g.strokePath (p, juce::PathStrokeType (1.5f));
}

void SceneCanvas::drawObstacles (juce::Graphics& g)
{
    const auto& sc = processor.getScene();
    for (int i = 0; i < kMaxObstacles; ++i)
    {
        const auto& o = sc.obstacles[(size_t) i];
        if (! o.active) continue;

        const float rot = o.spin != 0.0f ? live.rotations[(size_t) i] : o.rotation;
        const auto path = obstaclePath (i, rot);
        const int note = effectiveNote (sc.world, o);
        const float f = flash[(size_t) i];
        const auto col = o.muted ? Theme::textDim : Theme::noteColour (f > 0.05f ? flashNote[(size_t) i] : note);
        const bool open = o.shape == Shape::Line || o.shape == Shape::Arc;

        if (f > 0.02f)
        {
            g.setColour (col.withAlpha (0.18f * f));
            g.strokePath (path, juce::PathStrokeType (14.0f * f + 4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        if (! open)
        {
            juce::ColourGradient grad (col.withAlpha (0.35f + 0.5f * f), toScreen (o.x, o.y - o.size),
                                       col.darker (0.6f).withAlpha (0.15f + 0.4f * f), toScreen (o.x, o.y + o.size), false);
            g.setGradientFill (grad);
            g.fillPath (path);
        }
        g.setColour (col.brighter (0.3f * f).withAlpha (o.muted ? 0.4f : 0.95f));
        g.strokePath (path, juce::PathStrokeType (open ? 3.0f + 2.0f * f : 1.6f + 1.5f * f,
                                                  juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const float rpx = worldToScreen (o.size);
        if (rpx > 16.0f && ! open)
        {
            g.setColour (Theme::text.withAlpha (0.55f + 0.45f * f));
            g.setFont (Theme::font (juce::jlimit (9.0f, 14.0f, rpx * 0.4f), true));
            const auto c = toScreen (o.x, o.y);
            g.drawText (noteName (note), juce::Rectangle<float> (60.0f, 16.0f).withCentre (c), juce::Justification::centred);
        }
    }
}

void SceneCanvas::drawEmitters (juce::Graphics& g)
{
    const auto& sc = processor.getScene();
    for (int i = 0; i < kMaxEmitters; ++i)
    {
        const auto& e = sc.emitters[(size_t) i];
        if (! e.active) continue;
        const auto c = toScreen (e.x, e.y);
        const auto tip = aimHandle (e);
        const bool sel = selection.isEmitter() && selection.index == i;
        const auto col = sel ? Theme::accent : Theme::text;

        // spread cone
        if (e.spread > 0.01f)
        {
            juce::Path cone;
            const float len = c.getDistanceFrom (tip) * 1.4f;
            cone.startNewSubPath (c);
            cone.lineTo (c.x + std::cos (e.angle - e.spread) * len, c.y + std::sin (e.angle - e.spread) * len);
            cone.lineTo (c.x + std::cos (e.angle + e.spread) * len, c.y + std::sin (e.angle + e.spread) * len);
            cone.closeSubPath();
            g.setColour (col.withAlpha (0.07f));
            g.fillPath (cone);
        }

        g.setColour (col.withAlpha (0.15f));
        g.fillEllipse (juce::Rectangle<float> (26.0f, 26.0f).withCentre (c));
        g.setColour (col.withAlpha (0.9f));
        g.drawEllipse (juce::Rectangle<float> (16.0f, 16.0f).withCentre (c), 1.6f);
        g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre (c));

        juce::Path arrow;
        arrow.startNewSubPath (c.getPointOnCircumference (9.0f, e.angle + kPi * 0.5f));
        arrow.lineTo (tip);
        g.strokePath (arrow, juce::PathStrokeType (1.6f));
        juce::Path head;
        head.addTriangle (0.0f, -5.0f, 0.0f, 5.0f, 9.0f, 0.0f);
        g.fillPath (head, juce::AffineTransform::rotation (e.angle).translated (tip));

        g.setFont (Theme::font (10.0f));
        g.setColour (Theme::textDim);
        g.drawText (rateNames()[(int) e.rate], juce::Rectangle<float> (60.0f, 12.0f).withCentre (c.translated (0.0f, 20.0f)),
                    juce::Justification::centred);
    }
}

void SceneCanvas::drawBalls (juce::Graphics& g)
{
    for (const auto& t : trails)
    {
        if (t.lastFrame < frame - 2 || t.count < 2) continue;
        const auto col = t.note >= 0 ? Theme::noteColour (t.note) : Theme::text;
        juce::Point<float> prev;
        for (int k = 0; k < t.count; ++k)
        {
            const int idx = (t.head - t.count + k + (int) t.pts.size() * 2) % (int) t.pts.size();
            const auto p = toScreen (t.pts[(size_t) idx].x, t.pts[(size_t) idx].y);
            if (k > 0 && p.getDistanceFrom (prev) < area.getWidth() * 0.2f)
            {
                const float a = (float) k / (float) t.count;
                g.setColour (col.withAlpha (0.5f * a));
                g.drawLine ({ prev, p }, 1.0f + 3.0f * a);
            }
            prev = p;
        }
    }

    for (int b = 0; b < live.numBalls; ++b)
    {
        const auto& bv = live.balls[(size_t) b];
        const auto p = toScreen (bv.x, bv.y);
        const float r = juce::jmax (2.5f, worldToScreen (bv.radius));
        const auto col = bv.lastNote >= 0 ? Theme::noteColour (bv.lastNote) : Theme::text;
        g.setColour (col.withAlpha (0.22f));
        g.fillEllipse (juce::Rectangle<float> (r * 4.0f, r * 4.0f).withCentre (p));
        g.setColour (col);
        g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (p));
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.fillEllipse (juce::Rectangle<float> (r, r).withCentre (p.translated (-r * 0.25f, -r * 0.25f)));
    }
}

void SceneCanvas::drawSelection (juce::Graphics& g)
{
    const auto& sc = processor.getScene();
    if (selection.isObstacle())
    {
        const auto& o = sc.obstacles[(size_t) selection.index];
        const auto c = toScreen (o.x, o.y);
        const float r = worldToScreen (o.size) + 6.0f;
        juce::Path ring, dashed;
        ring.addEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
        const float dash[] { 4.0f, 4.0f };
        juce::PathStrokeType (1.0f).createDashedStroke (dashed, ring, dash, 2);
        g.setColour (Theme::accent.withAlpha (0.8f));
        g.fillPath (dashed);

        const float rot = o.spin != 0.0f ? live.rotations[(size_t) selection.index] : o.rotation;
        const auto rh = rotateHandle (o, rot);
        const auto sh = resizeHandle (o);
        g.drawLine ({ c, rh }, 1.0f);
        g.fillEllipse (juce::Rectangle<float> (10.0f, 10.0f).withCentre (rh));
        g.setColour (Theme::bg);
        g.fillEllipse (juce::Rectangle<float> (4.0f, 4.0f).withCentre (rh));
        g.setColour (Theme::accent);
        g.fillRect (juce::Rectangle<float> (9.0f, 9.0f).withCentre (sh));
    }
    else if (selection.isEmitter())
    {
        const auto& e = sc.emitters[(size_t) selection.index];
        const auto tip = aimHandle (e);
        g.setColour (Theme::accent);
        g.drawEllipse (juce::Rectangle<float> (14.0f, 14.0f).withCentre (tip), 1.5f);
    }

    // creation preview
    if (tool != Tool::Select && hoverWorld.x >= 0.0f && hoverWorld.x <= 1.0f && hoverWorld.y >= 0.0f && hoverWorld.y <= 1.0f
        && isMouseOver())
    {
        const auto p = toScreen (hoverWorld.x, hoverWorld.y);
        const auto icon = shapeIcon (tool, juce::Rectangle<float> (worldToScreen (0.09f), worldToScreen (0.09f)).withCentre (p));
        g.setColour (Theme::text.withAlpha (0.25f));
        g.strokePath (icon, juce::PathStrokeType (1.2f));
    }
}

void SceneCanvas::drawHud (juce::Graphics& g)
{
    const auto& sc = processor.getScene();
    const int bar = (int) std::floor (live.beat / 4.0) + 1;
    const int beatInBar = (int) std::floor (std::fmod (juce::jmax (0.0, live.beat), 4.0)) + 1;
    const juce::String hud = juce::String (bar) + "." + juce::String (beatInBar)
                           + "   " + juce::String (live.bpm, 1) + " BPM"
                           + "   balls " + juce::String (live.numBalls)
                           + "   voices " + juce::String (live.voices)
                           + "   " + rootNames()[sc.world.root] + " " + scaleNames()[sc.world.scale];
    g.setFont (Theme::font (11.0f));
    g.setColour (Theme::textDim);
    g.drawText (hud, area.toNearestInt().removeFromBottom (18).translated (6, 18), juce::Justification::centredLeft);

    // beat pulse dots
    for (int i = 0; i < 4; ++i)
    {
        const bool on = i == beatInBar - 1;
        g.setColour (on ? ambient : Theme::outline);
        g.fillEllipse (area.getRight() - 52.0f + (float) i * 12.0f, area.getBottom() + 6.0f, 7.0f, 7.0f);
    }
}
