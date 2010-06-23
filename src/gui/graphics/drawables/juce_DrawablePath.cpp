/*
  ==============================================================================

   This file is part of the JUCE library - "Jules' Utility Class Extensions"
   Copyright 2004-10 by Raw Material Software Ltd.

  ------------------------------------------------------------------------------

   JUCE can be redistributed and/or modified under the terms of the GNU General
   Public License (Version 2), as published by the Free Software Foundation.
   A copy of the license is included in the JUCE distribution, or can be found
   online at www.gnu.org/licenses.

   JUCE is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
   A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

  ------------------------------------------------------------------------------

   To release a closed-source product which uses JUCE, commercial licenses are
   available: visit www.rawmaterialsoftware.com/juce for more information.

  ==============================================================================
*/

#include "../../../core/juce_StandardHeader.h"

BEGIN_JUCE_NAMESPACE

#include "juce_DrawablePath.h"
#include "juce_DrawableComposite.h"
#include "../../../io/streams/juce_MemoryOutputStream.h"


//==============================================================================
DrawablePath::DrawablePath()
    : mainFill (Colours::black),
      strokeFill (Colours::black),
      strokeType (0.0f),
      pathNeedsUpdating (true),
      strokeNeedsUpdating (true)
{
}

DrawablePath::DrawablePath (const DrawablePath& other)
    : mainFill (other.mainFill),
      strokeFill (other.strokeFill),
      strokeType (other.strokeType),
      pathNeedsUpdating (true),
      strokeNeedsUpdating (true)
{
    if (other.relativePath != 0)
        relativePath = new RelativePointPath (*other.relativePath);
    else
        path = other.path;
}

DrawablePath::~DrawablePath()
{
}

//==============================================================================
void DrawablePath::setPath (const Path& newPath)
{
    path = newPath;
    strokeNeedsUpdating = true;
}

void DrawablePath::setFill (const FillType& newFill)
{
    mainFill = newFill;
}

void DrawablePath::setStrokeFill (const FillType& newFill)
{
    strokeFill = newFill;
}

void DrawablePath::setStrokeType (const PathStrokeType& newStrokeType)
{
    strokeType = newStrokeType;
    strokeNeedsUpdating = true;
}

void DrawablePath::setStrokeThickness (const float newThickness)
{
    setStrokeType (PathStrokeType (newThickness, strokeType.getJointStyle(), strokeType.getEndStyle()));
}

void DrawablePath::updatePath() const
{
    if (pathNeedsUpdating)
    {
        pathNeedsUpdating = false;

        if (relativePath != 0)
        {
            path.clear();
            relativePath->createPath (path, parent);
            strokeNeedsUpdating = true;
        }
    }
}

void DrawablePath::updateStroke() const
{
    if (strokeNeedsUpdating)
    {
        strokeNeedsUpdating = false;
        updatePath();
        stroke.clear();
        strokeType.createStrokedPath (stroke, path, AffineTransform::identity, 4.0f);
    }
}

const Path& DrawablePath::getPath() const
{
    updatePath();
    return path;
}

const Path& DrawablePath::getStrokePath() const
{
    updateStroke();
    return stroke;
}

bool DrawablePath::isStrokeVisible() const throw()
{
    return strokeType.getStrokeThickness() > 0.0f && ! strokeFill.isInvisible();
}

void DrawablePath::invalidatePoints()
{
    pathNeedsUpdating = true;
    strokeNeedsUpdating = true;
}

//==============================================================================
void DrawablePath::render (const Drawable::RenderingContext& context) const
{
    {
        FillType f (mainFill);
        if (f.isGradient())
            f.gradient->multiplyOpacity (context.opacity);

        f.transform = f.transform.followedBy (context.transform);
        context.g.setFillType (f);
        context.g.fillPath (getPath(), context.transform);
    }

    if (isStrokeVisible())
    {
        FillType f (strokeFill);
        if (f.isGradient())
            f.gradient->multiplyOpacity (context.opacity);

        f.transform = f.transform.followedBy (context.transform);
        context.g.setFillType (f);
        context.g.fillPath (getStrokePath(), context.transform);
    }
}

const Rectangle<float> DrawablePath::getBounds() const
{
    if (isStrokeVisible())
        return getStrokePath().getBounds();
    else
        return getPath().getBounds();
}

bool DrawablePath::hitTest (float x, float y) const
{
    return getPath().contains (x, y)
            || (isStrokeVisible() && getStrokePath().contains (x, y));
}

Drawable* DrawablePath::createCopy() const
{
    return new DrawablePath (*this);
}

//==============================================================================
const Identifier DrawablePath::valueTreeType ("Path");

const Identifier DrawablePath::ValueTreeWrapper::fill ("Fill");
const Identifier DrawablePath::ValueTreeWrapper::stroke ("Stroke");
const Identifier DrawablePath::ValueTreeWrapper::path ("Path");
const Identifier DrawablePath::ValueTreeWrapper::jointStyle ("jointStyle");
const Identifier DrawablePath::ValueTreeWrapper::capStyle ("capStyle");
const Identifier DrawablePath::ValueTreeWrapper::strokeWidth ("strokeWidth");
const Identifier DrawablePath::ValueTreeWrapper::nonZeroWinding ("nonZeroWinding");
const Identifier DrawablePath::ValueTreeWrapper::point1 ("p1");
const Identifier DrawablePath::ValueTreeWrapper::point2 ("p2");
const Identifier DrawablePath::ValueTreeWrapper::point3 ("p3");

//==============================================================================
DrawablePath::ValueTreeWrapper::ValueTreeWrapper (const ValueTree& state_)
    : ValueTreeWrapperBase (state_)
{
    jassert (state.hasType (valueTreeType));
}

ValueTree DrawablePath::ValueTreeWrapper::getPathState()
{
    return state.getOrCreateChildWithName (path, 0);
}

ValueTree DrawablePath::ValueTreeWrapper::getMainFillState()
{
    ValueTree v (state.getChildWithName (fill));
    if (v.isValid())
        return v;

    setMainFill (Colours::black, 0, 0, 0, 0, 0);
    return getMainFillState();
}

ValueTree DrawablePath::ValueTreeWrapper::getStrokeFillState()
{
    ValueTree v (state.getChildWithName (stroke));
    if (v.isValid())
        return v;

    setStrokeFill (Colours::black, 0, 0, 0, 0, 0);
    return getStrokeFillState();
}

const FillType DrawablePath::ValueTreeWrapper::getMainFill (RelativeCoordinate::NamedCoordinateFinder* nameFinder,
                                                            ImageProvider* imageProvider) const
{
    return readFillType (state.getChildWithName (fill), 0, 0, 0, nameFinder, imageProvider);
}

void DrawablePath::ValueTreeWrapper::setMainFill (const FillType& newFill, const RelativePoint* gp1,
                                                  const RelativePoint* gp2, const RelativePoint* gp3,
                                                  ImageProvider* imageProvider, UndoManager* undoManager)
{
    ValueTree v (state.getOrCreateChildWithName (fill, undoManager));
    writeFillType (v, newFill, gp1, gp2, gp3, imageProvider, undoManager);
}

const FillType DrawablePath::ValueTreeWrapper::getStrokeFill (RelativeCoordinate::NamedCoordinateFinder* nameFinder,
                                                              ImageProvider* imageProvider) const
{
    return readFillType (state.getChildWithName (stroke), 0, 0, 0, nameFinder, imageProvider);
}

void DrawablePath::ValueTreeWrapper::setStrokeFill (const FillType& newFill, const RelativePoint* gp1,
                                                    const RelativePoint* gp2, const RelativePoint* gp3,
                                                    ImageProvider* imageProvider, UndoManager* undoManager)
{
    ValueTree v (state.getOrCreateChildWithName (stroke, undoManager));
    writeFillType (v, newFill, gp1, gp2, gp3, imageProvider, undoManager);
}

const PathStrokeType DrawablePath::ValueTreeWrapper::getStrokeType() const
{
    const String jointStyleString (state [jointStyle].toString());
    const String capStyleString (state [capStyle].toString());

    return PathStrokeType (state [strokeWidth],
                           jointStyleString == "curved" ? PathStrokeType::curved
                                                        : (jointStyleString == "bevel" ? PathStrokeType::beveled
                                                                                       : PathStrokeType::mitered),
                           capStyleString == "square" ? PathStrokeType::square
                                                      : (capStyleString == "round" ? PathStrokeType::rounded
                                                                                   : PathStrokeType::butt));
}

void DrawablePath::ValueTreeWrapper::setStrokeType (const PathStrokeType& newStrokeType, UndoManager* undoManager)
{
    state.setProperty (strokeWidth, (double) newStrokeType.getStrokeThickness(), undoManager);
    state.setProperty (jointStyle, newStrokeType.getJointStyle() == PathStrokeType::mitered
                                     ? "miter" : (newStrokeType.getJointStyle() == PathStrokeType::curved ? "curved" : "bevel"), undoManager);
    state.setProperty (capStyle, newStrokeType.getEndStyle() == PathStrokeType::butt
                                   ? "butt" : (newStrokeType.getEndStyle() == PathStrokeType::square ? "square" : "round"), undoManager);
}

bool DrawablePath::ValueTreeWrapper::usesNonZeroWinding() const
{
    return state [nonZeroWinding];
}

void DrawablePath::ValueTreeWrapper::setUsesNonZeroWinding (bool b, UndoManager* undoManager)
{
    state.setProperty (nonZeroWinding, b, undoManager);
}

//==============================================================================
const Identifier DrawablePath::ValueTreeWrapper::Element::mode ("mode");
const Identifier DrawablePath::ValueTreeWrapper::Element::startSubPathElement ("Move");
const Identifier DrawablePath::ValueTreeWrapper::Element::closeSubPathElement ("Close");
const Identifier DrawablePath::ValueTreeWrapper::Element::lineToElement ("Line");
const Identifier DrawablePath::ValueTreeWrapper::Element::quadraticToElement ("Quad");
const Identifier DrawablePath::ValueTreeWrapper::Element::cubicToElement ("Cubic");

DrawablePath::ValueTreeWrapper::Element::Element (const ValueTree& state_)
    : state (state_)
{
}

DrawablePath::ValueTreeWrapper::Element::~Element()
{
}

DrawablePath::ValueTreeWrapper DrawablePath::ValueTreeWrapper::Element::getParent() const
{
    return ValueTreeWrapper (state.getParent().getParent());
}

DrawablePath::ValueTreeWrapper::Element DrawablePath::ValueTreeWrapper::Element::getPreviousElement() const
{
    return Element (state.getSibling (-1));
}

int DrawablePath::ValueTreeWrapper::Element::getNumControlPoints() const throw()
{
    const Identifier i (state.getType());
    if (i == startSubPathElement || i == lineToElement) return 1;
    if (i == quadraticToElement) return 2;
    if (i == cubicToElement) return 3;
    return 0;
}

const RelativePoint DrawablePath::ValueTreeWrapper::Element::getControlPoint (const int index) const
{
    jassert (index >= 0 && index < getNumControlPoints());
    return RelativePoint (state [index == 0 ? point1 : (index == 1 ? point2 : point3)].toString());
}

Value DrawablePath::ValueTreeWrapper::Element::getControlPointValue (int index, UndoManager* undoManager) const
{
    jassert (index >= 0 && index < getNumControlPoints());
    return state.getPropertyAsValue (index == 0 ? point1 : (index == 1 ? point2 : point3), undoManager);
}

void DrawablePath::ValueTreeWrapper::Element::setControlPoint (const int index, const RelativePoint& point, UndoManager* undoManager)
{
    jassert (index >= 0 && index < getNumControlPoints());
    return state.setProperty (index == 0 ? point1 : (index == 1 ? point2 : point3), point.toString(), undoManager);
}

const RelativePoint DrawablePath::ValueTreeWrapper::Element::getStartPoint() const
{
    const Identifier i (state.getType());

    if (i == startSubPathElement)
        return getControlPoint (0);

    jassert (i == lineToElement || i == quadraticToElement || i == cubicToElement || i == closeSubPathElement);

    return getPreviousElement().getEndPoint();
}

const RelativePoint DrawablePath::ValueTreeWrapper::Element::getEndPoint() const
{
    const Identifier i (state.getType());
    if (i == startSubPathElement || i == lineToElement)  return getControlPoint (0);
    if (i == quadraticToElement)                         return getControlPoint (1);
    if (i == cubicToElement)                             return getControlPoint (2);

    jassert (i == closeSubPathElement);
    return RelativePoint();
}

const String DrawablePath::ValueTreeWrapper::Element::getModeOfEndPoint() const
{
    return state [mode].toString();
}

void DrawablePath::ValueTreeWrapper::Element::setModeOfEndPoint (const String& newMode, UndoManager* undoManager)
{
    if (state.hasType (cubicToElement))
        state.setProperty (mode, newMode, undoManager);
}

void DrawablePath::ValueTreeWrapper::Element::convertToLine (UndoManager* undoManager)
{
    const Identifier i (state.getType());

    if (i == quadraticToElement || i == cubicToElement)
    {
        ValueTree newState (lineToElement);
        Element e (newState);
        e.setControlPoint (0, getEndPoint(), undoManager);
        state = newState;
    }
}

void DrawablePath::ValueTreeWrapper::Element::convertToCubic (RelativeCoordinate::NamedCoordinateFinder* nameFinder, UndoManager* undoManager)
{
    const Identifier i (state.getType());

    if (i == lineToElement || i == quadraticToElement)
    {
        ValueTree newState (cubicToElement);
        Element e (newState);

        const RelativePoint start (getStartPoint());
        const RelativePoint end (getEndPoint());
        const Point<float> startResolved (start.resolve (nameFinder));
        const Point<float> endResolved (end.resolve (nameFinder));
        e.setControlPoint (0, startResolved + (endResolved - startResolved) * 0.3f, undoManager);
        e.setControlPoint (1, startResolved + (endResolved - startResolved) * 0.7f, undoManager);
        e.setControlPoint (2, end, undoManager);

        state = newState;
    }
}

void DrawablePath::ValueTreeWrapper::Element::convertToPathBreak (UndoManager* undoManager)
{
    const Identifier i (state.getType());

    if (i != startSubPathElement)
    {
        ValueTree newState (startSubPathElement);
        Element e (newState);
        e.setControlPoint (0, getEndPoint(), undoManager);
        state = newState;
    }
}

void DrawablePath::ValueTreeWrapper::Element::insertPoint (double, RelativeCoordinate::NamedCoordinateFinder*, UndoManager*)
{
}

void DrawablePath::ValueTreeWrapper::Element::removePoint (UndoManager* undoManager)
{
    state.getParent().removeChild (state, undoManager);
}

//==============================================================================
const Rectangle<float> DrawablePath::refreshFromValueTree (const ValueTree& tree, ImageProvider* imageProvider)
{
    Rectangle<float> damageRect;
    ValueTreeWrapper v (tree);
    setName (v.getID());

    bool needsRedraw = false;
    const FillType newFill (v.getMainFill (parent, imageProvider));

    if (mainFill != newFill)
    {
        needsRedraw = true;
        mainFill = newFill;
    }

    const FillType newStrokeFill (v.getStrokeFill (parent, imageProvider));

    if (strokeFill != newStrokeFill)
    {
        needsRedraw = true;
        strokeFill = newStrokeFill;
    }

    const PathStrokeType newStroke (v.getStrokeType());

    ScopedPointer<RelativePointPath> newRelativePath (new RelativePointPath (tree));

    Path newPath;
    newRelativePath->createPath (newPath, parent);

    if (! newRelativePath->containsAnyDynamicPoints())
        newRelativePath = 0;

    if (strokeType != newStroke || path != newPath)
    {
        damageRect = getBounds();
        path.swapWithPath (newPath);
        strokeNeedsUpdating = true;
        strokeType = newStroke;
        needsRedraw = true;
    }

    relativePath = newRelativePath;

    if (needsRedraw)
        damageRect = damageRect.getUnion (getBounds());

    return damageRect;
}

const ValueTree DrawablePath::createValueTree (ImageProvider* imageProvider) const
{
    ValueTree tree (valueTreeType);
    ValueTreeWrapper v (tree);

    v.setID (getName(), 0);
    v.setMainFill (mainFill, 0, 0, 0, imageProvider, 0);
    v.setStrokeFill (strokeFill, 0, 0, 0, imageProvider, 0);
    v.setStrokeType (strokeType, 0);

    if (relativePath != 0)
    {
        relativePath->writeTo (tree, 0);
    }
    else
    {
        RelativePointPath rp (path);
        rp.writeTo (tree, 0);
    }

    return tree;
}


END_JUCE_NAMESPACE
