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

#ifndef RMLUI_CORE_LAYOUTINLINECONTAINER_H
#define RMLUI_CORE_LAYOUTINLINECONTAINER_H

#include "../../Include/RmlUi/Core/Box.h"
#include "../../Include/RmlUi/Core/Types.h"
#include "LayoutBlockBox.h"
#include "LayoutInlineBox.h"
#include <stack>

namespace Rml {

/**
    A container for inline-level boxes. Always a direct child of a block container, starts a new inline formatting context.
    Not directly a CSS term, but effectively a "block container that only contains inline-level boxes".
 */
class InlineContainer final : public BlockLevelBox {
public:
	/// Creates a new block box in an inline context.
	InlineContainer(BlockContainer* parent, bool wrap_content);
	~InlineContainer();

	/// Adds a new inline element to this inline-context box.
	/// @param element[in] The new inline element.
	/// @param box[in] The box defining the element's bounds.
	/// @return The inline box representing the element. Once the element's children have been positioned, Close() must be called on it.
	LayoutInlineBox* AddInlineElement(Element* element, const Box& box);
	// TODO
	void CloseInlineElement(LayoutInlineBox* inline_box);

	/// Add a break to the last line.
	void AddBreak(float line_height);

	/// Adds an inline box for resuming an inline box that has been split.
	/// @param[in] chained_box The box overflowed from a previous line.
	void AddChainedBox(LayoutInlineBox* chained_box);

	/// Closes the box. This will determine the element's height (if it was unspecified).
	/// @param[out] Optionally, output the open inline box.
	/// @return The result of the close; this may request a reformat of this element or our parent.
	CloseResult Close(LayoutInlineBox** out_open_inline_box = nullptr);

	/// Returns the offset from the top-left corner of this box's offset element the next child box will be positioned at.
	/// @param[in] top_margin The top margin of the box. This will be collapsed as appropriate against other block boxes.
	/// @param[in] clear_property The value of the underlying element's clear property.
	/// @return The box cursor position.
	Vector2f NextBoxPosition(float top_margin = 0, Style::Clear clear_property = Style::Clear::None) const;
	/// Returns the offset from the top-left corner of this box for the next line.
	/// @param[out] box_width The available width for the line box.
	/// @param[out] wrap_content Set to true if the line box should grow to fit inline boxes, false if it should wrap them.
	/// @param[in] dimensions The minimum dimensions of the line.
	/// @return The line box position.
	Vector2f NextLineBoxPosition(float& out_box_width, bool& out_wrap_content, Vector2f dimensions) const;

	/// Calculate the dimensions of the box's internal content width; i.e. the size used to calculate the shrink-to-fit width.
	float GetShrinkToFitWidth() const;

	/// Returns the height of this inline container, including the last line even if it is still open.
	float GetHeightIncludingOpenLine() const;

	/// Get the baseline of the last line.
	/// @param[out] out_baseline
	/// @return True if the baseline was found.
	bool GetBaselineOfLastLine(float& out_baseline) const;

	String DebugDumpTree(int depth) const override;

private:
	using LineBoxList = Vector<UniquePtr<LayoutLineBox>>;

	LayoutLineBox* EnsureLineBox();

	BlockContainer* parent; // [not-null]

	Vector2f position;
	Vector2f box_size;

	// True if the block box's line boxes should stretch to fit their inline content instead of wrapping.
	bool wrap_content;

	// The vertical position of the next block box to be added to this box, relative to the top of this box.
	float box_cursor = 0;

	InlineBox_Root root_inline_box;

	// The open inline box; this is nullptr if all inline boxes are closed.
	// TODO: Add std::stack to Types.h?
	// @performance Perhaps add parent to inline boxes, so we only need to store the leaf open node here, thereby avoiding allocations.
	std::stack<LayoutInlineBox*> open_inline_boxes;

	// Used by inline contexts only; stores the list of line boxes flowing inline content.
	// @performance Use first_child, next_sibling instead to build the tree?
	LineBoxList line_boxes;
};

} // namespace Rml
#endif
