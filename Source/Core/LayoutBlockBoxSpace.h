/*
 * This source file is part of RmlUi, the HTML/CSS Interface Middleware
 *
 * For the latest information, see http://github.com/mikke89/RmlUi
 *
 * Copyright (c) 2008-2010 CodePoint Ltd, Shift Technology Ltd
 * Copyright (c) 2019 The RmlUi Team, and contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#ifndef RMLUI_CORE_LAYOUTBLOCKBOXSPACE_H
#define RMLUI_CORE_LAYOUTBLOCKBOXSPACE_H

#include "../../Include/RmlUi/Core/StyleTypes.h"
#include "../../Include/RmlUi/Core/Types.h"

namespace Rml {

class Element;
class BlockContainer;

enum class LayoutFloatBoxEdge { Border, Margin };

/**
    Each block box has a space object for managing the space occupied by its floating elements, and those of its
    ancestors as relevant.

    @author Peter Curry
 */

class LayoutBlockBoxSpace {
public:
	LayoutBlockBoxSpace();
	~LayoutBlockBoxSpace();

	/// Imports boxes from another block into this space.
	/// @param[in] space The space to import boxes from.
	void ImportSpace(const LayoutBlockBoxSpace& space);

	/// Generates the position for a box of a given size within our block box.
	/// @param[out] box_width The available width for the box.
	/// @param[in] cursor The ideal vertical position for the box.
	/// @param[in] dimensions The minimum available space required for the box.
	/// @param[in] nowrap Restrict from wrapping down, returned vertical position always placed at ideal cursor.
	/// @return The generated position for the box.
	Vector2f NextBoxPosition(const BlockContainer* parent, float& box_width, float cursor, Vector2f dimensions, bool nowrap) const;

	/// Determines the position of a floated element within our block box.
	/// @param[out] box_width The available width for the box.
	/// @param[in] cursor The ideal vertical position for the box.
	/// @param[in] dimensions The floated element's margin size.
	/// @param[in] float_property The element's computed float property.
	/// @param[in] clear_property The element's computed clear property.
	/// @return The next placement position for the float at its top-left margin position.
	Vector2f NextFloatPosition(const BlockContainer* parent, float& box_width, float cursor, Vector2f dimensions, Style::Float float_property,
		Style::Clear clear_property) const;

	/// Generates and sets the position for a floating box of a given size within our block box. The element's box
	/// is then added into our list of floating boxes.
	/// @param[in] element The element to position.
	/// @param[in] cursor The ideal vertical position for the box.
	/// @return The offset of the bottom outer edge of the element.
	float PlaceFloat(const BlockContainer* parent, Element* element, float cursor);

	/// Determines the appropriate vertical position for an object that is choosing to clear floating elements to
	/// the left or right (or both).
	/// @param[in] cursor The ideal vertical position.
	/// @param[in] clear_property The value of the clear property of the clearing object.
	/// @return The appropriate vertical position for the clearing object.
	float DetermineClearPosition(float cursor, Style::Clear clear_property) const;

	/// Returns the size of the rectangle encompassing all boxes within the space, relative to the parent's content box.
	/// @param[in] edges Which edge of the boxes to encompass.
	/// @note Generally, the border box is used when determining overflow, while the margin box is used for layout sizing.
	Vector2f GetDimensions(LayoutFloatBoxEdge edge) const;

	// TODO: This will clear everything, for all boxes in the current block formatting context!
	void Reset()
	{
		for (auto& box_list : boxes)
			box_list.clear();
		extent_bottom_right_border = {};
		extent_bottom_right_border = {};
		extent_bottom_right_margin = {};
	}

	void* operator new(size_t size);
	void operator delete(void* chunk, size_t size);

private:
	enum AnchorEdge { LEFT = 0, RIGHT = 1, NUM_ANCHOR_EDGES = 2 };

	// Generates the position for an arbitrary box within our space layout, floated against either the left or right edge.
	Vector2f NextBoxPosition(const BlockContainer* parent, float& maximum_box_width, float cursor, Vector2f dimensions, bool nowrap,
		Style::Float float_property) const;

	struct SpaceBox {
		Vector2f offset;
		Vector2f dimensions;
	};

	using SpaceBoxList = Vector<SpaceBox>;

	// The boxes floating in our space.
	SpaceBoxList boxes[NUM_ANCHOR_EDGES];

	// The rectangle encompassing all boxes added specifically into this space, relative to our parent's content box.
	Vector2f extent_top_left_border;
	Vector2f extent_bottom_right_border;
	Vector2f extent_bottom_right_margin;
};

} // namespace Rml
#endif
