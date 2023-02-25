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

#ifndef RMLUI_CORE_LAYOUT_BLOCKCONTAINER_H
#define RMLUI_CORE_LAYOUT_BLOCKCONTAINER_H

#include "../../../Include/RmlUi/Core/Types.h"
#include "ContainerBox.h"

namespace Rml {

class FloatedBoxSpace;
class LineBox;
class InlineBox;
class InlineContainer;
struct InlineBoxHandle {
	InlineBox* inline_box;
};

/**
    A container for block-level boxes.

    TODO We act as the containing block for static and relative children, so this way our children's offset parent and
    containing block coincide, which is a nice feature.
    TODO Could generalize this so that the offset parent is defined to always be the element's containing block. To
    complete this we also need to consider absolute children of inline boxes.
 */
class BlockContainer final : public ContainerBox {
public:
	/// Creates a new block box for rendering a block element.
	/// @param parent[in] The parent of this block box. This will be nullptr for the root element.
	/// @param element[in] The element this block box is laying out.
	/// @param box[in] The box used for this block box.
	/// @param min_height[in] The minimum height of the content box.
	/// @param max_height[in] The maximum height of the content box.
	BlockContainer(ContainerBox* parent_container, FloatedBoxSpace* space, Element* element, const Box& box, float min_height, float max_height);
	/// Releases the block box.
	~BlockContainer();

	/// Closes the box. This will determine the element's height if it was unspecified.
	/// @param[in] parent_block_container Our parent which will be sized to contain this box, or nullptr for the root of the block formatting context.
	/// @return False if the block box caused an automatic vertical scrollbar to appear, forcing a reformat of the current block formatting context.
	bool Close(BlockContainer* parent_block_container);

	/// Creates and opens a new block box, and adds it as a child of this one.
	/// @param[in] element The new block element.
	/// @param[in] box The edges, width, and optionally height of the new box.
	/// @param[in] min_height The minimum height of the content box.
	/// @param[in] max_height The maximum height of the content box.
	/// @return The block box representing the element. Once the element's children have been positioned, Close() must be called on it.
	BlockContainer* OpenBlockBox(Element* element, const Box& box, float min_height, float max_height);

	/// Add a block-level box whose contents have been formatted in an independent formatting context.
	/// @param[in] block_level_box The box to add as a new child of this.
	/// @param[in] element The element represented by the new box.
	/// @param[in] box The formatted box.
	/// @return A pointer to the added block-level box.
	LayoutBox* AddBlockLevelBox(UniquePtr<LayoutBox> block_level_box, Element* element, const Box& box);

	// Adds an element to this block box to be handled as a floating element.
	void AddFloatElement(Element* element, Vector2f visible_overflow_size);

	/// Adds a new inline-level element to this block container.
	/// @param[in] element The new inline element.
	/// @param[in] box The box defining the element's bounds.
	/// @return A handle for the inline element, which must later be submitted to 'CloseInlineElement()'.
	/// @note Adds a new inline container to this box if needed, which starts a new inline formatting context.
	InlineBoxHandle AddInlineElement(Element* element, const Box& box);
	/// Closes a previously added inline element. This must be called after all its children have been added.
	/// @param[in] handle A handle previously returned from 'AddInlineElement()'.
	void CloseInlineElement(InlineBoxHandle handle);

	// Adds a line-break to this block box.
	void AddBreak();

	// Estimate the static position of a hypothetical next element to be placed.
	Vector2f GetOpenStaticPosition(Style::Display display) const;

	/// Returns the offset of a new child box to be placed here.
	/// @return The next box border position in the block formatting context space.
	Vector2f NextBoxPosition() const;
	/// Returns the offset of a new child box to be placed here. Collapses adjacent margins and optionally clears floats.
	/// @param[in] child_box The dimensions of the new box.
	/// @param[in] clear_property Specifies any floated boxes to be cleared (vertically skipped).
	/// @return The next border position in the block formatting context space.
	Vector2f NextBoxPosition(const Box& child_box, Style::Clear clear_property) const;

	// Places all queued floating elements.
	void PlaceQueuedFloats(float vertical_position);

	// Reset this box, so that it can be formatted again.
	void ResetContents();

	// Returns the block box's element.
	Element* GetElement() const;

	// Returns the block box space.
	const FloatedBoxSpace* GetBlockBoxSpace() const;

	// Returns the position box, relative to the border box of the root of our block formatting context.
	Vector2f GetPosition() const;

	Box& GetBox();
	const Box& GetBox() const;

	// -- Inherited from LayoutBox --

	const Box* GetIfBox() const override;
	float GetShrinkToFitWidth() const override;
	bool GetBaselineOfLastLine(float& out_baseline) const override;

private:
	InlineContainer* EnsureOpenInlineContainer();
	InlineContainer* GetOpenInlineContainer();
	const InlineContainer* GetOpenInlineContainer() const;

	const LayoutBox* GetOpenLayoutBox() const;

	/// Called by a closing child block-level box. Increments the cursor.
	/// @param[in] child The closing child box.
	/// @param[in] child_position The border position of the child, relative to the current block formatting context.
	/// @param[in] child_size The border size of the child.
	/// @param[in] child_margin_bottom The bottom margin width of the child.
	/// @return False if the block box caused an automatic vertical scrollbar to appear, forcing a reformat of the current block formatting context.
	/// TODO: Can we simplify this? Rename? This is more like increment box cursor and enlarge content size, and only needed for block-level boxes.
	bool CloseChildBox(LayoutBox* child, Vector2f child_position, float child_height, float child_margin_bottom);

	// Closes the inline container if there is one open. Returns false if our formatting context needs to be reformatted.
	bool CloseOpenInlineContainer();

	// Ensure that the interrupted line box is empty, otherwise produce a debug error.
	void EnsureEmptyInterruptedLineBox();

	// Positions a floating element within this block box.
	void PlaceFloat(Element* element, float vertical_position, Vector2f visible_overflow_size);


	// Debug dump layout tree.
	String DebugDumpTree(int depth) const override;

	using LayoutBoxList = Vector<UniquePtr<LayoutBox>>;
	struct QueuedFloat {
		Element* element;
		Vector2f visible_overflow_size;
	};
	using QueuedFloatList = Vector<QueuedFloat>;

	// Position of this box, relative to the border box of the root of our block formatting context.
	Vector2f position;

	Box box;
	float min_height = 0.f;
	float max_height = -1.f;

	// The vertical position of the next block box to be added to this box, relative to our box's top content edge.
	float box_cursor = 0.f;

	// Stores the floated boxes in the current block formatting context, if we are the root of the formatting context.
	UniquePtr<FloatedBoxSpace> root_space;
	// Pointer to the floated box space of the current block formatting context. [not-null]
	FloatedBoxSpace* space;
	// List of block-level boxes contained in this box.
	LayoutBoxList child_boxes;
	// Stores floated elements that are waiting for a line break to be positioned.
	QueuedFloatList queued_float_elements;
	// Stores the unplaced part of a line box that was split by a block-level box.
	UniquePtr<LineBox> interrupted_line_box;

	// The inner content size (excluding any padding/border/margins).
	// This is extended as child block boxes are closed, or from external formatting contexts.
	Vector2f inner_content_size;
};

} // namespace Rml
#endif
