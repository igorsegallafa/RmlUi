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

#ifndef RMLUI_CORE_LAYOUTBLOCKBOX_H
#define RMLUI_CORE_LAYOUTBLOCKBOX_H

#include "../../Include/RmlUi/Core/Box.h"
#include "../../Include/RmlUi/Core/Types.h"
#include "LayoutLineBox.h"

namespace Rml {

class LayoutBlockBoxSpace;
class LayoutEngine;
class InlineContainer;

class BlockLevelBox {
public:
	enum class Type { BlockContainer, InlineContainer, FlexContainer, TableWrapper, Replaced };
	enum class CloseResult { OK, LayoutSelf, LayoutParent };

	Type GetType() const { return type; }

	// Returns the outer size of this box including overflowing content. Similar to scroll width, but shrinked if overflow is caught
	// here. This can be wider than the box if we are overflowing.
	// @note Only available after the box has been closed.
	Vector2f GetVisibleOverflowSize() const { return visible_overflow_size; }

	// Debug dump layout tree.
	String DumpLayoutTree(int depth = 0) const { return DumpTree(depth); }

	virtual ~BlockLevelBox() = default;

protected:
	BlockLevelBox(Type type) : type(type) {}

	void SetVisibleOverflowSize(Vector2f new_visible_overflow_size) { visible_overflow_size = new_visible_overflow_size; }

	// Debug dump layout tree.
	virtual String DumpTree(int depth) const = 0;

private:
	Type type;

	Vector2f visible_overflow_size;
};

/**
    @author Peter Curry
 */
class BlockContainer final : public BlockLevelBox {
public:
	/// Creates a new block box for rendering a block element.
	/// @param parent[in] The parent of this block box. This will be nullptr for the root element.
	/// @param element[in] The element this block box is laying out.
	/// @param box[in] The box used for this block box.
	/// @param min_height[in] The minimum height of the content box.
	/// @param max_height[in] The maximum height of the content box.
	BlockContainer(BlockContainer* parent, Element* element, const Box& box, float min_height, float max_height);
	/// Releases the block box.
	~BlockContainer();

	/// Closes the box. This will determine the element's height (if it was unspecified).
	/// @return The result of the close; this may request a reformat of this element or our parent.
	CloseResult Close();

	/// Called by a closing block box child. Increments the cursor.
	/// @param[in] child The closing child block-level box.
	/// @param[in] child_position_top The position of the child, relative to this container.
	/// @param[in] child_size_y The vertical margin size of the child.
	/// @return False if the block box caused an automatic vertical scrollbar to appear, forcing an entire reformat of the block box.
	bool CloseBlockBox(BlockLevelBox* child, float child_position_top, float child_size_y);

	/// Adds a new block element to this block-context box.
	/// @param element[in] The new block element.
	/// @param box[in] The box used for the new block box.
	/// @param min_height[in] The minimum height of the content box.
	/// @param max_height[in] The maximum height of the content box.
	/// @return The block box representing the element. Once the element's children have been positioned, Close() must be called on it.
	BlockContainer* AddBlockElement(Element* element, const Box& box, float min_height, float max_height);
	/// Adds a new inline element to this inline-context box.
	/// @param element[in] The new inline element.
	/// @param box[in] The box defining the element's bounds.
	/// @return The inline box representing the element. Once the element's children have been positioned, Close() must be called on it.
	LayoutInlineBox* AddInlineElement(Element* element, const Box& box);
	/// Adds a line-break to this block box.
	void AddBreak();

	/// Adds an element to this block box to be handled as a floating element.
	bool AddFloatElement(Element* element);

	/// Adds an element to this block box to be handled as an absolutely-positioned element. This element will be
	/// laid out, sized and positioned appropriately once this box is finished. This should only be called on boxes
	/// rendering in a block-context.
	/// @param element[in] The element to be positioned absolutely within this block box.
	void AddAbsoluteElement(Element* element);
	/// Formats, sizes, and positions all absolute elements in this block.
	void CloseAbsoluteElements();

	/// Returns the offset from the top-left corner of this box's offset element the next child box will be positioned at.
	/// @param[in] top_margin The top margin of the box. This will be collapsed as appropriate against other block boxes.
	/// @param[in] clear_property The value of the underlying element's clear property.
	/// @return The box cursor position.
	Vector2f NextBoxPosition(float top_margin = 0, Style::Clear clear_property = Style::Clear::None) const;
	/// Returns the offset from the top-left corner of this box's offset element the next child block box, of the given dimensions,
	/// will be positioned at. This will include the margins on the new block box.
	/// @param[in] box The dimensions of the new box.
	/// @param[in] clear_property The value of the underlying element's clear property.
	/// @return The block box cursor position.
	Vector2f NextBlockBoxPosition(const Box& box, Style::Clear clear_property) const;

	// Places all queued floating elements.
	void PlaceQueuedFloats(float vertical_offset);

	/// Calculate the dimensions of the box's internal content width; i.e. the size used to calculate the shrink-to-fit width.
	float GetShrinkToFitWidth() const;
	/// Set the inner content size if it is larger than the current value on each axis individually.
	void ExtendInnerContentSize(Vector2f inner_content_size);

	/// Returns the block box's element.
	Element* GetElement() const;

	/// Returns the block box's parent.
	BlockContainer* GetParent() const;

	/// Returns the block box space.
	LayoutBlockBoxSpace* GetBlockBoxSpace() const;

	/// Returns the position of the block box, relative to its parent's content area.
	/// @return The relative position of the block box.
	Vector2f GetPosition() const;

	/// Returns the block box against which all positions of boxes in the hierarchy are set relative to.
	/// @return This box's offset parent.
	const BlockContainer* GetOffsetParent() const;
	/// Returns the block box against which all positions of boxes in the hierarchy are calculated relative to.
	/// @return This box's offset root.
	const BlockContainer* GetOffsetRoot() const;

	/// Returns the block box's dimension box.
	Box& GetBox();
	/// Returns the block box's dimension box.
	const Box& GetBox() const;

	void* operator new(size_t size);
	void operator delete(void* chunk, size_t size);

private:
	struct AbsoluteElement {
		Element* element;
		Vector2f position;
	};

	InlineContainer* GetOpenInlineContainer();
	const BlockContainer* GetOpenBlockContainer() const;

	// Closes our last block box, if it is an open inline block box.
	CloseResult CloseInlineBlockBox();

	// Positions a floating element within this block box.
	void PlaceFloat(Element* element, float offset = 0);

	// Checks if we have a new vertical overflow on an auto-scrolling element. If so, our vertical scrollbar will
	// be enabled and our block boxes will be destroyed. All content will need to re-formatted. Returns true if no
	// overflow occured, false if it did.
	bool CatchVerticalOverflow(float cursor = -1);

	// Debug dump layout tree.
	String DumpTree(int depth) const override;

	using AbsoluteElementList = Vector<AbsoluteElement>;
	using BlockBoxList = Vector<UniquePtr<BlockLevelBox>>;

	// The element this box represents. This will be nullptr for boxes rendering in an inline context.
	Element* element;

	// The element we'll be computing our offset relative to during layout.
	const BlockContainer* offset_root;
	// The element this block box's children are to be offset from.
	BlockContainer* offset_parent;

	// The box's block parent. This will be nullptr for the root of the box tree.
	BlockContainer* parent;

	// The block box's position.
	Vector2f position;
	// The block box's size.
	Box box;
	float min_height;
	float max_height;

	// Used by inline contexts only; set to true if the block box's line boxes should stretch to fit their inline content instead of wrapping.
	bool wrap_content;

	// The vertical position of the next block box to be added to this box, relative to the top of this box.
	float box_cursor;

	// Used by block contexts only; stores the list of block boxes under this box.
	BlockBoxList block_boxes;
	// Used by block contexts only; stores any elements that are to be absolutely positioned within this block box.
	AbsoluteElementList absolute_elements;
	// Used by block contexts only; stores any elements that are relatively positioned and whose containing block is this.
	ElementList relative_elements;
	// Used by block contexts only; stores the block box space managing our space, as occupied by floating elements of this box and our ancestors.
	UniquePtr<LayoutBlockBoxSpace> space;
	// Stores any floating elements that are waiting for a line break to be positioned.
	ElementList queued_float_elements;
	// Used by block contexts only; stores an inline element hierarchy that was interrupted by a child block box.
	// The hierarchy will be resumed in an inline-context box once the intervening block box is completed.
	LayoutInlineBox* interrupted_chain;
	// Used by block contexts only; stores the value of the overflow property for the element.
	Style::Overflow overflow_x_property;
	Style::Overflow overflow_y_property;

	// The inner content size (excluding any padding/border/margins).
	// This is extended as child block boxes are closed, or from external formatting contexts.
	Vector2f inner_content_size;

	// Used by block contexts only; if true, we've enabled our vertical scrollbar.
	bool vertical_overflow;
};

class InlineContainer final : public BlockLevelBox {
public:
	/// Creates a new block box in an inline context.
	InlineContainer(BlockContainer* parent, bool wrap_content);

	~InlineContainer();

	/// Closes the box. This will determine the element's height (if it was unspecified).
	/// @param[out] Optionally, output the open inline box.
	/// @return The result of the close; this may request a reformat of this element or our parent.
	CloseResult Close(LayoutInlineBox** out_open_inline_box = nullptr);

	/// Called by a closing line box child. Increments the cursor, and creates a new line box to fit the overflow (if any).
	/// @param child[in] The closing child line box.
	/// @param overflow[in] The overflow from the closing line box. May be nullptr if there was no overflow.
	/// @param overflow_chain[in] The end of the chained hierarchy to be spilled over to the new line, as the parent to the overflow box (if one
	/// exists).
	/// @return If the line box had overflow, this will be the last inline box created by the overflow.
	LayoutInlineBox* CloseLineBox(LayoutLineBox* child, UniquePtr<LayoutInlineBox> overflow, LayoutInlineBox* overflow_chain);

	/// Adds a new inline element to this inline-context box.
	/// @param element[in] The new inline element.
	/// @param box[in] The box defining the element's bounds.
	/// @return The inline box representing the element. Once the element's children have been positioned, Close() must be called on it.
	LayoutInlineBox* AddInlineElement(Element* element, const Box& box);

	/// Add a break to the last line.
	void AddBreak(float line_height);

	/// Adds an inline box for resuming an inline box that has been split.
	/// @param[in] chained_box The box overflowed from a previous line.
	void AddChainedBox(LayoutInlineBox* chained_box);

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

	/// Returns the block box's parent.
	BlockContainer* GetParent() const;

	/// Returns the position of the block box, relative to its parent's content area.
	Vector2f GetPosition() const;

	/// Returns the height of this inline container, including the last line even if it is still open.
	float GetHeightIncludingOpenLine() const;

	/// Get the baseline of the last line.
	/// @param[out] out_baseline
	/// @return True if the baseline was found.
	bool GetBaselineOfLastLine(float& out_baseline) const;

	// Debug dump layout tree.
	String DumpTree(int depth) const override;

	void* operator new(size_t size);
	void operator delete(void* chunk, size_t size);

private:
	using LineBoxList = Vector<UniquePtr<LayoutLineBox>>;

	// The box's block parent. This will be nullptr for the root of the box tree.
	BlockContainer* parent;

	// The block box's position.
	Vector2f position;
	// The block box's size.
	Vector2f box_size;

	// Used by inline contexts only; set to true if the block box's line boxes should stretch to fit their inline content instead of wrapping.
	bool wrap_content;

	// The vertical position of the next block box to be added to this box, relative to the top of this box.
	float box_cursor;

	// Used by inline contexts only; stores the list of line boxes flowing inline content.
	LineBoxList line_boxes;
};

} // namespace Rml
#endif
