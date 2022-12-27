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

namespace Rml {

InlineContainer::InlineContainer(BlockContainer* _parent, bool _wrap_content) :
	BlockLevelBox(Type::InlineContainer), parent(_parent), wrap_content(_wrap_content)
{
	RMLUI_ASSERT(_parent);

	line_boxes.push_back(MakeUnique<LayoutLineBox>(this));

	const Vector2f containing_block = LayoutDetails::GetContainingBlock(parent);
	box_size = Vector2f(containing_block.x, -1);
	position = parent->NextBoxPosition();
}

InlineContainer::~InlineContainer() {}

LayoutInlineBox* InlineContainer::AddInlineElement(Element* element, const Box& box)
{
	// We're an inline context box, so we'll add this new inline element into our line boxes.
	return line_boxes.back()->AddElement(element, box);
}

void InlineContainer::AddBreak(float line_height)
{
	// Increment by the line height if no line is open, otherwise simply end the line.
	LayoutLineBox* last_line = line_boxes.back().get();
	if (last_line->GetDimensions().y < 0)
		box_cursor += line_height;
	else
		last_line->Close();
}

void InlineContainer::AddChainedBox(LayoutInlineBox* chained_box)
{
	line_boxes.back()->AddChainedBox(chained_box);
}

InlineContainer::CloseResult InlineContainer::Close(LayoutInlineBox** out_open_inline_box)
{
	// The parent container may need the open inline box to be split and resumed.
	if (out_open_inline_box)
		*out_open_inline_box = line_boxes.back()->GetOpenInlineBox();

	// We're an inline context box; so close our last line, which will still be open.
	line_boxes.back()->Close();

	// Expand our content area if any line boxes had to push themselves out.
	for (size_t i = 0; i < line_boxes.size(); i++)
		box_size.x = Math::Max(box_size.x, line_boxes[i]->GetDimensions().x);

	// Set this box's height, if necessary.
	if (box_size.y < 0)
		box_size.y = Math::Max(box_cursor, 0.f);

	Vector2f visible_overflow_size;

	// Find the largest line in this layout block
	for (size_t i = 0; i < line_boxes.size(); i++)
	{
		LayoutLineBox* line_box = line_boxes[i].get();
		visible_overflow_size.x = Math::Max(visible_overflow_size.x, line_box->GetBoxCursor());
	}

	SetVisibleOverflowSize(visible_overflow_size);

	// Increment the parent's cursor.
	// If this close fails, it means this block box has caused our parent block box to generate an automatic vertical scrollbar.
	if (!parent->CloseChildBox(this, position.y, box_size.y))
		return CloseResult::LayoutParent;

	return CloseResult::OK;
}

Vector2f InlineContainer::NextBoxPosition(float top_margin, Style::Clear clear_property) const
{
	// If our element is establishing a new offset hierarchy, then any children of ours don't inherit our offset.
	Vector2f box_position = position;
	box_position.y += box_cursor;

	const float clear_margin =
		parent->GetBlockBoxSpace()->DetermineClearPosition(box_position.y + top_margin, clear_property) - (box_position.y + top_margin);
	if (clear_margin > 0)
		box_position.y += clear_margin;

	return box_position;
}

Vector2f InlineContainer::NextLineBoxPosition(float& box_width, bool& _wrap_content, const Vector2f dimensions) const
{
	const Vector2f cursor = NextBoxPosition();
	const Vector2f box_position = parent->GetBlockBoxSpace()->NextBoxPosition(box_width, cursor.y, dimensions);

	// Also, probably shouldn't check for widths when positioning the box?
	_wrap_content = wrap_content;

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
		content_width = Math::Max(content_width, line_box->GetBoxCursor());
	}
	content_width = Math::Min(content_width, box_size.x);

	return content_width;
}

float InlineContainer::GetHeightIncludingOpenLine() const
{
	const float last_line_height = line_boxes.back()->GetDimensions().y;
	return box_cursor + Math::Max(0.0f, last_line_height);
}

bool InlineContainer::GetBaselineOfLastLine(float& out_baseline) const
{
	bool found_baseline = false;
	for (int j = (int)line_boxes.size() - 1; j >= 0; j--)
	{
		found_baseline = line_boxes[j]->GetBaselineOfLastLine(out_baseline);
		if (found_baseline)
			break;
	}
	return found_baseline;
}

String InlineContainer::DebugDumpTree(int depth) const
{
	String value = String(depth * 2, ' ') + "InlineContainer" + '\n';

	for (auto&& line_box : line_boxes)
		value += line_box->DebugDumpTree(depth + 1);

	return value;
}

} // namespace Rml
