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
#include "LayoutInlineBox.h"
#include "LayoutInlineContainer.h"
#include "LayoutInlineLevelBoxText.h"
#include <algorithm>
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
	const bool first_box = !HasContent();

	float open_spacing_right = 0.f;
	for (int fragment_index : open_fragments)
		open_spacing_right += fragments[fragment_index].spacing_right;

	const float box_placement_cursor = box_cursor + open_spacing_left;

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

		open_fragments.push_back((int)fragments.size());

		// TODO simplify, combine with below.
		fragments.push_back(Fragment{
			FragmentType::InlineBox,
			box,
			{box_placement_cursor, 0.f},
			fragment.layout_bounds,
			std::move(fragment.text),
			fragment.total_height_above_baseline,
			fragment.total_depth_below_baseline,
			true,
			false,
			false,
			false,
			fragment.spacing_left,
			fragment.spacing_right,
		});

		open_spacing_left += fragment.spacing_left;
	}
	break;
	case FragmentType::SizedBox:
	case FragmentType::TextRun:
	{
		// Fixed-size fragment.
		RMLUI_ASSERT(fragment.layout_bounds.x >= 0.f && fragment.layout_bounds.y >= 0.f);

		// TODO.
		fragments.push_back(Fragment{fragment.type, box, {box_placement_cursor, 0.f}, fragment.layout_bounds, std::move(fragment.text),
			fragment.total_height_above_baseline, fragment.total_depth_below_baseline, fragment.principal_fragment});
		box_cursor = box_placement_cursor + fragment.layout_bounds.x;
		open_spacing_left = 0.f;

		if (fragment.overflow_handle)
		{
			continue_on_new_line = true;
			inout_overflow_handle = fragment.overflow_handle;
		}

		// TODO: Mark open fragments as having content so we later know whether we should split or move them in case of overflow. There are probably
		// better ways to go about this.
		for (int fragment_index : open_fragments)
			fragments[fragment_index].has_content = true;
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

// Returns the fragment's baseline relative to the parent's baseline.
static float GetParentRelativeBaseline(const InlineLevelBox* parent, Style::VerticalAlign vertical_align, float height_above_baseline,
	float depth_below_baseline)
{
	using Style::VerticalAlign;

	// Let's specify two anchor points, one on the parent box and one local. The goal is to align the two points.
	float parent_baseline_offset = 0.f; // The anchor on the parent, as an offset from its baseline.
	float self_baseline_offset = 0.f;   // The offset from the parent anchor to our baseline.

	switch (vertical_align.type)
	{
	case VerticalAlign::Baseline: parent_baseline_offset = 0.f; break;
	case VerticalAlign::Length: parent_baseline_offset = -vertical_align.value; break;
	case VerticalAlign::Sub: parent_baseline_offset = (1.f / 5.f) * (float)parent->GetFontMetrics().size; break;
	case VerticalAlign::Super: parent_baseline_offset = (-1.f / 3.f) * (float)parent->GetFontMetrics().size; break;
	case VerticalAlign::TextTop:
		parent_baseline_offset = -parent->GetFontMetrics().ascent;
		self_baseline_offset = height_above_baseline;
		break;
	case VerticalAlign::TextBottom:
		parent_baseline_offset = parent->GetFontMetrics().descent;
		self_baseline_offset = -depth_below_baseline;
		break;
	case VerticalAlign::Middle:
		parent_baseline_offset = -0.5f * parent->GetFontMetrics().x_height;
		self_baseline_offset = 0.5f * (height_above_baseline - depth_below_baseline);
		break;
	case VerticalAlign::Top:
	case VerticalAlign::Bottom:
		// These are relative to the line box and handled later.
		break;
	}

	return parent_baseline_offset + self_baseline_offset;
}

UniquePtr<LayoutLineBox> LayoutLineBox::Close(const InlineBoxRoot* root_box, Element* offset_parent, Vector2f line_position,
	Style::TextAlign text_align, float& out_height_of_line)
{
	RMLUI_ASSERT(!is_closed);

	UniquePtr<LayoutLineBox> new_line_box = SplitLine();

	RMLUI_ASSERTMSG(open_fragments.empty(), "All open fragments must be closed or split before the line can be closed.");

	// Vertical alignment.
	//
	// To do alignment, place all boxes relative to the root baseline. We iterate over the fragments and retrieve the
	// current fragment's baseline relative to its parent baseline. Then it is just a matter of keeping track of the
	// parent fragment's absolute baseline while iterating over the fragments. Then, when all baselines are resolved,
	// find the maximum height above root baseline and maximum depth below root baseline. Their sum is the height of the
	// line. Now, we can vertically position each box based on the line-box height above baseline, the box's root
	// baseline offset, and the box's baseline-to-top height.
	//
	// TODO: Aligned subtree.

	{
		using Style::VerticalAlign;

		float max_ascent = 0.f;
		float max_descent = 0.f;
		root_box->GetStrut(max_ascent, max_descent);

		// We might be able to reuse the memory from this one (unless it was split).
		open_fragments.clear();

		for (int i = 0; i < (int)fragments.size(); i++)
		{
			if (!open_fragments.empty() && i == fragments[open_fragments.back()].children_end_index)
				open_fragments.pop_back();

			// Absolute means relative to root box's baseline.
			const float parent_absolute_baseline = (open_fragments.empty() ? 0.f : fragments[open_fragments.back()].baseline_offset);

			Fragment& fragment = fragments[i];

			const InlineLevelBox* parent = (open_fragments.empty() ? root_box : fragments[open_fragments.back()].inline_level_box);
			const VerticalAlign vertical_align = fragment.inline_level_box->GetVerticalAlign();

			const float parent_relative_baseline =
				GetParentRelativeBaseline(parent, vertical_align, fragment.total_height_above_baseline, fragment.total_depth_below_baseline);

			fragment.baseline_offset = parent_absolute_baseline + parent_relative_baseline;

			if (fragment.type == FragmentType::InlineBox)
				open_fragments.push_back(i);
		}

		open_fragments.clear();

		for (const Fragment& fragment : fragments)
		{
			// TODO: Cache vertical align?
			const VerticalAlign vertical_align = fragment.inline_level_box->GetVerticalAlign();

			if (fragment.type != FragmentType::TextRun && vertical_align.type != VerticalAlign::Top && vertical_align.type != VerticalAlign::Bottom)
			{
				max_ascent = Math::Max(max_ascent, fragment.total_height_above_baseline - fragment.baseline_offset);
				max_descent = Math::Max(max_descent, fragment.total_depth_below_baseline + fragment.baseline_offset);
			}
		}

		for (Fragment& fragment : fragments)
		{
			const VerticalAlign vertical_align = fragment.inline_level_box->GetVerticalAlign();
			if (vertical_align.type == VerticalAlign::Top)
			{
				max_descent = Math::Max(max_descent, fragment.layout_bounds.y - max_ascent);
			}
			else if (vertical_align.type == VerticalAlign::Bottom)
			{
				max_ascent = Math::Max(max_ascent, fragment.layout_bounds.y - max_descent);
			}
		}

		// Size line.
		out_height_of_line = max_ascent + max_descent;
		total_height_above_baseline = max_ascent;

		// Now that the line is sized we can set the vertical position of the fragments.
		for (Fragment& fragment : fragments)
		{
			switch (fragment.inline_level_box->GetVerticalAlign().type)
			{
			case VerticalAlign::Top: fragment.position.y = 0.f; break;
			case VerticalAlign::Bottom: fragment.position.y = out_height_of_line - fragment.layout_bounds.y; break;
			default: fragment.position.y = max_ascent - fragment.total_height_above_baseline + fragment.baseline_offset; break;
			}
		}
	}

	// Horizontal alignment using available space on our line.
	if (box_cursor < line_width)
	{
		switch (text_align)
		{
		case Style::TextAlign::Center: offset_horizontal_alignment = (line_width - box_cursor) * 0.5f; break;
		case Style::TextAlign::Right: offset_horizontal_alignment = (line_width - box_cursor); break;
		case Style::TextAlign::Left:    // Already left-aligned.
		case Style::TextAlign::Justify: // Justification occurs at the text level.
			break;
		}
	}

	// Position and size all inline-level boxes, place geometry boxes.
	for (const auto& fragment : fragments)
	{
		// Skip inline-boxes which have not been closed (moved down to next line).
		if (fragment.type == FragmentType::InlineBox && fragment.children_end_index == 0)
			continue;

		RMLUI_ASSERT(fragment.layout_bounds.x >= 0.f);
		FragmentBox fragment_box = {
			offset_parent,
			line_position + fragment.position + Vector2f(offset_horizontal_alignment, 0.f),
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

LayoutLineBox::Fragment& LayoutLineBox::CloseFragment(int open_fragment_index, float inner_right_position)
{
	// TODO: We only mark it as closing, don't really need anything else. Could achieve this in other ways too.
	Fragment& open_fragment = fragments[open_fragment_index];
	RMLUI_ASSERT(open_fragment.type == FragmentType::InlineBox);

	open_fragment.children_end_index = (int)fragments.size();

	// TODO: Maybe use outer position/size?
	open_fragment.layout_bounds.x =
		Math::Max(inner_right_position - open_fragment.position.x - (open_fragment.split_left ? 0.f : open_fragment.spacing_left), 0.f);

	return open_fragment;
}

UniquePtr<LayoutLineBox> LayoutLineBox::SplitLine()
{
	if (open_fragments.empty())
		return nullptr;

	// Make a new line with the open fragments.
	auto new_line = MakeUnique<LayoutLineBox>();
	new_line->fragments.reserve(open_fragments.size());

	// Copy all open fragments to the next line. Fragments that had any content placed on the previous line is split,
	// otherwise the whole fragment is moved down.
	for (const int fragment_index : open_fragments)
	{
		new_line->fragments.push_back(Fragment{fragments[fragment_index]});

		Fragment& fragment = new_line->fragments.back();
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

	// Place any open fragments that have content, splitting their right side. An open copy is kept for split placement on the next line.
	for (auto it = open_fragments.rbegin(); it != open_fragments.rend(); ++it)
	{
		const int fragment_index = *it;
		if (fragments[fragment_index].has_content)
		{
			Fragment& closed_fragment = CloseFragment(fragment_index, box_cursor);
			closed_fragment.split_right = true;
		}
	}

	// Steal the open fragment vector memory, as it is no longer needed here.
	new_line->open_fragments = std::move(open_fragments);
	std::iota(new_line->open_fragments.begin(), new_line->open_fragments.end(), 0);

	return new_line;
}

void LayoutLineBox::CloseInlineBox(InlineBox* inline_box)
{
	if (open_fragments.empty() || fragments[open_fragments.back()].inline_level_box != inline_box)
	{
		RMLUI_ERRORMSG("Inline box open/close mismatch.");
		return;
	}

	box_cursor += open_spacing_left;
	open_spacing_left = 0.f;

	Fragment& closed_fragment = CloseFragment(open_fragments.back(), box_cursor);

	box_cursor += closed_fragment.spacing_right;
	open_fragments.pop_back();
}

InlineBox* LayoutLineBox::GetOpenInlineBox()
{
	if (open_fragments.empty())
		return nullptr;

	return static_cast<InlineBox*>(fragments[open_fragments.back()].inline_level_box);
}

String LayoutLineBox::DebugDumpTree(int depth) const
{
	const String value =
		String(depth * 2, ' ') + "LayoutLineBox (" + ToString(fragments.size()) + " fragment" + (fragments.size() == 1 ? "" : "s") + ")\n";

	return value;
}

void LayoutLineBox::SetLineBox(Vector2f _line_position, float _line_width)
{
	line_position = _line_position;
	line_width = _line_width;
}

float LayoutLineBox::GetExtentRight() const
{
	RMLUI_ASSERT(is_closed);
	return box_cursor + offset_horizontal_alignment;
}

float LayoutLineBox::GetBaseline() const
{
	RMLUI_ASSERT(is_closed);
	return line_position.y + total_height_above_baseline;
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
