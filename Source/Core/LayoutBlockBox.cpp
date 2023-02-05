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

void ContainerBox::AddAbsoluteElement(Element* element, Vector2f static_position)
{
	absolute_elements.push_back(AbsoluteElement{element, static_position});
}

void ContainerBox::AddRelativeElement(Element* element)
{
	relative_elements.push_back(element);
}

void ContainerBox::ClosePositionedElements(const Box& box, Vector2f root_relative_position)
{
	if (!absolute_elements.empty())
	{
		// The size of the containing box, including the padding. This is used to resolve relative offsets.
		Vector2f containing_block = box.GetSize(Box::PADDING);

		for (size_t i = 0; i < absolute_elements.size(); i++)
		{
			Element* absolute_element = absolute_elements[i].element;
			Vector2f absolute_position = absolute_elements[i].position;
			absolute_position -= root_relative_position; // position - offset_root->GetPosition();

			// Lay out the element.
			LayoutEngine::FormatElement(absolute_element, containing_block);

			// Now that the element's box has been built, we can offset the position we determined was appropriate for
			// it by the element's margin. This is necessary because the coordinate system for the box begins at the
			// border, not the margin.
			absolute_position.x += absolute_element->GetBox().GetEdge(Box::MARGIN, Box::LEFT);
			absolute_position.y += absolute_element->GetBox().GetEdge(Box::MARGIN, Box::TOP);

			// Set the offset of the element; the element itself will take care of any RCSS-defined positional offsets.
			absolute_element->SetOffset(absolute_position, element);
		}

		absolute_elements.clear();
	}

	// Any relatively positioned elements that we act as containing block for may also need to be have their positions
	// updated to reflect changes to the size of this block box.
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

bool ContainerBox::SubmitBox(const Vector2f content_box, const Box& box, const float max_height)
{
	if (!element)
	{
		SetVisibleOverflowSize({});
		return true;
	}

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
		if (!CatchOverflow(content_box, box, max_height))
			return false;

		const Vector2f padding_top_left = {box.GetEdge(Box::PADDING, Box::LEFT), box.GetEdge(Box::PADDING, Box::TOP)};
		const Vector2f padding_bottom_right = {box.GetEdge(Box::PADDING, Box::RIGHT), box.GetEdge(Box::PADDING, Box::BOTTOM)};
		const Vector2f padding_size = box.GetSize() + padding_top_left + padding_bottom_right;

		const bool is_scroll_container = IsScrollContainer();
		const Vector2f scrollbar_size = {
			is_scroll_container ? element->GetElementScroll()->GetScrollbarSize(ElementScroll::VERTICAL) : 0.f,
			is_scroll_container ? element->GetElementScroll()->GetScrollbarSize(ElementScroll::HORIZONTAL) : 0.f,
		};
		const Vector2f scrollable_overflow_size = Math::Max(padding_size - scrollbar_size, padding_top_left + content_box);

		element->SetBox(box);
		element->SetScrollableOverflowRectangle(scrollable_overflow_size);

		const Vector2f border_size = padding_size + box.GetSizeAround(Box::BORDER, Box::BORDER);

		// Set the visible overflow size so that ancestors can catch any overflow produced by us. That is, hiding it or
		// providing a scrolling mechanism. If this box is a scroll container, we catch our own overflow here; then,
		// just use the normal margin box as that will effectively remove the overflow from our ancestor's perspective.
		if (is_scroll_container)
		{
			visible_overflow_size = border_size;

			// Format any scrollbars which were enabled on this element.
			element->GetElementScroll()->FormatScrollbars();
		}
		else
		{
			const Vector2f border_top_left = {box.GetEdge(Box::BORDER, Box::LEFT), box.GetEdge(Box::BORDER, Box::TOP)};
			visible_overflow_size = Math::Max(border_size, content_box + border_top_left + padding_top_left);
		}
	}

	SetVisibleOverflowSize(visible_overflow_size);

	return true;
}

BlockContainer::BlockContainer(BlockContainer* _parent, Element* _element, const Box& _box, float _min_height, float _max_height) :
	ContainerBox(Type::BlockContainer, _element), parent(_parent), box(_box), min_height(_min_height), max_height(_max_height)
{
	space = MakeUnique<LayoutBlockBoxSpace>(this);

	// Determine the offset parent for our children.
	if (parent && parent->offset_parent->GetElement() && (!element || element->GetPosition() == Style::Position::Static))
		offset_parent = parent->offset_parent;
	else
		// We are a positioned element (thereby acting as 
		offset_parent = this;

	if (element)
		wrap_content = (element->GetComputedValues().white_space() != Style::WhiteSpace::Nowrap);
}

BlockContainer::~BlockContainer() {}

BlockContainer::CloseResult BlockContainer::Close()
{
	// If the last child of this block box is an inline box, then we haven't closed it; close it now!
	CloseResult result = CloseInlineBlockBox();
	if (result != CloseResult::OK)
		return CloseResult::LayoutSelf;

	// Set this box's height, if necessary.
	if (box.GetSize().y < 0)
	{
		Vector2f content_area = box.GetSize();
		content_area.y = Math::Clamp(box_cursor, min_height, max_height);

		if (element)
			content_area.y = Math::Max(content_area.y, space->GetDimensions(LayoutFloatBoxEdge::Margin).y);

		box.SetContent(content_area);
	}

	// Check how big our floated area is.
	const Vector2f space_box = space->GetDimensions(LayoutFloatBoxEdge::Border);

	// Start with the inner content size, as set by the child blocks boxes or external formatting contexts.
	Vector2f content_box = Math::Max(inner_content_size, space_box);
	content_box.y = Math::Max(content_box.y, box_cursor);

	if (!SubmitBox(content_box, box, max_height))
	{
		ResetContents();
		return CloseResult::LayoutSelf;
	}

	// Increment the parent's cursor.
	if (parent)
	{
		// If this close fails, it means this block box has caused our parent block box to generate an automatic vertical scrollbar.
		const Vector2f margin_corner = {box.GetEdge(Box::MARGIN, Box::LEFT), box.GetEdge(Box::MARGIN, Box::TOP)};
		const Vector2f margin_position = position - margin_corner;
		if (!parent->CloseChildBox(this, margin_position, margin_corner, box.GetSize(Box::MARGIN)))
			return CloseResult::LayoutParent;
	}

	// If we represent a positioned element, then we can now (as we've been sized) format the absolutely positioned
	// elements that this container acts as a containing block for.
	ClosePositionedElements(box, position);

	if (element)
	{
		// Find the element baseline which is the distance from the margin bottom of the element to its baseline.
		float element_baseline = 0;

		// For inline-blocks with visible overflow, this is the baseline of the last line of the element (see CSS2 10.8.1).
		if (element->GetDisplay() == Style::Display::InlineBlock && !IsScrollContainer())
		{
			float baseline = 0;
			bool found_baseline = GetBaselineOfLastLine(baseline);

			// The retrieved baseline is the vertical distance from the top of our root space (the coordinate system of
			// our local block formatting context), convert it to the element's local coordinates.
			if (found_baseline)
			{
				const float bottom_position = position.y + box.GetSizeAcross(Box::VERTICAL, Box::BORDER) + box.GetEdge(Box::MARGIN, Box::BOTTOM);
				element_baseline = bottom_position - baseline;
			}
		}

		element->SetBaseline(element_baseline);
	}

	ResetInterruptedLineBox();

	return CloseResult::OK;
}

bool BlockContainer::CloseChildBox(LayoutBox* child, Vector2f child_position, Vector2f child_margin_corner, Vector2f child_size)
{
	child_position -= (box.GetPosition() + position);
	box_cursor = child_position.y + child_size.y;

	// Extend the inner content size. The vertical size can be larger than the box_cursor due to overflow.
	inner_content_size = Math::Max(inner_content_size, child_position + child_margin_corner + child->GetVisibleOverflowSize());

	const Vector2f content_size = Math::Max(Vector2f{box.GetSize().x, box_cursor}, inner_content_size);

	if (!CatchOverflow(content_size, box, max_height))
	{
		ResetContents();
		return false;
	}

	return true;
}

BlockContainer* BlockContainer::AddBlockBox(Element* child_element, const Box& box, float min_height, float max_height)
{
	if (!CloseOpenInlineContainer())
		return nullptr;

	auto child_container_ptr = MakeUnique<BlockContainer>(this, child_element, box, min_height, max_height);
	BlockContainer* child_container = child_container_ptr.get();

	// Determine the offset parent for the child element.
	BlockContainer* child_offset_parent = (offset_parent->GetElement() ? offset_parent : child_container);

	child_container->space->ImportSpace(*space);

	// Position ourselves within our containing block (if we have a valid offset parent).
	if (element)
	{
		if (child_offset_parent != child_container)
		{
			// Get the next position within our offset parent's containing block.
			child_container->position = NextBlockBoxPosition(box, child_element->GetComputedValues().clear());
			child_element->SetOffset(child_container->position - child_offset_parent->GetPosition(), child_offset_parent->GetElement());
		}
		else
			child_element->SetOffset(child_container->position, nullptr);
	}

	if (child_element)
	{
		child_container->ResetScrollbars(box);

		// Store relatively positioned elements with their containing block so that their offset can be updated after
		// their containing block has been sized.
		if (child_offset_parent != child_container && child_element->GetPosition() == Style::Position::Relative)
			child_offset_parent->AddRelativeElement(child_element);
	}

	block_boxes.push_back(std::move(child_container_ptr));

	return child_container;
}

LayoutBox* BlockContainer::AddBlockLevelBox(UniquePtr<LayoutBox> block_level_box_ptr, Element* child_element, const Box& box)
{
	RMLUI_ASSERT(box.GetSize().y >= 0.f); // Assumes child element already formatted and sized.

	if (!CloseOpenInlineContainer())
		return nullptr;

	Vector2f child_position = NextBlockBoxPosition(box, child_element->GetComputedValues().clear());

	BlockContainer* child_offset_parent = (offset_parent->GetElement() ? offset_parent : this); // TODO: Verify

	// Position ourselves within our containing block (if we have a valid offset parent).
	// TODO: Essentially the same as above in 'AddBlockBox'.
	if (element)
	{
		// Get the next position within our offset parent's containing block.
		child_element->SetOffset(child_position - child_offset_parent->GetPosition(), child_offset_parent->GetElement());
	}

	// TODO Basically, almost copy/paste from above.
	if (child_element)
	{
		// Store relatively positioned elements with their containing block so that their offset can be updated after
		// their containing block has been sized.
		if (child_element->GetPosition() == Style::Position::Relative)
			child_offset_parent->AddRelativeElement(child_element);
	}

	block_boxes.push_back(std::move(block_level_box_ptr));
	LayoutBox* block_level_box = block_boxes.back().get();

	const Vector2f margin_corner = {box.GetEdge(Box::MARGIN, Box::LEFT), box.GetEdge(Box::MARGIN, Box::TOP)};
	const Vector2f margin_position = child_position - margin_corner;
	if (!CloseChildBox(block_level_box, margin_position, margin_corner, box.GetSize(Box::MARGIN)))
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
		offset_parent->AddRelativeElement(element);

	return {inline_container, inline_box};
}

void BlockContainer::CloseInlineElement(InlineBoxHandle handle)
{
	// If the inline-level element did not generate an inline box, then there is no need to close anything.
	if (!handle.inline_box || !handle.inline_container)
		return;

	// Check that the handle's inline container is still the open box, otherwise it has been closed already possibly
	// by an intermediary block-level element. If we don't have an open inline container at all, open a new one,
	// even if the sole purpose of the new line is to close this inline element.
	InlineContainer* inline_container = EnsureOpenInlineContainer();
	if (inline_container != handle.inline_container)
	{
		// TODO: Not needed once everything works as intended.
		Log::Message(Log::LT_INFO, "Inline element was split across a block-level element.");
	}

	inline_container->CloseInlineElement(handle.inline_box);
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

void BlockContainer::AddFloatElement(Element* element)
{
	if (InlineContainer* inline_container = GetOpenInlineContainer())
	{
		// Try to add the float to our inline container, placing it next to any open line if possible. Otherwise, queue it for later.
		if (!queued_float_elements.empty() || !inline_container->PlaceFloatElement(element, space.get()))
		{
			queued_float_elements.push_back(element);
		}
	}
	else
	{
		// Nope ... just place it!
		PlaceFloat(element);
	}

	if (element->GetPosition() == Style::Position::Relative)
		offset_parent->AddRelativeElement(element);
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

BlockContainer* BlockContainer::GetAbsolutePositioningContainingBlock()
{
	BlockContainer* absolute_parent = this;
	while (absolute_parent != absolute_parent->offset_parent)
		absolute_parent = absolute_parent->parent;
	return absolute_parent;
}

Vector2f BlockContainer::NextBoxPosition(float top_margin, Style::Clear clear_property) const
{
	// If our element is establishing a new offset hierarchy, then any children of ours don't inherit our offset.
	Vector2f box_position = position + box.GetPosition();
	box_position.y += box_cursor;

	float clear_margin = space->DetermineClearPosition(box_position.y + top_margin, clear_property) - (box_position.y + top_margin);
	if (clear_margin > 0)
		box_position.y += clear_margin;
	else if (const LayoutBox* block_box = GetOpenLayoutBox())
	{
		// Check for a collapsing vertical margin.
		if (const Box* open_box = block_box->GetBoxPtr())
		{
			const float bottom_margin = open_box->GetEdge(Box::MARGIN, Box::BOTTOM);

			const int num_negative_margins = int(top_margin < 0.f) + int(bottom_margin < 0.f);
			switch (num_negative_margins)
			{
			case 0:
				// Use the largest margin by subtracting the smallest margin.
				box_position.y -= Math::Min(top_margin, bottom_margin);
				break;
			case 1:
				// Use the sum of the positive and negative margin, no special behavior needed here.
				break;
			case 2:
				// Use the most negative margin by subtracting the least negative margin.
				box_position.y -= Math::Max(top_margin, bottom_margin);
				break;
			}
		}
	}

	return box_position;
}

Vector2f BlockContainer::NextBlockBoxPosition(const Box& box, Style::Clear clear_property) const
{
	Vector2f box_position = NextBoxPosition(box.GetEdge(Box::MARGIN, Box::TOP), clear_property);
	box_position.x += box.GetEdge(Box::MARGIN, Box::LEFT);
	box_position.y += box.GetEdge(Box::MARGIN, Box::TOP);
	return box_position;
}

void BlockContainer::PlaceQueuedFloats(float vertical_offset)
{
	if (!queued_float_elements.empty())
	{
		for (Element* float_element : queued_float_elements)
			PlaceFloat(float_element, vertical_offset);

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
	if (element)
	{
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
	}
	else
	{
		get_content_width_from_children();
	}

	// Can add the dimensions of floating elements here if we want to support that.

	return content_width;
}

void BlockContainer::ExtendInnerContentSize(Vector2f _inner_content_size)
{
	inner_content_size.x = Math::Max(inner_content_size.x, _inner_content_size.x);
	inner_content_size.y = Math::Max(inner_content_size.y, _inner_content_size.y);
}

Element* BlockContainer::GetElement() const
{
	return element;
}

const BlockContainer* BlockContainer::GetParent() const
{
	return parent;
}

const LayoutBlockBoxSpace* BlockContainer::GetBlockBoxSpace() const
{
	return space.get();
}

Vector2f BlockContainer::GetPosition() const
{
	return position;
}

const BlockContainer* BlockContainer::GetOffsetParent() const
{
	return offset_parent;
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
	space = MakeUnique<LayoutBlockBoxSpace>(this);

	box_cursor = 0;
	interrupted_line_box.reset();

	inner_content_size = Vector2f(0);
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
	// TODO: What if we already have an open block container, do we need to close it first?
	// RMLUI_ASSERT(!GetOpenBlockContainer());

	// First check to see if we already have an open inline container.
	InlineContainer* inline_container = GetOpenInlineContainer();

	// Otherwise, we open a new one.
	if (!inline_container)
	{
		const float line_height = element->GetLineHeight();
		auto inline_container_ptr = MakeUnique<InlineContainer>(this, line_height, wrap_content);
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

const BlockContainer* BlockContainer::GetOpenBlockContainer() const
{
	if (!block_boxes.empty() && block_boxes.back()->GetType() == Type::BlockContainer)
		return static_cast<BlockContainer*>(block_boxes.back().get());
	return nullptr;
}

const LayoutBox* BlockContainer::GetOpenLayoutBox() const
{
	if (!block_boxes.empty())
		return block_boxes.back().get();
	return nullptr;
}

BlockContainer::CloseResult BlockContainer::CloseInlineBlockBox()
{
	if (InlineContainer* inline_container = GetOpenInlineContainer())
	{
		ResetInterruptedLineBox();
		return inline_container->Close(&interrupted_line_box);
	}

	return CloseResult::OK;
}

bool BlockContainer::CloseOpenInlineContainer()
{
	RMLUI_ZoneScoped;

	if (InlineContainer* inline_container = GetOpenInlineContainer())
	{
		UniquePtr<LayoutLineBox> open_line_box;

		if (inline_container->Close(&open_line_box) != CloseResult::OK)
			return false;

		if (open_line_box)
		{
			// TODO: Is comment still valid?
			// There's an open inline box chain, which means this block element is parented to it. The chain needs to
			// be positioned (if it hasn't already), closed and duplicated after this block box closes. Also, this
			// block needs to be aware of its parentage, so it can correctly compute its relative position. First of
			// all, we need to close the inline box; this will position the last line if necessary, but it will also
			// create a new line in the inline block box; we want this line to be in an inline box after our block
			// element.

			ResetInterruptedLineBox();

			interrupted_line_box = std::move(open_line_box);
		}
	}

	return true;
}

void BlockContainer::ResetInterruptedLineBox()
{
	if (interrupted_line_box)
	{
		Log::Message(Log::LT_WARNING, "Interrupted line box leaked.");
		interrupted_line_box.reset();
	}
}

void BlockContainer::PlaceFloat(Element* element, float offset)
{
	const Vector2f box_position = NextBoxPosition();
	space->PlaceFloat(element, box_position.y + offset);
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
