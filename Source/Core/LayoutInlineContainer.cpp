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
#include "../../Include/RmlUi/Core/ElementUtilities.h"
#include "../../Include/RmlUi/Core/Profiling.h"
#include "../../Include/RmlUi/Core/Property.h"
#include "LayoutBlockBox.h"
#include "LayoutBlockBoxSpace.h"
#include "LayoutDetails.h"
#include "LayoutInlineLevelBoxText.h"

namespace Rml {

InlineContainer::InlineContainer(BlockContainer* _parent, float _element_line_height, bool _wrap_content) :
	BlockLevelBox(Type::InlineContainer), parent(_parent), element_line_height(_element_line_height), wrap_content(_wrap_content)
{
	RMLUI_ASSERT(_parent);

	const Vector2f containing_block = LayoutDetails::GetContainingBlock(parent);
	box_size = Vector2f(containing_block.x, -1);
	position = parent->NextBoxPosition();
}

InlineContainer::~InlineContainer() {}

InlineBox* InlineContainer::AddInlineElement(Element* element, const Box& box)
{
	RMLUI_ASSERT(element);

	InlineBox* inline_box = nullptr;
	InlineLevelBox* inline_level_box = nullptr;
	InlineBoxBase* open_inline_box = GetOpenInlineBox();

	if (auto text_element = rmlui_dynamic_cast<ElementText*>(element))
	{
		inline_level_box = open_inline_box->AddChild(MakeUnique<InlineLevelBox_Text>(text_element));
	}
	else if (box.GetSize().x >= 0.f)
	{
		inline_level_box = open_inline_box->AddChild(MakeUnique<InlineLevelBox_Atomic>(element, box));
	}
	else
	{
		auto inline_box_ptr = MakeUnique<InlineBox>(element, box);
		inline_box = inline_box_ptr.get();
		inline_level_box = open_inline_box->AddChild(std::move(inline_box_ptr));
	}

	LayoutOverflowHandle overflow_handle = {};

	while (true)
	{
		LayoutLineBox* line_box = EnsureOpenLineBox();

		// TODO: subtract floats
		const float line_width = box_size.x;

		const bool add_to_new_line = line_box->AddBox(inline_level_box, wrap_content, line_width, overflow_handle);
		if (!add_to_new_line)
			break;

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

	// Expand our content area if any line boxes had to push themselves out.
	// TODO
	// for (size_t i = 0; i < line_boxes.size(); i++)
	//	box_size.x = Math::Max(box_size.x, line_boxes[i]->GetDimensions().x);

	// Set this box's height, if necessary.
	if (box_size.y < 0)
		box_size.y = Math::Max(box_cursor, 0.f);

	Vector2f visible_overflow_size;

	// Find the largest line in this layout block
	for (size_t i = 0; i < line_boxes.size(); i++)
	{
		LayoutLineBox* line_box = line_boxes[i].get();
		visible_overflow_size.x = Math::Max(visible_overflow_size.x, line_box->GetBoxCursor()); // TODO: Should also add line position
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
		const BlockContainer* offset_parent = parent->GetOffsetParent();
		const Vector2f relative_position = position - (offset_parent->GetPosition() - parent->GetOffsetRoot()->GetPosition());
		const Vector2f line_position = {relative_position.x, relative_position.y + box_cursor};

		// TODO Position due to floats.
		float height_of_line = 0.f;
		UniquePtr<LayoutLineBox> split_line_box = line_box->Close(offset_parent->GetElement(), line_position, element_line_height, height_of_line);
		box_cursor += height_of_line;

		if (split_line_box)
		{
			if (out_split_line)
				*out_split_line = std::move(split_line_box);
			else
				line_boxes.push_back(std::move(split_line_box));
		}
	}
}

Vector2f InlineContainer::NextBoxPosition(float top_margin, Style::Clear clear_property) const
{
	Vector2f box_position = position;
	box_position.y += box_cursor;

	const float clear_margin =
		parent->GetBlockBoxSpace()->DetermineClearPosition(box_position.y + top_margin, clear_property) - (box_position.y + top_margin);
	if (clear_margin > 0)
		box_position.y += clear_margin;

	return box_position;
}

Vector2f InlineContainer::NextLineBoxPosition(float& out_box_width, const Vector2f dimensions) const
{
	const Vector2f cursor = NextBoxPosition();
	const Vector2f box_position = parent->GetBlockBoxSpace()->NextBoxPosition(out_box_width, cursor.y, dimensions);
	return box_position;
}

float InlineContainer::GetShrinkToFitWidth() const
{
	float content_width = 0.0f;

	// Find the largest line in this layout block
	for (size_t i = 0; i < line_boxes.size(); i++)
	{
		// Perhaps a more robust solution is to modify how we set the line box dimension on 'line_box->close()'
		// and use that, or add another value in the line_box ... but seems to work for now.
		LayoutLineBox* line_box = line_boxes[i].get();
		content_width = Math::Max(content_width, line_box->GetBoxCursor()); // TODO line positions due to floats?
	}
	content_width = Math::Min(content_width, box_size.x);

	return content_width;
}

float InlineContainer::GetHeightIncludingOpenLine() const
{
	return box_cursor + (GetOpenLineBox() ? element_line_height : 0.f);
}

bool InlineContainer::GetBaselineOfLastLine(float& out_baseline) const
{
	bool found_baseline = false;
	for (int j = (int)line_boxes.size() - 1; j >= 0; j--)
	{
		// TODO
		// found_baseline = line_boxes[j]->GetBaselineOfLastLine(out_baseline);
		if (found_baseline)
			break;
	}
	return found_baseline;
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
