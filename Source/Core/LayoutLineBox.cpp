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

#include "LayoutLineBox.h"
#include "../../Include/RmlUi/Core/ComputedValues.h"
#include "../../Include/RmlUi/Core/ElementText.h"
#include "../../Include/RmlUi/Core/ElementUtilities.h"
#include "../../Include/RmlUi/Core/Profiling.h"
#include "../../Include/RmlUi/Core/Property.h"
#include "LayoutBlockBox.h"
#include "LayoutEngine.h"
#include "LayoutInlineBoxText.h"
#include "LayoutInlineContainer.h"
#include <stack>

namespace Rml {

LayoutLineBox::~LayoutLineBox() {}

bool LayoutLineBox::AddBox(InlineLevelBox* box, bool wrap_content, float line_width, LayoutOverflowHandle& inout_overflow_handle)
{
	// TODO: The spacing this element must leave on the right of the line, to account not only for its margins and padding,
	// but also for its parents which will close immediately after it.
	// (Right edge width of all open fragments)
	float right_spacing_width = 0.f;
	ForAllOpenFragments(
		[&right_spacing_width](PlacedFragment& open_fragment) { right_spacing_width += open_fragment.inline_box->GetOuterSpacing(Box::RIGHT); });

	// TODO See old LayoutLineBox::AddBox
	const bool first_box = fragments.empty();
	float available_width = FLT_MAX;
	if (wrap_content)
		// TODO: Subtract floats (or perhaps in passed-in line_width).
		available_width = Math::RoundUpFloat(line_width - box_cursor);

	LayoutFragment fragment = box->LayoutContent(first_box, available_width, right_spacing_width, inout_overflow_handle);

	inout_overflow_handle = {};
	bool box_fully_placed = true;

	if (fragment)
	{
		RMLUI_ASSERT(fragment.layout_bounds.y >= 0.f);

		if (fragment.overflow_handle)
		{
			box_fully_placed = false;
			inout_overflow_handle = fragment.overflow_handle;
		}

		// TODO: Split case.
		if (fragment.layout_bounds.x < 0.f)
		{
			// Opening up an inline box.
			box_cursor += box->GetOuterSpacing(Box::LEFT);
			const Vector2f fragment_position = {box_cursor, 0.f};
			fragments.push_back(PlacedFragment{box, fragment_position, fragment.layout_bounds, open_fragment_index, std::move(fragment)});
			open_fragment_index = (FragmentIndex)fragments.size() - 1;
		}
		else
		{
			// Closed, fixed-size fragment.
			const Vector2f fragment_position = {box_cursor, 0.f};
			box_cursor += fragment.layout_bounds.x;
			fragments.push_back(PlacedFragment{box, fragment_position, fragment.layout_bounds, InvalidIndex, std::move(fragment)});

			// TODO: Here we essentially mark open fragments as having content, there are probably better ways to achieve this.
			ForAllOpenFragments([box_cursor = box_cursor](PlacedFragment& open_fragment) { open_fragment.has_content = true; });
		}
	}
	else
	{
		RMLUI_ASSERT(!first_box);
		// TODO We couldn't place it on this line, wrap it down to the next one.
		box_fully_placed = false;
	}

	return box_fully_placed;
}

float LayoutLineBox::Close(Element* offset_parent, Vector2f line_position, float element_line_height)
{
	RMLUI_ASSERT(!is_closed);
	RMLUI_ASSERTMSG(open_fragment_index == InvalidIndex, "Some fragments were not properly closed.");

	// Vertically align fragments and size line.
	float height_of_line = element_line_height;

	for (const auto& fragment : fragments)
		height_of_line = Math::Max(fragment.layout_bounds.y, height_of_line);

	// TODO: Alignment

	// Position and size all inline-level boxes, place geometry boxes.
	for (const auto& fragment : fragments)
	{
		if (fragment.layout_bounds.x >= 0.f)
		{
			if (fragment.layout_fragment.type == LayoutFragment::Type::Principal)
				fragment.inline_box->Submit(offset_parent, line_position + fragment.position, fragment.layout_bounds,
					std::move(fragment.layout_fragment.text));
			else
				fragment.inline_box->SubmitFragment(line_position + fragment.position, fragment.layout_bounds,
					std::move(fragment.layout_fragment.text));
		}
	}

	is_closed = true;

	return height_of_line;
}

void LayoutLineBox::CloseInlineBox(InlineBox* inline_box)
{
	PlacedFragment* fragment = GetFragment(open_fragment_index);
	if (fragment && fragment->inline_box == inline_box)
	{
		open_fragment_index = fragment->parent_index;
		fragment->layout_bounds.x = box_cursor - fragment->position.x;
		fragment->parent_index = InvalidIndex;
		box_cursor += inline_box->GetOuterSpacing(Box::RIGHT);
	}
}

String LayoutLineBox::DebugDumpTree(int depth) const
{
	const String value =
		String(depth * 2, ' ') + "LayoutLineBox (" + ToString(fragments.size()) + " fragment" + (fragments.size() == 1 ? "" : "s") + ")\n";

	return value;
}

void* LayoutLineBox::operator new(size_t size)
{
	return LayoutEngine::AllocateLayoutChunk(size);
}

void LayoutLineBox::operator delete(void* chunk, size_t size)
{
	LayoutEngine::DeallocateLayoutChunk(chunk, size);
}

} // namespace Rml
