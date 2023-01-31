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

BlockContainer::BlockContainer(BlockContainer* _parent, Element* _element, const Box& _box, float _min_height, float _max_height) :
	BlockLevelBox(Type::BlockContainer), position(0), box(_box), min_height(_min_height), max_height(_max_height)
{
	RMLUI_ZoneScoped;

	space = MakeUnique<LayoutBlockBoxSpace>(this);

	parent = _parent;
	element = _element;

	box_cursor = 0;
	vertical_overflow = false;

	// Get our offset root from our parent, if it has one; otherwise, our element is the offset parent.
	if (parent && parent->offset_root->GetElement())
		offset_root = parent->offset_root;
	else
		offset_root = this;

	// Determine the offset parent for this element.
	BlockContainer* self_offset_parent;
	if (parent && parent->offset_parent->GetElement())
		self_offset_parent = parent->offset_parent;
	else
		self_offset_parent = this;

	// Determine the offset parent for our children.
	if (parent && parent->offset_parent->GetElement() && (!element || element->GetPosition() == Style::Position::Static))
		offset_parent = parent->offset_parent;
	else
		offset_parent = this;

	// Build the box for our element, and position it if we can.
	if (parent)
	{
		space->ImportSpace(*parent->space);

		// Position ourselves within our containing block (if we have a valid offset parent).
		if (parent->GetElement())
		{
			if (self_offset_parent != this)
			{
				// Get the next position within our offset parent's containing block.
				position = parent->NextBlockBoxPosition(box, element->GetComputedValues().clear());
				element->SetOffset(position - (self_offset_parent->GetPosition() - offset_root->GetPosition()), self_offset_parent->GetElement());
			}
			else
				element->SetOffset(position, nullptr);
		}
	}

	if (element)
	{
		const auto& computed = element->GetComputedValues();
		wrap_content = computed.white_space() != Style::WhiteSpace::Nowrap;

		// Determine if this element should have scrollbars or not, and create them if so.
		overflow_x_property = computed.overflow_x();
		overflow_y_property = computed.overflow_y();

		if (overflow_x_property == Style::Overflow::Scroll)
			element->GetElementScroll()->EnableScrollbar(ElementScroll::HORIZONTAL, box.GetSize(Box::PADDING).x);
		else
			element->GetElementScroll()->DisableScrollbar(ElementScroll::HORIZONTAL);

		if (overflow_y_property == Style::Overflow::Scroll)
			element->GetElementScroll()->EnableScrollbar(ElementScroll::VERTICAL, box.GetSize(Box::PADDING).x);
		else
			element->GetElementScroll()->DisableScrollbar(ElementScroll::VERTICAL);

		// Store relatively positioned elements with their containing block so that their offset can be updated after their containing block has been
		// sized.
		if (self_offset_parent != this && computed.position() == Style::Position::Relative)
			self_offset_parent->relative_elements.push_back(element);
	}
	else
	{
		wrap_content = true;
		overflow_x_property = Style::Overflow::Visible;
		overflow_y_property = Style::Overflow::Visible;
	}
}

BlockContainer::~BlockContainer() {}

BlockContainer::CloseResult BlockContainer::Close()
{
	using Style::Overflow;

	// If the last child of this block box is an inline box, then we haven't closed it; close it now!
	CloseResult result = CloseInlineBlockBox();
	if (result != CloseResult::OK)
		return CloseResult::LayoutSelf;

	// Set this box's height, if necessary.
	if (box.GetSize(Box::CONTENT).y < 0)
	{
		Vector2f content_area = box.GetSize();
		content_area.y = Math::Clamp(box_cursor, min_height, max_height);

		if (element)
			content_area.y = Math::Max(content_area.y, space->GetDimensions(LayoutFloatBoxEdge::Margin).y);

		box.SetContent(content_area);
	}

	// TODO: Properly compute the visible overflow size / scrollable overflow rectangle.
	//       https://www.w3.org/TR/css-overflow-3/#scrollable
	//
	Vector2f visible_overflow_size;
	const bool is_scroll_container = (overflow_x_property != Overflow::Visible || overflow_y_property != Overflow::Visible);

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

		// Start with the inner content size, as set by the child blocks boxes or external formatting contexts.
		Vector2f content_box = inner_content_size;

		// Check how big our floated area is.
		const Vector2f space_box = space->GetDimensions(LayoutFloatBoxEdge::Border);
		content_box.x = Math::Max(content_box.x, space_box.x);

		const Vector2f padding_top_left = {box.GetEdge(Box::PADDING, Box::LEFT), box.GetEdge(Box::PADDING, Box::TOP)};
		const Vector2f padding_bottom_right = {box.GetEdge(Box::PADDING, Box::RIGHT), box.GetEdge(Box::PADDING, Box::BOTTOM)};
		const Vector2f padding_size = box.GetSize() + padding_top_left + padding_bottom_right;

		// If our content is larger than our window, we can enable the horizontal scrollbar if we're set to
		// auto-scrollbars. If we're set to always use scrollbars, then the horiontal scrollbar will already have been
		// enabled in the constructor.
		if (content_box.x > box.GetSize().x + padding_bottom_right.x + 0.5f)
		{
			if (overflow_x_property == Overflow::Auto)
			{
				element->GetElementScroll()->EnableScrollbar(ElementScroll::HORIZONTAL, padding_size.x);

				if (!CatchVerticalOverflow())
					return CloseResult::LayoutSelf;
			}
		}

		content_box.y = Math::Max(content_box.y, box_cursor);
		content_box.y = Math::Max(content_box.y, space_box.y);
		if (!CatchVerticalOverflow(content_box.y))
			return CloseResult::LayoutSelf;

		const Vector2f scrollbar_size = {
			is_scroll_container ? element->GetElementScroll()->GetScrollbarSize(ElementScroll::VERTICAL) : 0.f,
			is_scroll_container ? element->GetElementScroll()->GetScrollbarSize(ElementScroll::HORIZONTAL) : 0.f,
		};
		const Vector2f scrollable_overflow_size = Math::Max(padding_size - scrollbar_size, padding_top_left + content_box);

		element->SetBox(box);
		element->SetScrollableOverflowRectangle(scrollable_overflow_size);

		const Vector2f margin_size = padding_size + box.GetSizeAround(Box::MARGIN, Box::MARGIN);

		// Set the visible overflow size so that ancestors can catch any overflow produced by us. That is, hiding it or
		// providing a scrolling mechanism. If this box is a scroll container, we catch our own overflow here; then,
		// just use the normal margin box as that will effectively remove the overflow from our ancestor's perspective.
		if (is_scroll_container)
		{
			visible_overflow_size = margin_size;

			// Format any scrollbars which were enabled on this element.
			element->GetElementScroll()->FormatScrollbars();
		}
		else
		{
			const Vector2f top_left_edges = {
				box.GetCumulativeEdge(Box::PADDING, Box::LEFT),
				box.GetCumulativeEdge(Box::PADDING, Box::TOP),
			};
			visible_overflow_size = Math::Max(margin_size, content_box + top_left_edges);
		}
	}

	SetVisibleOverflowSize(visible_overflow_size);

	// Increment the parent's cursor.
	if (parent)
	{
		// If this close fails, it means this block box has caused our parent block box to generate an automatic vertical scrollbar.
		const float self_position_top = position.y - box.GetEdge(Box::MARGIN, Box::TOP);
		if (!parent->CloseChildBox(this, self_position_top, box.GetSize(Box::MARGIN).y))
			return CloseResult::LayoutParent;
	}

	// If we represent a positioned element, then we can now (as we've been sized) act as the containing block for all
	// the absolutely-positioned elements of our descendants.
	CloseAbsoluteElements();

	if (element)
	{
		// Any relatively positioned elements that we act as containing block for may also need to be have their positions
		// updated to reflect changes to the size of this block box.
		for (Element* child : relative_elements)
			child->UpdateOffset();

		// Set the baseline for inline-block elements to the baseline of the last line of the element.
		// This is a special rule for inline-blocks (see CSS 2.1 Sec. 10.8.1).
		if (element->GetDisplay() == Style::Display::InlineBlock)
		{
			// Baseline relative to the border box of our children's offset parent.
			float baseline = 0;
			bool found_baseline = GetBaselineOfLastLine(baseline);

			if (found_baseline)
			{
				// It is assumed here that we are the offset parent of our children. If this is not the case, we might need
				// to take into account the offset between this box's position and our children's offset parent.
				RMLUI_ASSERT(this == offset_parent);

				// Set the element baseline which is the distance from the margin bottom of the element to its baseline.
				float element_baseline = 0;
				if (!is_scroll_container)
					element_baseline = box.GetSizeAcross(Box::VERTICAL, Box::BORDER) + box.GetEdge(Box::MARGIN, Box::BOTTOM) - baseline;

				element->SetBaseline(element_baseline);
			}
		}
		else
			element->SetBaseline(0);
	}

	ResetInterruptedLineBox();

	return CloseResult::OK;
}

bool BlockContainer::CloseChildBox(BlockLevelBox* child, float child_position_top, float child_size_y)
{
	const float child_position_y = child_position_top - (box.GetPosition().y + position.y);
	box_cursor = child_position_y + child_size_y;

	// Extend the inner content size. The vertical size can be larger than the box_cursor due to overflow.
	inner_content_size.x = Math::Max(inner_content_size.x, child->GetVisibleOverflowSize().x);
	inner_content_size.y = Math::Max(inner_content_size.y, child_position_y + child->GetVisibleOverflowSize().y);

	return CatchVerticalOverflow();
}

BlockContainer* BlockContainer::AddBlockElement(Element* element, const Box& box, float min_height, float max_height)
{
	RMLUI_ZoneScoped;

	// Close the inline container if there is one open.
	if (InlineContainer* inline_container = GetOpenInlineContainer())
	{
		UniquePtr<LayoutLineBox> open_line_box;

		if (inline_container->Close(&open_line_box) != CloseResult::OK)
			return nullptr;

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

	block_boxes.push_back(MakeUnique<BlockContainer>(this, element, box, min_height, max_height));
	return static_cast<BlockContainer*>(block_boxes.back().get());
}

BlockContainer::InlineBoxHandle BlockContainer::AddInlineElement(Element* element, const Box& box)
{
	RMLUI_ZoneScoped;

	// Inline-level elements need to be added to an inline container, open one if needed.
	InlineContainer* inline_container = EnsureOpenInlineContainer();

	InlineBox* inline_box = inline_container->AddInlineElement(element, box);

	if (element->GetPosition() == Style::Position::Relative)
		offset_parent->relative_elements.push_back(element);

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
		offset_parent->relative_elements.push_back(element);
}

void BlockContainer::AddAbsoluteElement(Element* element)
{
	AbsoluteElement absolute_element;
	absolute_element.element = element;
	absolute_element.position = NextBoxPosition();

	// Position the box as if it had static position (10.6.4). If the element is inline-level, position it on the open
	// line if we have one. Otherwise, block-level elements are positioned on a hypothetical next line.
	if (InlineContainer* inline_container = GetOpenInlineContainer())
	{
		const Style::Display display = element->GetDisplay();
		const bool inline_level_element = (display == Style::Display::Inline || display == Style::Display::InlineBlock);
		absolute_element.position += inline_container->GetStaticPositionEstimate(inline_level_element);
	}

	// Find the positioned parent for this element.
	BlockContainer* absolute_parent = this;
	while (absolute_parent != absolute_parent->offset_parent)
		absolute_parent = absolute_parent->parent;

	absolute_parent->absolute_elements.push_back(absolute_element);
}

void BlockContainer::CloseAbsoluteElements()
{
	if (!absolute_elements.empty())
	{
		// The size of the containing box, including the padding. This is used to resolve relative offsets.
		Vector2f containing_block = GetBox().GetSize(Box::PADDING);

		for (size_t i = 0; i < absolute_elements.size(); i++)
		{
			Element* absolute_element = absolute_elements[i].element;
			Vector2f absolute_position = absolute_elements[i].position;
			absolute_position -= position - offset_root->GetPosition();

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
}


void BlockContainer::AddRelativeElements(ElementList&& elements)
{
	if (relative_elements.empty())
		relative_elements = std::move(elements);
	else
		relative_elements.insert(relative_elements.end(), elements.begin(), elements.end());
	elements.clear();
}

Vector2f BlockContainer::NextBoxPosition(float top_margin, Style::Clear clear_property) const
{
	// If our element is establishing a new offset hierarchy, then any children of ours don't inherit our offset.
	Vector2f box_position = position + box.GetPosition();
	box_position.y += box_cursor;

	float clear_margin = space->DetermineClearPosition(box_position.y + top_margin, clear_property) - (box_position.y + top_margin);
	if (clear_margin > 0)
		box_position.y += clear_margin;
	else
	{
		// Check for a collapsing vertical margin.
		if (const BlockContainer* block_box = GetOpenBlockContainer())
		{
			const float bottom_margin = block_box->GetBox().GetEdge(Box::MARGIN, Box::BOTTOM);

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

const BlockContainer* BlockContainer::GetOffsetRoot() const
{
	return offset_root;
}

Box& BlockContainer::GetBox()
{
	return box;
}

const Box& BlockContainer::GetBox() const
{
	return box;
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

BlockContainer::CloseResult BlockContainer::CloseInlineBlockBox()
{
	if (InlineContainer* inline_container = GetOpenInlineContainer())
	{
		ResetInterruptedLineBox();
		return inline_container->Close(&interrupted_line_box);
	}

	return CloseResult::OK;
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

bool BlockContainer::CatchVerticalOverflow(float cursor)
{
	if (cursor == -1)
		cursor = Math::Max(box_cursor, inner_content_size.y);

	float box_height = box.GetSize().y;
	if (box_height < 0)
		box_height = max_height;

	// If we're auto-scrolling and our height is fixed, we have to check if this box has exceeded our client height.
	if (!vertical_overflow && box_height >= 0 && overflow_y_property == Style::Overflow::Auto)
	{
		if (cursor > box_height - element->GetElementScroll()->GetScrollbarSize(ElementScroll::HORIZONTAL) + 0.5f)
		{
			RMLUI_ZoneScopedC(0xDD3322);
			vertical_overflow = true;
			element->GetElementScroll()->EnableScrollbar(ElementScroll::VERTICAL, box.GetSize(Box::PADDING).x);

			block_boxes.clear();

			space = MakeUnique<LayoutBlockBoxSpace>(this);

			box_cursor = 0;
			interrupted_line_box.reset();

			inner_content_size = Vector2f(0);

			return false;
		}
	}

	return true;
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
