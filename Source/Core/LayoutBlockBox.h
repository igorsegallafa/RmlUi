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
	enum class Type { Root, BlockContainer, InlineContainer, FlexContainer, TableWrapper, Replaced };

	Type GetType() const { return type; }

	// Returns the border size of this box including overflowing content. Similar to the scrollable overflow rectangle,
	// but shrinked if overflow is caught here. This can be wider than the box if we are overflowing.
	// @note Only available after the box has been closed.
	Vector2f GetVisibleOverflowSize() const { return visible_overflow_size; }

	// TODO: Do we really want virtual for these?
	virtual const Box* GetBoxPtr() const { return nullptr; }
	virtual bool GetBaselineOfLastLine(float& /*out_baseline*/) const { return false; }

	/// Calculate the dimensions of the box's internal content width; i.e. the size used to calculate the shrink-to-fit width.
	/// @return The inner shrink-to-fit width of this box.
	virtual float GetShrinkToFitWidth() const { return 0.f; }

	// Debug dump layout tree.
	String DumpLayoutTree(int depth = 0) const { return DebugDumpTree(depth); }

	virtual ~LayoutBox() = default;

	void* operator new(size_t size);
	void operator delete(void* chunk, size_t size);

protected:
	LayoutBox(Type type) : type(type) {}

	void SetVisibleOverflowSize(Vector2f size) { visible_overflow_size = size; }

	// Debug dump layout tree.
	virtual String DebugDumpTree(int depth) const = 0;

private:
	Type type;

	Vector2f visible_overflow_size;
};

class ContainerBox : public LayoutBox {
public:
	bool IsScrollContainer() const { return overflow_x != Style::Overflow::Visible || overflow_y != Style::Overflow::Visible; }

	// Determine if this element should have scrollbars or not, and create them if so.
	void ResetScrollbars(const Box& box);

	/// Adds an absolutely positioned element, to be formatted and positioned when closing this container, see 'ClosePositionedElements'.
	void AddAbsoluteElement(Element* element, Vector2f static_position, Element* static_relative_offset_parent);
	/// Adds a relatively positioned descendent which we act as a containing block for.
	void AddRelativeElement(Element* element);

	ContainerBox* GetParent() { return parent_container; }
	Element* GetElement() { return element; }
	Style::Position GetPositionProperty() const { return position_property; }
	bool HasLocalTransformOrPerspective() const { return has_local_transform_or_perspective; }

protected:
	ContainerBox(Type type, Element* element, ContainerBox* parent_container) : LayoutBox(type), element(element), parent_container(parent_container)
	{
		if (element)
		{
			const auto& computed = element->GetComputedValues();
			overflow_x = computed.overflow_x();
			overflow_y = computed.overflow_y();
			position_property = computed.position();
			has_local_transform_or_perspective = (computed.has_local_transform() || computed.has_local_perspective());
		}
	}

	// Checks if we have a new overflow on an auto-scrolling element. If so, our vertical scrollbar will be enabled and
	// our block boxes will be destroyed. All content will need to re-formatted.
	// @returns True if no overflow occured, false if it did.
	bool CatchOverflow(const Vector2f content_size, const Box& box, const float max_height) const;

	/// Set the box and scrollable area on our element, possibly catch any overflow.
	/// @param content_overflow_size The size of the visible content, relative to our content area.
	/// @param box The box to be set on the element.
	/// @param max_height Maximum height of the content area, if any.
	/// @returns True if no overflow occured, false if it did.
	bool SubmitBox(const Vector2f content_overflow_size, const Box& box, const float max_height);

	/// Formats, sizes, and positions all absolute elements whose containing block is this, and offsets relative elements.
	void ClosePositionedElements();

	Element* const element;

private:
	struct AbsoluteElement {
		Vector2f static_position;               // The hypothetical position of the element as if it was placed in normal flow.
		Element* static_position_offset_parent; // The element for which the static position is offset from.
	};
	using AbsoluteElementMap = SmallUnorderedMap<Element*, AbsoluteElement>;

	// Used by block contexts only; stores any elements that are to be absolutely positioned within this block box.
	AbsoluteElementMap absolute_elements;
	// Used by block contexts only; stores any elements that are relatively positioned and whose containing block is this.
	ElementList relative_elements;

	Style::Overflow overflow_x = Style::Overflow::Visible;
	Style::Overflow overflow_y = Style::Overflow::Visible;
	Style::Position position_property = Style::Position::Static;
	bool has_local_transform_or_perspective = false;

	ContainerBox* parent_container = nullptr;
};

class RootBox final : public ContainerBox {
public:
	RootBox(Vector2f containing_block) : ContainerBox(Type::Root, nullptr, nullptr), box(containing_block) {}

	const Box* GetBoxPtr() const override { return &box; }

	String DebugDumpTree(int depth) const override { return String(depth * 2, ' ') + "RootBox"; /* TODO */ }

private:
	Box box;
};

class FlexContainer final : public ContainerBox {
public:
	FlexContainer(Element* element, ContainerBox* parent_container) : ContainerBox(Type::FlexContainer, element, parent_container)
	{
		RMLUI_ASSERT(element);
	}

	String DebugDumpTree(int depth) const override { return String(depth * 2, ' ') + "FlexContainer"; /* TODO */ }

	bool Close(const Vector2f content_overflow_size, const Box& box)
	{
		if (!SubmitBox(content_overflow_size, box, -1.f))
			return false;

		ClosePositionedElements();
		return true;
	}

	const Box* GetBoxPtr() const override { return &box; }

	Box& GetBox() { return box; }

private:
	Box box;
};

class TableWrapper final : public ContainerBox {
public:
	TableWrapper(Element* element, ContainerBox* parent_container) : ContainerBox(Type::TableWrapper, element, parent_container)
	{
		RMLUI_ASSERT(element);
	}

	String DebugDumpTree(int depth) const override { return String(depth * 2, ' ') + "TableWrapper"; /* TODO */ }

	void Close(const Vector2f content_overflow_size, const Box& box)
	{
		bool result = SubmitBox(content_overflow_size, box, -1.f);

		// Since the table wrapper cannot generate scrollbars, this should always pass.
		RMLUI_ASSERT(result);
		(void)result;

		ClosePositionedElements();
	}

	const Box* GetBoxPtr() const override { return &box; }

	Box& GetBox() { return box; }

private:
	Box box;
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
	BlockContainer(ContainerBox* parent_container, LayoutBlockBoxSpace* space, Element* element, const Box& box, float min_height, float max_height);
	/// Releases the block box.
	~BlockContainer();

	/// Closes the box. This will determine the element's height (if it was unspecified).
	/// @param parent_block_box Our parent which will be sized to contain this box, or nullptr for the root of the block formatting context.
	/// @return False if the block box caused an automatic vertical scrollbar to appear, forcing a reformat of the current block formatting context.
	bool Close(BlockContainer* parent_block_container);

	/// Called by a closing block box child. Increments the cursor.
	/// @param[in] child The closing child block-level box.
	/// @param[in] child_position The border position of the child, relative to the current block formatting context.
	/// @param[in] child_size The border size of the child.
	/// @param[in] child_margin_bottom The bottom margin width of the child.
	/// @return False if the block box caused an automatic vertical scrollbar to appear, forcing a reformat of the current block formatting context.
	/// TODO: Can we simplify this? Rename? This is more like increment box cursor and enlarge content size, and only needed for block-level boxes.
	bool CloseChildBox(LayoutBox* child, Vector2f child_position, float child_height, float child_margin_bottom);

	/// Creates a new block box and adds it as a child of this one.
	/// @param element[in] The new block element.
	/// @param box[in] The box used for the new block box.
	/// @param min_height[in] The minimum height of the content box.
	/// @param max_height[in] The maximum height of the content box.
	/// @return The block box representing the element. Once the element's children have been positioned, Close() must be called on it.
	BlockContainer* AddBlockBox(Element* element, const Box& box, float min_height, float max_height);

	LayoutBox* AddBlockLevelBox(UniquePtr<LayoutBox> block_level_box, Element* element, const Box& box);

	/// Adds an element to this block box to be handled as a floating element.
	void AddFloatElement(Element* element, Vector2f visible_overflow_size);

	struct InlineBoxHandle {
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

	// Estimate the static position of a hypothetical next element to be placed.
	Vector2f GetOpenStaticPosition(Style::Display display) const;

	/// Returns the offset of a new child box to be placed here.
	/// @return The next box border position in the block formatting context space.
	Vector2f NextBoxPosition() const;
	/// Returns the offset of a new child box to be placed here. Collapses adjacent margins and optionally clears floats.
	/// @param[in] child_box The dimensions of the new box.
	/// @param[in] clear_property The value of the underlying element's clear property.
	/// @return The next border position in the block formatting context space.
	Vector2f NextBoxPosition(const Box& child_box, Style::Clear clear_property) const;

	// Places all queued floating elements.
	void PlaceQueuedFloats(float vertical_position);

	float GetShrinkToFitWidth() const override;

	// Reset this box, so that it can be formatted again.
	void ResetContents();

	/// Returns the block box's element.
	Element* GetElement() const;

	/// Returns the block box space.
	const LayoutBlockBoxSpace* GetBlockBoxSpace() const;

	/// Returns the position of the block box, relative to its parent's content area.
	/// @return The relative position of the block box.
	Vector2f GetPosition() const;

	Box& GetBox();
	const Box& GetBox() const;

	const Box* GetBoxPtr() const override { return &box; }

private:
	InlineContainer* EnsureOpenInlineContainer();
	InlineContainer* GetOpenInlineContainer();
	const InlineContainer* GetOpenInlineContainer() const;

	const LayoutBox* GetOpenLayoutBox() const;

	// Closes the inline container if there is one open. Returns false if our formatting context needs to be reformatted.
	bool CloseOpenInlineContainer();

	void ResetInterruptedLineBox();

	// Positions a floating element within this block box.
	void PlaceFloat(Element* element, float vertical_position, Vector2f visible_overflow_size);

	// Return the baseline of the last line box of this or any descendant inline-level boxes.
	bool GetBaselineOfLastLine(float& out_baseline) const override;

	// Debug dump layout tree.
	String DebugDumpTree(int depth) const override;

	using BlockBoxList = Vector<UniquePtr<LayoutBox>>;
	struct QueuedFloat {
		Element* element;
		Vector2f visible_overflow_size;
	};
	using QueuedFloatList = Vector<QueuedFloat>;

	// The block box's position.
	Vector2f position;
	// The block box's size.
	Box box;
	float min_height = 0.f;
	float max_height = -1.f;

	// Used by inline contexts only; set to true if the block box's line boxes should stretch to fit their inline content instead of wrapping.
	bool wrap_content = true;

	// The vertical position of the next block box to be added to this box, relative to the top of our content box.
	float box_cursor = 0.f;

	// TODO: All comments in the following.

	UniquePtr<LayoutBlockBoxSpace> root_space;
	// Used by block contexts only; stores the block box space managing our space, as occupied by floating elements of this box and our ancestors.
	LayoutBlockBoxSpace* space;
	// Used by block contexts only; stores the list of block boxes under this box.
	BlockBoxList block_boxes;
	// Stores any floating elements that are waiting for a line break to be positioned.
	QueuedFloatList queued_float_elements;
	// Used by block contexts only; stores an inline element hierarchy that was interrupted by a child block box.
	// The hierarchy will be resumed in an inline-context box once the intervening block box is completed.
	UniquePtr<LayoutLineBox> interrupted_line_box;

	// The inner content size (excluding any padding/border/margins).
	// This is extended as child block boxes are closed, or from external formatting contexts.
	Vector2f inner_content_size;
};

} // namespace Rml
#endif
