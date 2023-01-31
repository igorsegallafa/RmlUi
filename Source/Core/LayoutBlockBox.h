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
class InlineBox;
class InlineContainer;

class LayoutBox {
public:
	enum class Type { BlockContainer, InlineContainer, FlexContainer, TableWrapper, Replaced };
	enum class OuterType { BlockLevel, InlineLevel };
	enum class CloseResult { OK, LayoutSelf, LayoutParent };

	Type GetType() const { return type; }

	// Debug dump layout tree.
	String DumpLayoutTree(int depth = 0) const { return DebugDumpTree(depth); }

	virtual ~LayoutBox() = default;

	void* operator new(size_t size);
	void operator delete(void* chunk, size_t size);

protected:
	LayoutBox(OuterType outer_type, Type type) : outer_type(outer_type), type(type) {}

	// Debug dump layout tree.
	virtual String DebugDumpTree(int depth) const = 0;

private:
	OuterType outer_type;
	Type type;
};

class BlockLevelBox : public LayoutBox {
public:
	// Returns the outer size of this box including overflowing content. Similar to scroll width, but shrinked if
	// overflow is caught here. This can be wider than the box if we are overflowing.
	// @note Only available after the box has been closed.
	Vector2f GetVisibleOverflowSize() const { return visible_overflow_size; }

	virtual bool GetBaselineOfLastLine(float& /*out_baseline*/) const { return false; }

protected:
	BlockLevelBox(Type type) : LayoutBox(OuterType::BlockLevel, type) {}

	void SetVisibleOverflowSize(Vector2f size) { visible_overflow_size = size; }

private:
	Vector2f visible_overflow_size;
};

// class InlineLevelBox : public LayoutBox {
// public:
// protected:
//	InlineLevelBox(Type type) : LayoutBox(OuterType::InlineLevel, type) {}
// };
//
// class FormattingContext {
// public:
// private:
//	// Contains absolute elements.
// };
//
// class BlockFlexContainer : public BlockLevelBox {
// public:
//	BlockFlexContainer() : BlockLevelBox(Type::FlexContainer) {}
//
// private:
// };
// class InlineFlexContainer : public InlineLevelBox {
// public:
//	InlineFlexContainer() : InlineLevelBox(Type::FlexContainer) {}
//
// private:
// };

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
	bool CloseChildBox(BlockLevelBox* child, float child_position_top, float child_size_y);

	/// Adds a new block element to this block-context box.
	/// @param element[in] The new block element.
	/// @param box[in] The box used for the new block box.
	/// @param min_height[in] The minimum height of the content box.
	/// @param max_height[in] The maximum height of the content box.
	/// @return The block box representing the element. Once the element's children have been positioned, Close() must be called on it.
	BlockContainer* AddBlockElement(Element* element, const Box& box, float min_height, float max_height);

	struct InlineBoxHandle {
		InlineContainer* inline_container;
		InlineBox* inline_box;
	};

	/// Adds a new inline element to this inline-context box.
	/// @param element[in] The new inline element.
	/// @param box[in] The box defining the element's bounds.
	/// @return The inline box representing the element. Once the element's children have been positioned, Close() must be called on it.
	InlineBoxHandle AddInlineElement(Element* element, const Box& box);
	// TODO
	void CloseInlineElement(InlineBoxHandle handle);

	/// Adds a line-break to this block box.
	void AddBreak();

	/// Adds an element to this block box to be handled as a floating element.
	void AddFloatElement(Element* element);

	/// Adds an element to this block box to be handled as an absolutely-positioned element. This element will be
	/// laid out, sized and positioned appropriately once this box is finished. This should only be called on boxes
	/// rendering in a block-context.
	/// @param element[in] The element to be positioned absolutely within this block box.
	void AddAbsoluteElement(Element* element);
	/// Formats, sizes, and positions all absolute elements in this block.
	void CloseAbsoluteElements();

	/// Adds relatively positioned descendents which we act as a containing block for.
	void AddRelativeElements(ElementList&& elements);

	/// Returns the offset from the top-left corner of this box's offset element the next child box will be positioned at.
	/// @param[in] top_margin The top margin of the box. This will be collapsed as appropriate against other block boxes.
	/// @param[in] clear_property The value of the underlying element's clear property.
	/// @return The box cursor position.
	/// TODO/note: Returns the position in root-relative coordinates.
	Vector2f NextBoxPosition(float top_margin = 0, Style::Clear clear_property = Style::Clear::None) const;
	/// Returns the offset from the top-left corner of this box's offset element the next child block box, of the given dimensions,
	/// will be positioned at. This will include the margins on the new block box.
	/// @param[in] box The dimensions of the new box.
	/// @param[in] clear_property The value of the underlying element's clear property.
	/// @return The block box cursor position.
	/// TODO/note: Returns the position in root-relative coordinates.
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
	const BlockContainer* GetParent() const;

	/// Returns the block box space.
	const LayoutBlockBoxSpace* GetBlockBoxSpace() const;

	/// Returns the position of the block box, relative to its parent's content area.
	/// @return The relative position of the block box.
	Vector2f GetPosition() const;

	/// Returns the block box against which all positions of boxes in the hierarchy are set relative to.
	const BlockContainer* GetOffsetParent() const;
	/// Returns the block box against which all positions of boxes in the hierarchy are calculated relative to.
	// The element we'll be computing our offset relative to during layout.
	const BlockContainer* GetOffsetRoot() const;

	Box& GetBox();
	const Box& GetBox() const;

private:
	struct AbsoluteElement {
		Element* element;
		Vector2f position;
	};

	InlineContainer* GetOpenInlineContainer();
	InlineContainer* EnsureOpenInlineContainer();
	const BlockContainer* GetOpenBlockContainer() const;

	// Closes our last block box, if it is an open inline block box.
	CloseResult CloseInlineBlockBox();

	void ResetInterruptedLineBox();

	// Positions a floating element within this block box.
	void PlaceFloat(Element* element, float offset = 0);

	// Checks if we have a new vertical overflow on an auto-scrolling element. If so, our vertical scrollbar will
	// be enabled and our block boxes will be destroyed. All content will need to re-formatted. Returns true if no
	// overflow occured, false if it did.
	bool CatchVerticalOverflow(float cursor = -1);

	// Return the baseline of the last line box of this or any descendant inline-level boxes.
	bool GetBaselineOfLastLine(float& out_baseline) const override;

	// Debug dump layout tree.
	String DebugDumpTree(int depth) const override;

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

	// The vertical position of the next block box to be added to this box, relative to the top of our content box.
	float box_cursor;

	// TODO: All comments in the following.

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
	UniquePtr<LayoutLineBox> interrupted_line_box;
	// Used by block contexts only; stores the value of the overflow property for the element.
	Style::Overflow overflow_x_property;
	Style::Overflow overflow_y_property;

	// The inner content size (excluding any padding/border/margins).
	// This is extended as child block boxes are closed, or from external formatting contexts.
	Vector2f inner_content_size;

	// Used by block contexts only; if true, we've enabled our vertical scrollbar.
	bool vertical_overflow;
};

} // namespace Rml
#endif
