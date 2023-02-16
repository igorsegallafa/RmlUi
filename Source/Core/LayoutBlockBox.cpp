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

#include "LayoutBlockBox.h"
#include "../../Include/RmlUi/Core/ComputedValues.h"
#include "../../Include/RmlUi/Core/Element.h"
#include "../../Include/RmlUi/Core/ElementScroll.h"
#include "../../Include/RmlUi/Core/ElementUtilities.h"
#include "../../Include/RmlUi/Core/Profiling.h"
#include "../../Include/RmlUi/Core/Property.h"
#include "LayoutBlockBoxSpace.h"
#include "LayoutDetails.h"
#include "LayoutEngine.h"
#include "LayoutInlineContainer.h"

namespace Rml {

void* LayoutBox::operator new(size_t size)
{
	void* memory = LayoutEngine::AllocateLayoutChunk(size);
	return memory;
}

void LayoutBox::operator delete(void* chunk, size_t size)
{
	LayoutEngine::DeallocateLayoutChunk(chunk, size);
}

void ContainerBox::ResetScrollbars(const Box& box)
{
	RMLUI_ASSERT(element);
	if (overflow_x == Style::Overflow::Scroll)
		element->GetElementScroll()->EnableScrollbar(ElementScroll::HORIZONTAL, box.GetSizeAcross(Box::HORIZONTAL, Box::PADDING));
	else
		element->GetElementScroll()->DisableScrollbar(ElementScroll::HORIZONTAL);

	if (overflow_y == Style::Overflow::Scroll)
		element->GetElementScroll()->EnableScrollbar(ElementScroll::VERTICAL, box.GetSizeAcross(Box::HORIZONTAL, Box::PADDING));
	else
		element->GetElementScroll()->DisableScrollbar(ElementScroll::VERTICAL);
}

void ContainerBox::AddAbsoluteElement(Element* element, Vector2f static_position, Element* static_relative_offset_parent)
{
	absolute_elements.push_back(AbsoluteElement{element, static_position, static_relative_offset_parent});
}

void ContainerBox::AddRelativeElement(Element* element)
{
	relative_elements.push_back(element);
}

void ContainerBox::ClosePositionedElements()
{
	// Note: Indexed iteration must be used here, as new absolute elements may be added to this box during each
	// iteration while the element is formatted, thereby invalidating references and iterators of 'absolute_elements'.
	for (int i = 0; i < (int)absolute_elements.size(); i++)
	{
		Element* absolute_element = absolute_elements[i].element;
		const Vector2f static_position = absolute_elements[i].static_position;
		Element* static_position_offset_parent = absolute_elements[i].static_position_offset_parent;

		// Find the static position relative to this containing block. First, calculate the offset from ourself to the
		// static position's offset parent. Assumes (1) that this container box is part of the containing block chain of
		// the static position offset parent, and (2) that all offsets in this chain has been set already.
		Vector2f relative_position;
		for (Element* ancestor = static_position_offset_parent; ancestor && ancestor != element; ancestor = ancestor->GetOffsetParent())
			relative_position += ancestor->GetRelativeOffset(Box::BORDER);

		// Now simply add the result to the stored static position to get the static position in our local space.
		Vector2f offset = relative_position + static_position;

		// Lay out the element.
		auto formatting_context = FormattingContext::ConditionallyCreateIndependentFormattingContext(this, absolute_element);
		RMLUI_ASSERTMSG(formatting_context, "Absolutely positioned elements should always generate an independent formatting context");
		formatting_context->Format({});

		// Now that the element's box has been built, we can offset the position we determined was appropriate for it by
		// the element's margin. This is necessary because the coordinate system for the box begins at the border, not
		// the margin.
		offset.x += absolute_element->GetBox().GetEdge(Box::MARGIN, Box::LEFT);
		offset.y += absolute_element->GetBox().GetEdge(Box::MARGIN, Box::TOP);

		// Set the offset of the element; the element itself will take care of any RCSS-defined positional offsets.
		absolute_element->SetOffset(offset, element);
	}

	absolute_elements.clear();

	// Any relatively positioned elements that we act as containing block for may also need to be have their positions
	// updated to reflect changes to the size of this block box.
	// TODO: Maybe do this first, in case the static position of absolutely positioned elements depends on any of these relative positions.
	for (Element* child : relative_elements)
		child->UpdateOffset();

	relative_elements.clear();
}

void ContainerBox::ClearPositionedElements()
{
	absolute_elements.clear();
	relative_elements.clear();
}

bool ContainerBox::CatchOverflow(const Vector2f content_size, const Box& box, const float max_height) const
{
	if (!IsScrollContainer())
		return true;

	const Vector2f padding_bottom_right = {box.GetEdge(Box::PADDING, Box::RIGHT), box.GetEdge(Box::PADDING, Box::BOTTOM)};
	const float padding_width = box.GetSizeAcross(Box::HORIZONTAL, Box::PADDING);

	Vector2f available_space = box.GetSize();
	if (available_space.y < 0.f)
		available_space.y = max_height;
	if (available_space.y < 0.f)
		available_space.y = INFINITY;

	RMLUI_ASSERT(available_space.x >= 0.f && available_space.y >= 0.f);

	// Allow overflow onto the padding area.
	available_space += padding_bottom_right;

	ElementScroll* element_scroll = element->GetElementScroll();
	bool scrollbar_size_changed = false;

	// @performance If we have auto-height sizing and the horizontal scrollbar is enabled, then we can in principle
	// simply add the scrollbar size to the height instead of formatting the element all over again.
	if (overflow_x == Style::Overflow::Auto && content_size.x > available_space.x + 0.5f)
	{
		if (element_scroll->GetScrollbarSize(ElementScroll::HORIZONTAL) == 0.f)
		{
			element_scroll->EnableScrollbar(ElementScroll::HORIZONTAL, padding_width);
			const float new_size = element_scroll->GetScrollbarSize(ElementScroll::HORIZONTAL);
			scrollbar_size_changed = (new_size != 0.f);
			available_space.y -= new_size;
		}
	}

	// If we're auto-scrolling and our height is fixed, we have to check if this box has exceeded our client height.
	if (overflow_y == Style::Overflow::Auto && content_size.y > available_space.y + 0.5f)
	{
		if (element_scroll->GetScrollbarSize(ElementScroll::VERTICAL) == 0.f)
		{
			element_scroll->EnableScrollbar(ElementScroll::VERTICAL, padding_width);
			const float new_size = element_scroll->GetScrollbarSize(ElementScroll::VERTICAL);
			scrollbar_size_changed |= (new_size != 0.f);
		}
	}

	return !scrollbar_size_changed;
}

bool ContainerBox::SubmitBox(const Vector2f content_overflow_size, const Box& box, const float max_height)
{
	// TODO: Properly compute the visible overflow size / scrollable overflow rectangle.
	//       https://www.w3.org/TR/css-overflow-3/#scrollable
	//
	Vector2f visible_overflow_size;

	// Set the computed box on the element.
	if (element)
	{
		// Calculate the dimensions of the box's scrollable overflow rectangle. This is the union of the tightest-
		// fitting box around all of the internal elements, and this element's padding box. We really only care about
		// overflow on the bottom-right sides, as these are the only ones allowed to be scrolled to in CSS.
		//
		// If we are a scroll container (use any other value than 'overflow: visible'), then any overflow outside our
		// padding box should be caught here. Otherwise, our overflow should be included in the overflow calculations of
		// our nearest scroll container ancestor.

		// If our content is larger than our padding box, we can add scrollbars if we're set to auto-scrollbars. If
		// we're set to always use scrollbars, then the scrollbars have already been enabled.
		if (!CatchOverflow(content_overflow_size, box, max_height))
			return false;

		const Vector2f padding_top_left = {box.GetEdge(Box::PADDING, Box::LEFT), box.GetEdge(Box::PADDING, Box::TOP)};
		const Vector2f padding_bottom_right = {box.GetEdge(Box::PADDING, Box::RIGHT), box.GetEdge(Box::PADDING, Box::BOTTOM)};
		const Vector2f padding_size = box.GetSize() + padding_top_left + padding_bottom_right;

		const bool is_scroll_container = IsScrollContainer();
		const Vector2f scrollbar_size = {
			is_scroll_container ? element->GetElementScroll()->GetScrollbarSize(ElementScroll::VERTICAL) : 0.f,
			is_scroll_container ? element->GetElementScroll()->GetScrollbarSize(ElementScroll::HORIZONTAL) : 0.f,
		};
		const Vector2f scrollable_overflow_size = Math::Max(padding_size - scrollbar_size, padding_top_left + content_overflow_size);

		element->SetBox(box);
		element->SetScrollableOverflowRectangle(scrollable_overflow_size);

		const Vector2f border_size = padding_size + box.GetSizeAround(Box::BORDER, Box::BORDER);

		// Set the visible overflow size so that ancestors can catch any overflow produced by us. That is, hiding it or
		// providing a scrolling mechanism. If this box is a scroll container, we catch our own overflow here; then,
		// just use the normal margin box as that will effectively remove the overflow from our ancestor's perspective.
		if (is_scroll_container)
		{
			visible_overflow_size = border_size;

			// Format any scrollbars in case they were enabled on this element.
			element->GetElementScroll()->FormatScrollbars();
		}
		else
		{
			const Vector2f border_top_left = {box.GetEdge(Box::BORDER, Box::LEFT), box.GetEdge(Box::BORDER, Box::TOP)};
			visible_overflow_size = Math::Max(border_size, content_overflow_size + border_top_left + padding_top_left);
		}
	}

	SetVisibleOverflowSize(visible_overflow_size);

	return true;
}

BlockContainer::BlockContainer(ContainerBox* _parent_container, LayoutBlockBoxSpace* space, Element* _element, const Box& _box, float _min_height,
	float _max_height) :
	ContainerBox(Type::BlockContainer, _element, _parent_container),
	box(_box), min_height(_min_height), max_height(_max_height), space(space)
{
	RMLUI_ASSERT(element);
	wrap_content = (element->GetComputedValues().white_space() != Style::WhiteSpace::Nowrap);
}

BlockContainer::~BlockContainer() {}

bool BlockContainer::Close(BlockContainer* parent_block_container)
{
	// If the last child of this block box is an inline box, then we haven't closed it; close it now!
	if (!CloseOpenInlineContainer())
		return false;

	// Set this box's height, if necessary.
	if (box.GetSize().y < 0)
	{
		float content_height = box_cursor;

		if (!parent_block_container)
			content_height = Math::Max(content_height, space->GetDimensions(LayoutFloatBoxEdge::Margin).y - (position.y + box.GetPosition().y));

		content_height = Math::Clamp(content_height, min_height, max_height);
		box.SetContent({box.GetSize().x, content_height});
	}

	// Check how big our floated area is.
	const Vector2f space_box = space->GetDimensions(LayoutFloatBoxEdge::Overflow) - (position + box.GetPosition());

	// Start with the inner content size, as set by the child block boxes or external formatting contexts.
	Vector2f content_box = Math::Max(inner_content_size, space_box);
	content_box.y = Math::Max(content_box.y, box_cursor);

	if (!SubmitBox(content_box, box, max_height))
		return false;

	// Increment the parent's cursor.
	if (parent_block_container)
	{
		RMLUI_ASSERTMSG(GetParent() == parent_block_container, "Mismatched parent box.");

		// If this close fails, it means this block box has caused our parent block box to generate an automatic vertical scrollbar.
		if (!parent_block_container->CloseChildBox(this, position, box.GetSize(Box::BORDER), box.GetEdge(Box::MARGIN, Box::BOTTOM)))
			return false;
	}

	// Now that we have been sized, we can proceed with formatting and placing positioned elements that this container
	// acts as a containing block for.
	ClosePositionedElements();

	// Find the element baseline which is the distance from the margin bottom of the element to its baseline.
	float element_baseline = 0;

	// For inline-blocks with visible overflow, this is the baseline of the last line of the element (see CSS2 10.8.1).
	if (element->GetDisplay() == Style::Display::InlineBlock && !IsScrollContainer())
	{
		float baseline = 0;
		bool found_baseline = GetBaselineOfLastLine(baseline);

		// The retrieved baseline is the vertical distance from the top of our root space (the coordinate system of our
		// local block formatting context), convert it to the element's local coordinates.
		if (found_baseline)
		{
			const float bottom_position = position.y + box.GetSizeAcross(Box::VERTICAL, Box::BORDER) + box.GetEdge(Box::MARGIN, Box::BOTTOM);
			element_baseline = bottom_position - baseline;
		}
	}

	element->SetBaseline(element_baseline);

	ResetInterruptedLineBox();

	return true;
}

bool BlockContainer::CloseChildBox(LayoutBox* child, Vector2f child_position, Vector2f child_size, float child_margin_bottom)
{
	child_position -= (box.GetPosition() + position);

	box_cursor = child_position.y + child_size.y + child_margin_bottom;

	// Extend the inner content size. The vertical size can be larger than the box_cursor due to overflow.
	inner_content_size = Math::Max(inner_content_size, child_position + child->GetVisibleOverflowSize());

	const Vector2f content_size = Math::Max(Vector2f{box.GetSize().x, box_cursor}, inner_content_size);

	const bool result = CatchOverflow(content_size, box, max_height);

	return result;
}

BlockContainer* BlockContainer::AddBlockBox(Element* child_element, const Box& box, float min_height, float max_height)
{
	if (!CloseOpenInlineContainer())
		return nullptr;

	auto child_container_ptr = MakeUnique<BlockContainer>(this, space, child_element, box, min_height, max_height);
	BlockContainer* child_container = child_container_ptr.get();

	child_container->position = NextBoxPosition(box, child_element->GetComputedValues().clear());
	child_element->SetOffset(child_container->position - position, element);

	child_container->ResetScrollbars(box);

	// Store relatively positioned elements with their containing block so that their offset can be updated after
	// their containing block has been sized.
	if (child_element->GetPosition() == Style::Position::Relative)
		AddRelativeElement(child_element);

	block_boxes.push_back(std::move(child_container_ptr));

	return child_container;
}

LayoutBox* BlockContainer::AddBlockLevelBox(UniquePtr<LayoutBox> block_level_box_ptr, Element* child_element, const Box& box)
{
	RMLUI_ASSERT(box.GetSize().y >= 0.f); // Assumes child element already formatted and sized.

	// TODO: Most of this is essentially the same as above in 'AddBlockBox'.
	if (!CloseOpenInlineContainer())
		return nullptr;

	// Always clear any floats to avoid overlap here. In CSS, it is allowed to instead shrink the box and place it next
	// to any floats, but we keep it simple here for now and just clear them.
	Vector2f child_position = NextBoxPosition(box, Style::Clear::Both);

	child_element->SetOffset(child_position - position, element);

	if (child_element->GetPosition() == Style::Position::Relative)
		AddRelativeElement(child_element);

	LayoutBox* block_level_box = block_level_box_ptr.get();
	block_boxes.push_back(std::move(block_level_box_ptr));

	if (!CloseChildBox(block_level_box, child_position, box.GetSize(Box::BORDER), box.GetEdge(Box::MARGIN, Box::BOTTOM)))
		return nullptr;

	return block_level_box;
}

BlockContainer::InlineBoxHandle BlockContainer::AddInlineElement(Element* element, const Box& box)
{
	RMLUI_ZoneScoped;

	// Inline-level elements need to be added to an inline container, open one if needed.
	InlineContainer* inline_container = EnsureOpenInlineContainer();

	InlineBox* inline_box = inline_container->AddInlineElement(element, box);

	if (element->GetPosition() == Style::Position::Relative)
		AddRelativeElement(element);

	return {inline_box};
}

void BlockContainer::CloseInlineElement(InlineBoxHandle handle)
{
	// If the inline-level element did not generate an inline box, then there is no need to close anything.
	if (!handle.inline_box)
		return;

	// Usually the inline container the box was placed in is still the open box, and we can just close the inline
	// element in it. However, it is possible that an intermediary block-level element was placed, thereby splitting the
	// inline element into multiple inline containers around the block-level box. If we don't have an open inline
	// container at all, open a new one, even if the sole purpose of the new line is to close this inline element.
	EnsureOpenInlineContainer()->CloseInlineElement(handle.inline_box);
}

void BlockContainer::AddBreak()
{
	const float line_height = element->GetLineHeight();

	// Check for an inline box as our last child; if so, we can simply end its line and bail.
	if (InlineContainer* inline_container = GetOpenInlineContainer())
	{
		inline_container->AddBreak(line_height);
		return;
	}

	// No inline box as our last child; no problem, just increment the cursor by the line height of this element.
	box_cursor += line_height;
}

void BlockContainer::AddFloatElement(Element* element, Vector2f visible_overflow_size)
{
	if (InlineContainer* inline_container = GetOpenInlineContainer())
	{
		// Try to add the float to our inline container, placing it next to any open line if possible. Otherwise, queue it for later.
		bool float_placed = false;
		float line_position_top = 0.f;
		Vector2f line_size;
		if (queued_float_elements.empty() && inline_container->GetOpenLineBoxDimensions(line_position_top, line_size))
		{
			const Vector2f margin_size = element->GetBox().GetSize(Box::MARGIN);
			const Style::Float float_property = element->GetComputedValues().float_();
			const Style::Clear clear_property = element->GetComputedValues().clear();

			float available_width = 0.f;
			const Vector2f float_position =
				space->NextFloatPosition(this, available_width, line_position_top, margin_size, float_property, clear_property);

			const float line_position_bottom = line_position_top + line_size.y;
			const float line_and_element_width = margin_size.x + line_size.x;

			// If the float can be positioned on the open line, and it can fit next to the line's contents, place it now.
			if (float_position.y < line_position_bottom && line_and_element_width <= available_width)
			{
				PlaceFloat(element, line_position_top, visible_overflow_size);
				inline_container->UpdateOpenLineBoxPlacement();
				float_placed = true;
			}
		}

		if (!float_placed)
			queued_float_elements.push_back({element, visible_overflow_size});
	}
	else
	{
		// There is no inline container, so just place it!
		const Vector2f box_position = NextBoxPosition();
		PlaceFloat(element, box_position.y, visible_overflow_size);
	}

	if (element->GetPosition() == Style::Position::Relative)
		AddRelativeElement(element);
}

Vector2f BlockContainer::GetOpenStaticPosition(Style::Display display) const
{
	// Estimate the next box as if it had static position (10.6.4). If the element is inline-level, position it on the
	// open line if we have one. Otherwise, block-level elements are positioned on a hypothetical next line.
	Vector2f static_position = NextBoxPosition();

	if (const InlineContainer* inline_container = GetOpenInlineContainer())
	{
		const bool inline_level_element = (display == Style::Display::Inline || display == Style::Display::InlineBlock);
		static_position += inline_container->GetStaticPositionEstimate(inline_level_element);
	}

	return static_position;
}

Vector2f BlockContainer::NextBoxPosition() const
{
	Vector2f box_position = position + box.GetPosition();
	box_position.y += box_cursor;
	return box_position;
}

Vector2f BlockContainer::NextBoxPosition(const Box& child_box, Style::Clear clear_property) const
{
	const float child_top_margin = child_box.GetEdge(Box::MARGIN, Box::TOP);

	Vector2f box_position = NextBoxPosition();

	box_position.x += child_box.GetEdge(Box::MARGIN, Box::LEFT);
	box_position.y += child_top_margin;

	float clear_margin = space->DetermineClearPosition(box_position.y, clear_property) - box_position.y;
	if (clear_margin > 0.f)
	{
		box_position.y += clear_margin;
	}
	else if (const LayoutBox* block_box = GetOpenLayoutBox())
	{
		// Check for a collapsing vertical margin with our last child, which will be vertically adjacent to the new box.
		if (const Box* open_box = block_box->GetBoxPtr())
		{
			const float open_bottom_margin = open_box->GetEdge(Box::MARGIN, Box::BOTTOM);
			const float margin_sum = open_bottom_margin + child_top_margin;

			// Find and add the collapsed margin to the position, then subtract the sum of the margins which have already been added.
			const int num_negative_margins = int(child_top_margin < 0.f) + int(open_bottom_margin < 0.f);
			switch (num_negative_margins)
			{
			case 0: box_position.y += Math::Max(child_top_margin, open_bottom_margin) - margin_sum; break; // Use the largest margin.
			case 1: break; // Use the sum of the positive and negative margin. These are already added to the position, so do nothing.
			case 2: box_position.y += Math::Min(child_top_margin, open_bottom_margin) - margin_sum; break; // Use the most negative margin.
			}
		}
	}

	return box_position;
}

void BlockContainer::PlaceQueuedFloats(float vertical_position)
{
	if (!queued_float_elements.empty())
	{
		for (QueuedFloat entry : queued_float_elements)
			PlaceFloat(entry.element, vertical_position, entry.visible_overflow_size);

		queued_float_elements.clear();
	}
}

float BlockContainer::GetShrinkToFitWidth() const
{
	float content_width = 0.0f;

	auto get_content_width_from_children = [this, &content_width]() {
		for (size_t i = 0; i < block_boxes.size(); i++)
		{
			// TODO: Bad design. Doesn't account for all types. Use virtual?
			if (block_boxes[i]->GetType() == Type::BlockContainer)
			{
				BlockContainer* block_child = static_cast<BlockContainer*>(block_boxes[i].get());
				const Box& box = block_child->GetBox();
				const float edge_size = box.GetSizeAcross(Box::HORIZONTAL, Box::MARGIN, Box::PADDING);
				content_width = Math::Max(content_width, block_child->GetShrinkToFitWidth() + edge_size);
			}
			else if (block_boxes[i]->GetType() == Type::InlineContainer)
			{
				InlineContainer* block_child = static_cast<InlineContainer*>(block_boxes[i].get());
				content_width = Math::Max(content_width, block_child->GetShrinkToFitWidth());
			}
			else if (const Box* box = block_boxes[i]->GetBoxPtr())
			{
				// TODO: For unknown types (e.g. tables, flexboxes), we add spacing for the edges at least, but can be improved.
				const float edge_size = box->GetSizeAcross(Box::HORIZONTAL, Box::MARGIN, Box::PADDING);
				content_width = Math::Max(content_width, edge_size);
			}
		}
	};

	// Block boxes with definite sizes should use that size. Otherwise, find the maximum content width of our children.
	//  Alternative solution: Add some 'intrinsic_width' property to every 'BlockContainer' and have that propagate up.
	auto& computed = element->GetComputedValues();
	const float block_width = box.GetSize(Box::CONTENT).x;

	if (computed.width().type == Style::Width::Auto)
	{
		get_content_width_from_children();
	}
	else
	{
		float width_value = ResolveValue(computed.width(), block_width);
		content_width = Math::Max(content_width, width_value);
	}

	float min_width, max_width;
	LayoutDetails::GetMinMaxWidth(min_width, max_width, computed, box, block_width);
	content_width = Math::Clamp(content_width, min_width, max_width);

	// Can add the dimensions of floating elements here if we want to support that.

	return content_width;
}

Element* BlockContainer::GetElement() const
{
	return element;
}

const LayoutBlockBoxSpace* BlockContainer::GetBlockBoxSpace() const
{
	return space;
}

Vector2f BlockContainer::GetPosition() const
{
	return position;
}

Box& BlockContainer::GetBox()
{
	return box;
}

const Box& BlockContainer::GetBox() const
{
	return box;
}

void BlockContainer::ResetContents()
{
	RMLUI_ZoneScopedC(0xDD3322);

	block_boxes.clear();
	queued_float_elements.clear();

	box_cursor = 0;
	interrupted_line_box.reset();

	inner_content_size = {};
}

String BlockContainer::DebugDumpTree(int depth) const
{
	String value = String(depth * 2, ' ') + "BlockContainer" + " | " + LayoutDetails::GetDebugElementName(element) + '\n';

	for (auto&& block_box : block_boxes)
		value += block_box->DumpLayoutTree(depth + 1);

	return value;
}

InlineContainer* BlockContainer::GetOpenInlineContainer()
{
	return const_cast<InlineContainer*>(static_cast<const BlockContainer&>(*this).GetOpenInlineContainer());
}

const InlineContainer* BlockContainer::GetOpenInlineContainer() const
{
	if (!block_boxes.empty() && block_boxes.back()->GetType() == Type::InlineContainer)
		return static_cast<InlineContainer*>(block_boxes.back().get());
	return nullptr;
}

InlineContainer* BlockContainer::EnsureOpenInlineContainer()
{
	// First check to see if we already have an open inline container.
	InlineContainer* inline_container = GetOpenInlineContainer();

	// Otherwise, we open a new one.
	if (!inline_container)
	{
		const float line_height = element->GetLineHeight();
		const float scrollbar_width = (IsScrollContainer() ? element->GetElementScroll()->GetScrollbarSize(ElementScroll::VERTICAL) : 0.f);
		const float available_width = box.GetSize().x - scrollbar_width;

		auto inline_container_ptr = MakeUnique<InlineContainer>(this, available_width, line_height, wrap_content);
		inline_container = inline_container_ptr.get();
		block_boxes.push_back(std::move(inline_container_ptr));

		if (interrupted_line_box)
		{
			inline_container->AddChainedBox(std::move(interrupted_line_box));
			interrupted_line_box.reset();
		}
	}

	return inline_container;
}

const LayoutBox* BlockContainer::GetOpenLayoutBox() const
{
	if (!block_boxes.empty())
		return block_boxes.back().get();
	return nullptr;
}

bool BlockContainer::CloseOpenInlineContainer()
{
	if (InlineContainer* inline_container = GetOpenInlineContainer())
	{
		ResetInterruptedLineBox();
		return inline_container->Close(&interrupted_line_box);
	}

	return true;
}

void BlockContainer::ResetInterruptedLineBox()
{
	if (interrupted_line_box)
	{
		RMLUI_ERROR; // Internal error: Interrupted line box leaked.
		interrupted_line_box.reset();
	}
}

void BlockContainer::PlaceFloat(Element* element, float vertical_position, Vector2f visible_overflow_size)
{
	const Box& element_box = element->GetBox();

	const Vector2f border_size = element_box.GetSize(Box::BORDER);
	visible_overflow_size = Math::Max(border_size, visible_overflow_size);

	const Vector2f margin_top_left = {element_box.GetEdge(Box::MARGIN, Box::LEFT), element_box.GetEdge(Box::MARGIN, Box::TOP)};
	const Vector2f margin_bottom_right = {element_box.GetEdge(Box::MARGIN, Box::RIGHT), element_box.GetEdge(Box::MARGIN, Box::BOTTOM)};
	const Vector2f margin_size = border_size + margin_top_left + margin_bottom_right;

	Style::Float float_property = element->GetComputedValues().float_();
	Style::Clear clear_property = element->GetComputedValues().clear();

	float unused_box_width = 0.f;
	const Vector2f margin_position = space->NextFloatPosition(this, unused_box_width, vertical_position, margin_size, float_property, clear_property);
	const Vector2f border_position = margin_position + margin_top_left;

	space->PlaceFloat(float_property, margin_position, margin_size, border_position, visible_overflow_size);

	// Shift the offset into this container's space, which acts as the float element's containing block.
	element->SetOffset(border_position - position, GetElement());
}

bool BlockContainer::GetBaselineOfLastLine(float& out_baseline) const
{
	for (int i = (int)block_boxes.size() - 1; i >= 0; i--)
	{
		if (block_boxes[i]->GetBaselineOfLastLine(out_baseline))
			return true;
	}

	return false;
}

} // namespace Rml
