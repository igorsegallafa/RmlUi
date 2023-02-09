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

namespace Rml {

/**
    A container for inline-level boxes.

    Always a direct child of a block container, and starts a new inline formatting context. Maintains a stack of line
    boxes in which generated inline-level boxes are placed within. Not directly a CSS term, but effectively a "block
    container that only contains inline-level boxes".

    @author Michael R. P. Ragazzon
 */
class InlineContainer final : public LayoutBox {
public:
	/// Creates a new block box in an inline context.
	InlineContainer(BlockContainer* parent, float available_width, float element_line_height, bool wrap_content);
	~InlineContainer();

	/// Adds a new inline-level element to this inline-context box.
	/// @param[in] element The new inline-level element.
	/// @param[in] box The box defining the element's bounds.
	/// @return The inline box if one was generated for the elmeent, otherwise nullptr.
	/// @note Any non-null return value must be closed with a call to CloseInlineElement().
	InlineBox* AddInlineElement(Element* element, const Box& box);

	/// Closes the previously added inline box.
	/// @param[in] inline_box The box to close.
	/// @note Calls to this function should be submitted in reverse order to AddInlineElement().
	void CloseInlineElement(InlineBox* inline_box);

	/// Add a break to the last line.
	void AddBreak(float line_height);
	/// Adds a line box for resuming one that was previously split.
	/// @param[in] open_line_box The line box overflowing from a previous inline container.
	void AddChainedBox(UniquePtr<LayoutLineBox> open_line_box);
	/// Place a float element next to our open line box, if possible.
	/// @param space The space for the float to be placed inside.
	/// @param element The float element to be placed.
	/// @return True if the element was placed, otherwise false.
	bool PlaceFloatElement(Element* element, LayoutBlockBoxSpace* space);

	/// Closes the box. This will determine the element's height (if it was unspecified).
	/// @param[out] Optionally, output the open inline box.
	/// @return The result of the close; this may request a reformat of this element or our parent.
	CloseResult Close(UniquePtr<LayoutLineBox>* out_open_line_box);

	/// Calculate the dimensions of the box's internal content width; i.e. the size used to calculate the shrink-to-fit width.
	float GetShrinkToFitWidth() const;

	/// Returns an estimate for the position of a hypothetical next box to be placed, relative to the content box of this container.
	Vector2f GetStaticPositionEstimate(bool inline_level_box) const;

	/// Get the baseline of the last line.
	/// @return True if the baseline was found.
	bool GetBaselineOfLastLine(float& out_baseline) const override;

	String DebugDumpTree(int depth) const override;

private:
	using LineBoxList = Vector<UniquePtr<LayoutLineBox>>;

	LayoutLineBox* EnsureOpenLineBox();
	LayoutLineBox* GetOpenLineBox() const;
	InlineBoxBase* GetOpenInlineBox();

	/// Close any open line box.
	/// @param[in] split_all_open_boxes Split all open inline boxes, even if they have no content.
	/// @param[out] out_split_line Optionally return any resulting split line, otherwise it will be added as a new line box to this container.
	void CloseOpenLineBox(bool split_all_open_boxes, UniquePtr<LayoutLineBox>* out_split_line = nullptr);

	/// Find and set the position and line width for the currently open line box.
	/// @param[in] line_box The currently open line box.
	/// @param[in] minimum_width The minimum line width to consider for this search.
	/// @param[in] minimum_height The minimum line height to to be considered for this and future searches.
	void UpdateLineBoxPlacement(LayoutLineBox* line_box, float minimum_width, float minimum_height);

	BlockContainer* parent; // [not-null]

	Vector2f position;
	Vector2f box_size;

	// The element's computed line-height. Not necessarily the same as the height of our lines.
	float element_line_height;
	// True if the block box's line boxes should stretch to fit their inline content instead of wrapping.
	bool wrap_content;
	// The element's text-align property.
	Style::TextAlign text_align;

	// The vertical position of the currently open line, or otherwise the next one to be placed, relative to the top of this box.
	float box_cursor = 0;

	// The root of the tree of inline boxes located in this inline container.
	InlineBoxRoot root_inline_box;

	// The list of line boxes, each of which flows fragments generated by inline boxes.
	// @performance Use first_child, next_sibling instead to build the list?
	LineBoxList line_boxes;
};

} // namespace Rml
#endif
