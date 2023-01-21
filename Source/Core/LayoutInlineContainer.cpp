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
#include "LayoutInlineLevelBoxText.h"

namespace Rml {

InlineContainer::InlineContainer(BlockContainer* _parent, float _element_line_height, bool _wrap_content) :
	BlockLevelBox(Type::InlineContainer), parent(_parent), element_line_height(_element_line_height), wrap_content(_wrap_content),
	root_inline_box(_parent->GetElement())
{
	RMLUI_ASSERT(_parent);

	const Vector2f containing_block = LayoutDetails::GetContainingBlock(parent);
	box_size = Vector2f(containing_block.x, -1);
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

	// TODO: Move to InlineLevelBox?
	const float minimum_line_height = Math::Max(element_line_height, (box.GetSize().y >= 0.f ? box.GetSizeAcross(Box::VERTICAL, Box::MARGIN) : 0.f));

	LayoutOverflowHandle overflow_handle = {};
	float minimum_width_next = 0.f;

	while (true)
	{
		LayoutLineBox* line_box = EnsureOpenLineBox();

		const Vector2f minimum_dimensions = {
			Math::Max(line_box->GetBoxCursor(), minimum_width_next),
			minimum_line_height,
		};

		float available_width = 0.f;
		// TODO: We don't know the exact line height yet. Do we need to check placement after closing the line, or is
		// this approximation alright? Perhaps experiment with a very tall inline-element.
		// @performance: We could do this only once for each line, and instead update it if we get new inline floats.
		Vector2f line_position = NextLineBoxPosition(available_width, minimum_dimensions, !wrap_content);
		line_box->SetLineBox(line_position, available_width);

		// TODO: Cleanup logic
		const bool line_shrinked_by_floats = (available_width < box_size.x);
		const bool can_wrap_any = (line_shrinked_by_floats || line_box->HasContent());
		const InlineLayoutMode layout_mode =
			(wrap_content ? (can_wrap_any ? InlineLayoutMode::WrapAny : InlineLayoutMode::WrapAfterContent) : InlineLayoutMode::Nowrap);

		const bool add_to_new_line = line_box->AddBox(inline_level_box, layout_mode, available_width, overflow_handle);
		if (!add_to_new_line)
			break;

		minimum_width_next = (line_box->HasContent() ? 0.f : available_width + 1.f);

		// Keep adding boxes on a new line, either because the box couldn't fit on the current line at all, or because it had to be split.
		CloseOpenLineBox();
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
	// Increment by the line height if no line is open, otherwise simply end the line.
	if (LayoutLineBox* line_box = GetOpenLineBox())
		CloseOpenLineBox();
	else
		box_cursor += line_height;
}

void InlineContainer::AddChainedBox(UniquePtr<LayoutLineBox> open_line_box)
{
	RMLUI_ASSERT(line_boxes.empty());
	RMLUI_ASSERT(open_line_box && !open_line_box->IsClosed());
	line_boxes.push_back(std::move(open_line_box));
}

InlineContainer::CloseResult InlineContainer::Close(UniquePtr<LayoutLineBox>* out_open_line_box)
{
	// The parent container may need the open line box to be split and resumed.
	CloseOpenLineBox(out_open_line_box);

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
	if (!parent->CloseChildBox(this, position.y, box_size.y))
		return CloseResult::LayoutParent;

	return CloseResult::OK;
}

void InlineContainer::CloseOpenLineBox(UniquePtr<LayoutLineBox>* out_split_line)
{
	// Find the position of the line box, relative to its parent's block box's offset parent.
	if (LayoutLineBox* line_box = GetOpenLineBox())
	{
		// TODO Cleanup: Move calls to parent into function arguments where possible.

		// Find the position of the line box relative to its parent's block box's offset parent.
		const BlockContainer* offset_parent = parent->GetOffsetParent();
		const Vector2f line_position = line_box->GetPosition();

		// Make position relative to our own offset parent.
		const Vector2f root_to_offset_parent_offset = offset_parent->GetPosition() - parent->GetOffsetRoot()->GetPosition();
		const Vector2f line_position_offset_parent = line_position - root_to_offset_parent_offset;

		float height_of_line = 0.f;
		UniquePtr<LayoutLineBox> split_line_box =
			line_box->Close(&root_inline_box, offset_parent->GetElement(), line_position_offset_parent, text_align, height_of_line);

		// Move the cursor down, but only if our line has any width.
		if (line_box->GetBoxCursor() != 0.f)
			box_cursor = (line_position.y - position.y) + height_of_line;

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

Vector2f InlineContainer::NextLineBoxPosition(float& out_box_width, const Vector2f dimensions, const bool nowrap) const
{
	const float ideal_position_y = position.y + box_cursor;
	const Vector2f box_position = parent->GetBlockBoxSpace()->NextBoxPosition(out_box_width, ideal_position_y, dimensions, nowrap);
	return box_position;
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
		out_baseline = line_boxes.back()->GetBaseline();
		return true;
	}
	return false;
}

LayoutLineBox* InlineContainer::EnsureOpenLineBox()
{
	if (line_boxes.empty() || line_boxes.back()->IsClosed())
		line_boxes.push_back(MakeUnique<LayoutLineBox>());
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
