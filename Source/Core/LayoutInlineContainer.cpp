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

#include "LayoutInlineContainer.h"
#include "../../Include/RmlUi/Core/ComputedValues.h"
#include "../../Include/RmlUi/Core/Element.h"
#include "../../Include/RmlUi/Core/ElementScroll.h"
#include "../../Include/RmlUi/Core/ElementText.h"
#include "../../Include/RmlUi/Core/ElementUtilities.h"
#include "../../Include/RmlUi/Core/Profiling.h"
#include "../../Include/RmlUi/Core/Property.h"
#include "LayoutBlockBox.h"
#include "LayoutBlockBoxSpace.h"
#include "LayoutDetails.h"
#include "LayoutInlineLevelBox.h"

namespace Rml {

InlineContainer::InlineContainer(BlockContainer* _parent, float _available_width, float _element_line_height, bool _wrap_content) :
	LayoutBox(Type::InlineContainer), parent(_parent), element_line_height(_element_line_height), wrap_content(_wrap_content),
	root_inline_box(_parent->GetElement())
{
	RMLUI_ASSERT(_parent);

	box_size = Vector2f(_available_width, -1);
	position = parent->NextBoxPosition();
	text_align = parent->GetElement()->GetComputedValues().text_align();
}

InlineContainer::~InlineContainer() {}

InlineBox* InlineContainer::AddInlineElement(Element* element, const Box& box)
{
	RMLUI_ASSERT(element);

	InlineBox* inline_box = nullptr;
	InlineLevelBox* inline_level_box = nullptr;
	InlineBoxBase* parent_box = GetOpenInlineBox();

	if (auto text_element = rmlui_dynamic_cast<ElementText*>(element))
	{
		inline_level_box = parent_box->AddChild(MakeUnique<InlineLevelBox_Text>(text_element));
	}
	else if (box.GetSize().x >= 0.f)
	{
		inline_level_box = parent_box->AddChild(MakeUnique<InlineLevelBox_Atomic>(parent_box, element, box));
	}
	else
	{
		auto inline_box_ptr = MakeUnique<InlineBox>(parent_box, element, box);
		inline_box = inline_box_ptr.get();
		inline_level_box = parent_box->AddChild(std::move(inline_box_ptr));
	}

	const float minimum_line_height = Math::Max(element_line_height, (box.GetSize().y >= 0.f ? box.GetSizeAcross(Box::VERTICAL, Box::MARGIN) : 0.f));

	LayoutOverflowHandle overflow_handle = {};
	float minimum_width_next = 0.f;

	while (true)
	{
		LayoutLineBox* line_box = EnsureOpenLineBox();

		UpdateLineBoxPlacement(line_box, minimum_width_next, minimum_line_height);

		InlineLayoutMode layout_mode = InlineLayoutMode::Nowrap;
		if (wrap_content)
		{
			const bool line_shrinked_by_floats = (line_box->GetLineWidth() < box_size.x);
			const bool can_wrap_any = (line_shrinked_by_floats || line_box->HasContent());
			layout_mode = (can_wrap_any ? InlineLayoutMode::WrapAny : InlineLayoutMode::WrapAfterContent);
		}

		const bool add_new_line = line_box->AddBox(inline_level_box, layout_mode, overflow_handle);
		if (!add_new_line)
			break;

		minimum_width_next = (line_box->HasContent() ? 0.f : line_box->GetLineWidth() + 1.f);

		// Keep adding boxes on a new line, either because the box couldn't fit on the current line at all, or because it had to be split.
		CloseOpenLineBox(false);
	}

	return inline_box;
}

void InlineContainer::CloseInlineElement(InlineBox* inline_box)
{
	if (LayoutLineBox* line_box = GetOpenLineBox())
	{
		line_box->CloseInlineBox(inline_box);
	}
	else
	{
		RMLUI_ERROR;
	}
}

void InlineContainer::AddBreak(float line_height)
{
	// Simply end the line if one is open, otherwise increment by the line height.
	if (LayoutLineBox* line_box = GetOpenLineBox())
		CloseOpenLineBox(true);
	else
		box_cursor += line_height;
}

void InlineContainer::AddChainedBox(UniquePtr<LayoutLineBox> open_line_box)
{
	RMLUI_ASSERT(line_boxes.empty());
	RMLUI_ASSERT(open_line_box && !open_line_box->IsClosed());
	line_boxes.push_back(std::move(open_line_box));
}

bool InlineContainer::PlaceFloatElement(Element* element, LayoutBlockBoxSpace* space)
{
	if (LayoutLineBox* line_box = GetOpenLineBox())
	{
		const Vector2f margin_size = element->GetBox().GetSize(Box::MARGIN);
		const Style::Float float_property = element->GetComputedValues().float_();
		const Style::Clear clear_property = element->GetComputedValues().clear();

		const float line_box_top = position.y + box_cursor;
		float available_width = 0.f;
		const Vector2f float_position = space->NextFloatPosition(parent, available_width, line_box_top, margin_size, float_property, clear_property);

		const float line_box_bottom = line_box_top + element_line_height;
		const float line_box_and_element_width = margin_size.x + line_box->GetBoxCursor();

		// If the float can be positioned on this line, and it can fit next to the line's contents, place it now.
		if (float_position.y < line_box_bottom && line_box_and_element_width <= available_width)
		{
			space->PlaceFloat(parent, element, position.y + box_cursor);
			UpdateLineBoxPlacement(line_box, 0.f, element_line_height);
			return true;
		}
	}

	return false;
}

InlineContainer::CloseResult InlineContainer::Close(UniquePtr<LayoutLineBox>* out_open_line_box)
{
	// The parent container may need the open line box to be split and resumed.
	CloseOpenLineBox(true, out_open_line_box);

	// It is possible that floats were queued between the last line close and this container close, if so place them now.
	parent->PlaceQueuedFloats(box_cursor);

	// Expand our content area if any line boxes had to push themselves out.
	// TODO
	// for (size_t i = 0; i < line_boxes.size(); i++)
	//	box_size.x = Math::Max(box_size.x, line_boxes[i]->GetDimensions().x);

	// Set this box's height.
	box_size.y = Math::Max(box_cursor, 0.f);

	// TODO: Specify which coordinate system is used for overflow size.
	Vector2f visible_overflow_size = {0.f, box_size.y};

	// Find the largest line in this layout block
	for (const auto& line_box : line_boxes)
	{
		visible_overflow_size.x = Math::Max(visible_overflow_size.x, line_box->GetPosition().x - position.x + line_box->GetExtentRight());
	}

	visible_overflow_size.x = Math::RoundDownFloat(visible_overflow_size.x);
	SetVisibleOverflowSize(visible_overflow_size);

	// Increment the parent's cursor.
	// If this close fails, it means this block box has caused our parent block box to generate an automatic vertical scrollbar.
	if (!parent->CloseChildBox(this, position, Vector2f(0), box_size))
		return CloseResult::LayoutParent;

	return CloseResult::OK;
}

void InlineContainer::CloseOpenLineBox(bool split_all_open_boxes, UniquePtr<LayoutLineBox>* out_split_line)
{
	if (LayoutLineBox* line_box = GetOpenLineBox())
	{
		float height_of_line = 0.f;
		UniquePtr<LayoutLineBox> split_line_box = line_box->DetermineVerticalPositioning(&root_inline_box, split_all_open_boxes, height_of_line);

		// If the final height of the line is larger than previously considered, we might need to push the line down to
		// clear overlapping floats.
		if (height_of_line > line_box->GetLineMinimumHeight())
			UpdateLineBoxPlacement(line_box, 0.f, height_of_line);

		// Now that the line has been given a final position and size, close the line box to submit all the fragments.
		// Our parent block container acts as the containing block for our inline boxes.
		line_box->Close(parent->GetElement(), parent->GetPosition(), text_align);

		// Move the cursor down, unless we should collapse the line.
		if (!line_box->CanCollapseLine())
			box_cursor = (line_box->GetPosition().y - position.y) + height_of_line;

		// If we have any pending floating elements for our parent, then this would be an ideal time to place them.
		parent->PlaceQueuedFloats(box_cursor);

		if (split_line_box)
		{
			if (out_split_line)
				*out_split_line = std::move(split_line_box);
			else
				line_boxes.push_back(std::move(split_line_box));
		}
	}
}

void InlineContainer::UpdateLineBoxPlacement(LayoutLineBox* line_box, float minimum_width, float minimum_height)
{
	RMLUI_ASSERT(line_box);

	Vector2f minimum_dimensions = {
		Math::Max(minimum_width, line_box->GetBoxCursor()),
		Math::Max(minimum_height, line_box->GetLineMinimumHeight()),
	};

	// @performance: We might benefit from doing this search only when the minimum dimensions change, or if we get new inline floats.
	const float ideal_position_y = position.y + box_cursor;
	float available_width = 0.f;
	const Vector2f line_position =
		parent->GetBlockBoxSpace()->NextBoxPosition(parent, available_width, ideal_position_y, minimum_dimensions, !wrap_content);
	available_width = Math::Max(available_width, 0.f);

	line_box->SetLineBox(line_position, available_width, minimum_dimensions.y);
}

float InlineContainer::GetShrinkToFitWidth() const
{
	// TODO: This is basically the same as visible overflow size?
	float content_width = 0.0f;

	// Find the largest line in this layout block
	for (size_t i = 0; i < line_boxes.size(); i++)
	{
		// Perhaps a more robust solution is to modify how we set the line box dimension on 'line_box->close()'
		// and use that, or add another value in the line_box ... but seems to work for now.
		LayoutLineBox* line_box = line_boxes[i].get();
		content_width = Math::Max(content_width, line_box->GetPosition().x - position.x + line_box->GetBoxCursor());
	}
	content_width = Math::Min(content_width, box_size.x);

	return content_width;
}

Vector2f InlineContainer::GetStaticPositionEstimate(bool inline_level_box) const
{
	Vector2f result = {0.f, box_cursor};

	if (const LayoutLineBox* line_box = GetOpenLineBox())
	{
		if (inline_level_box)
			result.x += line_box->GetBoxCursor();
		else
			result.y += element_line_height;
	}

	return result;
}

bool InlineContainer::GetBaselineOfLastLine(float& out_baseline) const
{
	if (!line_boxes.empty())
	{
		out_baseline = line_boxes.back()->GetPosition().y + line_boxes.back()->GetBaseline();
		return true;
	}
	return false;
}

LayoutLineBox* InlineContainer::EnsureOpenLineBox()
{
	if (line_boxes.empty() || line_boxes.back()->IsClosed())
	{
		line_boxes.push_back(MakeUnique<LayoutLineBox>());
	}
	return line_boxes.back().get();
}

LayoutLineBox* InlineContainer::GetOpenLineBox() const
{
	if (line_boxes.empty() || line_boxes.back()->IsClosed())
		return nullptr;
	return line_boxes.back().get();
}

InlineBoxBase* InlineContainer::GetOpenInlineBox()
{
	if (LayoutLineBox* line_box = GetOpenLineBox())
	{
		if (InlineBox* inline_box = line_box->GetOpenInlineBox())
			return inline_box;
	}
	return &root_inline_box;
}

String InlineContainer::DebugDumpTree(int depth) const
{
	String value = String(depth * 2, ' ') + "InlineContainer" + '\n';

	value += root_inline_box.DebugDumpTree(depth + 1);

	for (auto&& line_box : line_boxes)
		value += line_box->DebugDumpTree(depth + 1);

	return value;
}

} // namespace Rml
