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
#include "LayoutInlineContainer.h"
#include "LayoutInlineLevelBoxText.h"
#include <numeric>

namespace Rml {

LayoutLineBox::~LayoutLineBox() {}

bool LayoutLineBox::AddBox(InlineLevelBox* box, InlineLayoutMode layout_mode, float line_width, LayoutOverflowHandle& inout_overflow_handle)
{
	RMLUI_ASSERT(!is_closed);

	// TODO: The spacing this element must leave on the right of the line, to account not only for its margins and padding,
	// but also for its parents which will close immediately after it.
	// (Right edge width of all open fragments)
	// TODO: We don't necessarily need to consider all the open boxes if there is content coming after this box.
	const float open_spacing_right = std::accumulate(open_fragments.begin(), open_fragments.end(), 0.f,
		[](float value, const auto& fragment) { return value + fragment.spacing_right; });
	const float box_placement_cursor = box_cursor + open_spacing_left;
	const bool first_box = fragments.empty();

	// TODO: Maybe always pass the actual available width, and let the createfragment functions handle the mode correctly.
	float available_width = FLT_MAX;
	if (layout_mode != InlineLayoutMode::Nowrap)
		available_width = Math::Max(Math::RoundUpFloat(line_width - box_placement_cursor), 0.f);

	FragmentResult fragment = box->CreateFragment(layout_mode, available_width, open_spacing_right, first_box, inout_overflow_handle);

	inout_overflow_handle = {};
	bool continue_on_new_line = false;

	switch (fragment.type)
	{
	case FragmentType::InlineBox:
	{
		// Opens up an inline box.
		RMLUI_ASSERT(fragment.layout_bounds.x < 0.f && fragment.layout_bounds.y >= 0.f);
		RMLUI_ASSERT(rmlui_dynamic_cast<InlineBox*>(box));

		open_fragments.push_back(OpenFragment{static_cast<InlineBox*>(box), {box_placement_cursor, 0.f}, fragment.layout_bounds.y,
			fragment.spacing_left, fragment.spacing_right});
		open_spacing_left += fragment.spacing_left;
	}
	break;
	case FragmentType::Principal:
	case FragmentType::Additional:
	{
		// Fixed-size fragment.
		RMLUI_ASSERT(fragment.layout_bounds.x >= 0.f && fragment.layout_bounds.y >= 0.f);

		fragments.push_back(PlacedFragment{box, {box_placement_cursor, 0.f}, fragment.layout_bounds, std::move(fragment.text),
			(fragment.type == FragmentType::Principal)});
		box_cursor = box_placement_cursor + fragment.layout_bounds.x;
		open_spacing_left = 0.f;

		if (fragment.overflow_handle)
		{
			continue_on_new_line = true;
			inout_overflow_handle = fragment.overflow_handle;
		}

		// TODO: Mark open fragments as having content so we later know whether we should split or move them in case of overflow. There are probably
		// better ways to go about this.
		for (auto&& open_fragment : open_fragments)
			open_fragment.has_content = true;
	}
	break;
	case FragmentType::Invalid:
	{
		// Could not place fragment on this line, try again on a new line.
		RMLUI_ASSERT(layout_mode == InlineLayoutMode::WrapAny);
		continue_on_new_line = true;
	}
	break;
	}

	return continue_on_new_line;
}

UniquePtr<LayoutLineBox> LayoutLineBox::Close(Element* offset_parent, Vector2f line_position, float element_line_height, float& out_height_of_line)
{
	RMLUI_ASSERT(!is_closed);

	UniquePtr<LayoutLineBox> new_line_box = SplitLine();

	RMLUI_ASSERTMSG(open_fragments.empty(), "All open fragments must be closed or split before the line can be closed.");

	// Vertically align fragments and size line.
	out_height_of_line = element_line_height;
	for (const auto& fragment : fragments)
		out_height_of_line = Math::Max(fragment.layout_bounds.y, out_height_of_line);

	// TODO: Alignment

	// Position and size all inline-level boxes, place geometry boxes.
	for (const auto& fragment : fragments)
	{
		RMLUI_ASSERT(fragment.layout_bounds.x >= 0.f);
		FragmentBox fragment_box = {
			offset_parent,
			line_position + fragment.position,
			fragment.layout_bounds,
			fragment.principal_fragment,
			fragment.split_left,
			fragment.split_right,
		};
		fragment.inline_level_box->Submit(fragment_box, std::move(fragment.text));
	}

	is_closed = true;

	return new_line_box;
}

LayoutLineBox::PlacedFragment& LayoutLineBox::PlaceFragment(const OpenFragment& open_fragment, float inner_right_position)
{
	fragments.push_back({});
	PlacedFragment& fragment = fragments.back();

	fragment.inline_level_box = open_fragment.inline_box;
	fragment.position = open_fragment.position;

	// TODO: Maybe store layout bounds as outer size always?
	fragment.layout_bounds.x =
		Math::Max(inner_right_position - (open_fragment.position.x + open_fragment.spacing_left * (open_fragment.split_left ? 0.f : 1.0f)), 0.f);

	fragment.layout_bounds.y = open_fragment.layout_height_bound;

	fragment.principal_fragment = open_fragment.principal_fragment;
	fragment.split_left = open_fragment.split_left;
	fragment.split_right = open_fragment.split_right;

	return fragment;
}

UniquePtr<LayoutLineBox> LayoutLineBox::SplitLine()
{
	// Place any open fragments that have content, splitting their right side. An open copy is kept for split placement on the next line.
	for (auto it = open_fragments.crbegin(); it != open_fragments.crend(); ++it)
	{
		const OpenFragment& open_fragment = *it;
		if (open_fragment.has_content)
		{
			PlacedFragment& new_fragment = PlaceFragment(open_fragment, box_cursor);
			new_fragment.split_right = true;
		}
	}

	if (open_fragments.empty())
		return nullptr;

	auto new_line = MakeUnique<LayoutLineBox>();

	// Move all open fragments to the next line. Fragments that were placed on the previous line Split open fragments with content, move fragments
	// without content.
	new_line->open_fragments = std::move(open_fragments);
	for (OpenFragment& fragment : new_line->open_fragments)
	{
		fragment.position.x = new_line->box_cursor;
		if (fragment.has_content)
		{
			fragment.split_left = true;
			fragment.principal_fragment = false;
			fragment.has_content = false;
		}
		else
		{
			new_line->open_spacing_left += fragment.spacing_left;
		}
	}

	return new_line;
}

void LayoutLineBox::CloseInlineBox(InlineBox* inline_box)
{
	if (open_fragments.empty() || open_fragments.back().inline_box != inline_box)
	{
		RMLUI_ERRORMSG("Inline box open/close mismatch.");
		return;
	}

	box_cursor += open_spacing_left;
	open_spacing_left = 0.f;

	OpenFragment& open_fragment = open_fragments.back();
	PlaceFragment(open_fragment, box_cursor);

	box_cursor += open_fragment.spacing_right;
	open_fragments.pop_back();
}

InlineBox* LayoutLineBox::GetOpenInlineBox()
{
	if (open_fragments.empty())
		return nullptr;

	return open_fragments.back().inline_box;
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
